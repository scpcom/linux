/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */


#include "types.h"
#include "checksum.h"
#include "module_tunnel.h"
#include "module_hidrv.h"
#include "module_timer.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "fpp.h"
#include "gemac.h"
#include "fe.h"
#include "layer2.h"
#include "module_ipsec.h"
#include "module_bridge.h"
#include "module_pppoe.h"
#include "module_qm.h"
#include "control_common.h"

#if defined(COMCERTO_2000)
void tnl_update(int tunnel_index);
struct dlist_head hw_gre_tunnel_removal_list;
struct dlist_head hw_gre_tunnel_active_list[NUM_GRE_TUNNEL_ENTRIES];
void tnl_update_gre(PTnlEntry pTunnelEntry);
extern TIMER_ENTRY gre_tnl_timer;
#endif

U32 tunnel_created_dmem = 0;
U32 tunnel_created_gre = 0;

/**
 * M_tnl_get_by_name
 *
 *
 *
 */
static PTnlEntry M_tnl_get_by_name(U8 *tnl_name)
{
	PTnlEntry pTunnelEntry;
	struct slist_entry *entry;
	int i;

	if (tnl_name)
	{
		for (i = 0; i < TNL_MAX_TUNNEL_DMEM; i++)
		{
			pTunnelEntry = &gTNLCtx.tunnel_table[i];
			if (pTunnelEntry->state != TNL_STATE_FREE &&
					!strcmp((const char*)tnl_name, get_onif_name(pTunnelEntry->itf.index)))
				return pTunnelEntry;
		}

		for (i = 0; i < NUM_GRE_TUNNEL_ENTRIES; i++)
		{
			slist_for_each(pTunnelEntry, entry, &gre_tunnel_cache[i], list) {
				if(!strcmp((const char*)tnl_name, get_onif_name(pTunnelEntry->itf.index)))
					return pTunnelEntry;
			}
		}
	}
	return NULL;
}


/**
 * M_tnl_get_free_entry
 *
 *
 *
 */
static int M_tnl_get_free_entry(void)
{
	PTnlEntry pTunnelEntry;
	int i;

	for (i = 0; i < TNL_MAX_TUNNEL_DMEM; i++)
	{
		pTunnelEntry = &gTNLCtx.tunnel_table[i];
		if (pTunnelEntry->state == TNL_STATE_FREE)
			return i;
	}

	return -1;
}

static void tnl_update_gre_cache(u32 host_addr, int hash)
{
	u32 *cache_addr;
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	cache_addr = (u32*)&gre_tunnel_cache[hash];
	pe_sync_stop(ctrl, CLASS_MASK);
	class_bus_write(host_addr, virt_to_class_pe_lmem(cache_addr), 4);
	pe_start(ctrl, CLASS_MASK);
}



/** Processes hardware gre tunnel delayed removal list.
 * Free all hardware gre tunnel entries in the removal list that have reached their removal time.
 *
 *
 */
static void gre_tunnel_delayed_remove(void)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct dlist_head *entry;
	PHw_TnlEntry_gre phw_tnl;

	dlist_for_each_safe(phw_tnl, entry, &hw_gre_tunnel_removal_list, list)
	{
		if (!time_after(jiffies, phw_tnl->removal_time))
			continue;

		dlist_remove(&phw_tnl->list);

		/* Free the hardware entry */
		dma_pool_free(ctrl->dma_pool_512, phw_tnl, be32_to_cpu(phw_tnl->dma_addr));

	}
}


/**
 * M_tnl_get
 *
 *
 *
 */

static int M_tnl_get(int tnl_is_gre, PTnlEntry *pTunnelEntry)
{

	int tunnel_index;
	int rc = 0;
	if(!tnl_is_gre)
	{
		/* Fill in tunnel entry */
		tunnel_index = M_tnl_get_free_entry();
		if (tunnel_index < 0)
		{
			rc = ERR_TNL_NO_FREE_ENTRY;
			goto out;
		}
		*pTunnelEntry = &gTNLCtx.tunnel_table[tunnel_index];
		(*pTunnelEntry)->tunnel_index = tunnel_index;
	}
	else
	{
		/* This is a GRE tunnel Allocate memory from DDR */
		*pTunnelEntry = pfe_kzalloc(sizeof(Hw_TnlEntry_gre), GFP_KERNEL);
		if(!*pTunnelEntry)
			rc = ERR_NOT_ENOUGH_MEMORY;
	}
out:
	return rc;
}

/**
 * M_tnl_add_gre
 *
 *
 */
int M_tnl_add_gre(PTnlEntry pTunnelEntry, int hash)
{
	PHw_TnlEntry_gre phw_tnl, phw_tnl_first =NULL;
	dma_addr_t dma_addr;
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int rc = 0;

	/* Create HW entry and add SW and HW entries to the HASH */
	phw_tnl = dma_pool_alloc(ctrl->dma_pool_512, GFP_ATOMIC, &dma_addr);
	if(!phw_tnl)
	{
		printk(KERN_ERR "%s: dma alloc failed\n", __func__);
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto out;
	}
	memset(phw_tnl, 0, sizeof(*phw_tnl));
	phw_tnl->dma_addr = cpu_to_be32(dma_addr);
	/* Link hardware entry to the software entry */
	pTunnelEntry->hw_tnl_entry = phw_tnl;

	/* add hw entry to active list and update next pointer */
	if(!dlist_empty(&hw_gre_tunnel_active_list[hash]))
	{
		/* list is not empty, and we'll be added at head, so current first will become our next pointer */
		phw_tnl_first = container_of(dlist_first(&hw_gre_tunnel_active_list[hash]), typeof( struct _tHw_TnlEntry_gre ), list);
		hw_entry_set_field(&phw_tnl->next, hw_entry_get_field(&phw_tnl_first->dma_addr));
	}
	else
	{
		/* entry is empty, so we'll be the first and only one entry */
		hw_entry_set_field(&phw_tnl->next, 0);
	}
	dlist_add(&hw_gre_tunnel_active_list[hash], &phw_tnl->list);
	/* Add Software entry to the GRE Local HASH */
	slist_add(&gre_tunnel_cache[hash], &pTunnelEntry->list);

	/* Add Hardware entry to the GRE HW HASH in PE-LMEM */
	tnl_update_gre_cache(hw_entry_get_field(&phw_tnl->dma_addr), hash);

out:
	return rc;
}




/**
 * M_tnl_delete
 *
 *
 *
 */
static BOOL M_tnl_delete(PTnlEntry pTunnelEntry)
{
	int tunnel_index = pTunnelEntry->tunnel_index;
	int hash = 0;

	PHw_TnlEntry_gre phw_tnl, phw_tnl_prev;
	if (pTunnelEntry->pRtEntry)
		L2_route_put(pTunnelEntry->pRtEntry);

	pTunnelEntry->state = TNL_STATE_FREE;
#if defined(COMCERTO_2000)
	if(pTunnelEntry->mode == TNL_MODE_GRE_IPV6)
	{
		hash = HASH_GRE_TNL(pTunnelEntry->remote);
		phw_tnl = pTunnelEntry->hw_tnl_entry;
		if(phw_tnl) /* If phw_tnl is NULL, we're yet to allocate hardware entry and add the the tunnel to the lists */
		{
			/* Detatch HW tnl entry from SW tunnel entry */
			pTunnelEntry->hw_tnl_entry = NULL;

			/* if the removed entry is first in hash slot then only PE dmem hash need to be updated */
			if (&phw_tnl->list == dlist_first(&hw_gre_tunnel_active_list[hash]))
			{
				tnl_update_gre_cache(hw_entry_get_field(&phw_tnl->next), hash);
			}
			else
			{
				phw_tnl_prev = container_of(phw_tnl->list.prev, typeof(struct _tHw_TnlEntry_gre), list);
				hw_entry_set_field(&phw_tnl_prev->next, hw_entry_get_field(&phw_tnl->next));
			}
			/* Unlink from the hardware list */
			dlist_remove(&phw_tnl->list);

			/* Schedule to remove this entry a little later, to ensure there is no invalid access */
			phw_tnl->removal_time = jiffies + 2;
			dlist_add(&hw_gre_tunnel_removal_list, &phw_tnl->list);

			/* Unlink from software list */
			slist_remove(&gre_tunnel_cache[hash], &pTunnelEntry->list);
		}
		/* Free the software entry */
		pfe_kfree(pTunnelEntry);
	}
	else
	{
		memset(pTunnelEntry, 0 , sizeof(TnlEntry));
		tnl_update(tunnel_index);
	}
#endif
	return TRUE;
}


/**
 * M_tnl_build_header
 *
 *
 *
 */
static void M_tnl_build_header(PTnlEntry pTunnelEntry)
{
	ipv6_hdr_t ip6_hdr;
	ipv4_hdr_t ip4_hdr;

	switch (pTunnelEntry->mode)
	{
		/* EtherIP over IPv6 case : MAC|IPV6|ETHIP|MAC|IPV4 */
		/* Here IPV6|ETHIP part is pre-built            */

		case TNL_MODE_ETHERIPV6:

			/* add IPv6 header */
			SFL_memcpy((U8*)ip6_hdr.DestinationAddress, (U8*)pTunnelEntry->remote, IPV6_ADDRESS_LENGTH);
			SFL_memcpy((U8*)ip6_hdr.SourceAddress, (U8*)pTunnelEntry->local, IPV6_ADDRESS_LENGTH);
			IPV6_SET_VER_TC_FL(&ip6_hdr, pTunnelEntry->fl);
			ip6_hdr.HopLimit = pTunnelEntry->hlim;
			ip6_hdr.TotalLength = 0; //to be computed for each packet
			ip6_hdr.NextHeader = IPV6_ETHERIP;

			pTunnelEntry->header_size = sizeof(ipv6_hdr_t);
			SFL_memcpy(pTunnelEntry->header, (U8*)&ip6_hdr, pTunnelEntry->header_size);

			/* add EtherIP header */
			*(U16*)(pTunnelEntry->header + pTunnelEntry->header_size) = htons(TNL_ETHERIP_VERSION);
			pTunnelEntry->header_size += TNL_ETHERIP_HDR_LEN;
			break;

		/* EtherIP over IPv4 case : MAC|IPV4|ETHIP|MAC|IPV4 */
		/* Here IPV4|ETHIP part is pre-built            */

		case TNL_MODE_ETHERIPV4:
			/* add IPv4 header */
			ip4_hdr.SourceAddress = pTunnelEntry->local[0];
			ip4_hdr.DestinationAddress = pTunnelEntry->remote[0];
			ip4_hdr.Version_IHL = 0x45;
			ip4_hdr.Protocol = IPPROTOCOL_ETHERIP;
			ip4_hdr.TypeOfService = pTunnelEntry->fl & 0xFF;
			ip4_hdr.TotalLength = 0; //to be computed for each packet
			ip4_hdr.TTL = pTunnelEntry->hlim;
			ip4_hdr.Identification = 0;
			ip4_hdr.HeaderChksum = 0; //to be computed
			ip4_hdr.Flags_FragmentOffset = 0;

			pTunnelEntry->header_size = sizeof(ipv4_hdr_t);
			SFL_memcpy(pTunnelEntry->header, (U8*)&ip4_hdr, pTunnelEntry->header_size);

			/* add EtherIP header */
			*(U16*)(pTunnelEntry->header + pTunnelEntry->header_size) = htons(TNL_ETHERIP_VERSION);
			pTunnelEntry->header_size += TNL_ETHERIP_HDR_LEN;
			break;


		/* 6o4 case : MAC|IPV4|IPV6 		*/
		/* Here IPV4 part is pre-built	*/

		case TNL_MODE_6O4:
			ip4_hdr.SourceAddress = pTunnelEntry->local[0];
			ip4_hdr.DestinationAddress = pTunnelEntry->remote[0];
			ip4_hdr.Version_IHL = 0x45;
			ip4_hdr.Protocol = IPPROTOCOL_IPV6;
			ip4_hdr.TypeOfService = pTunnelEntry->fl & 0xFF;
			ip4_hdr.TotalLength = 0; //to be computed for each packet
			ip4_hdr.TTL = pTunnelEntry->hlim;
			ip4_hdr.Identification = 0;
			ip4_hdr.HeaderChksum = 0; //to be computed
			ip4_hdr.Flags_FragmentOffset = 0;

			pTunnelEntry->header_size = sizeof(ipv4_hdr_t);
			SFL_memcpy(pTunnelEntry->header, (U8*)&ip4_hdr, pTunnelEntry->header_size);
			break;

                /* 4o6 case : MAC|IPV6|IPV4             */
                /* Here IPV6 part is pre-built  */


                case TNL_MODE_4O6:

                        SFL_memcpy((U8*)ip6_hdr.DestinationAddress, (U8*)pTunnelEntry->remote, IPV6_ADDRESS_LENGTH);
                        SFL_memcpy((U8*)ip6_hdr.SourceAddress, (U8*)pTunnelEntry->local, IPV6_ADDRESS_LENGTH);
			IPV6_SET_VER_TC_FL(&ip6_hdr, pTunnelEntry->fl);
                        ip6_hdr.HopLimit = pTunnelEntry->hlim;
                        ip6_hdr.TotalLength = 0; //to be computed for each packet
                        ip6_hdr.NextHeader = IPPROTOCOL_IPIP;

                        pTunnelEntry->header_size = sizeof(ipv6_hdr_t);
                        SFL_memcpy(pTunnelEntry->header, (U8*)&ip6_hdr, pTunnelEntry->header_size);

                        break;

		case TNL_MODE_GRE_IPV6:

			/* add IPv6 header */
			memset(&ip6_hdr, 0, sizeof(ip6_hdr));
			SFL_memcpy((U8*)ip6_hdr.DestinationAddress, (U8*)pTunnelEntry->remote, IPV6_ADDRESS_LENGTH);
			SFL_memcpy((U8*)ip6_hdr.SourceAddress, (U8*)pTunnelEntry->local, IPV6_ADDRESS_LENGTH);
			IPV6_SET_VER_TC_FL(&ip6_hdr, pTunnelEntry->fl);
			ip6_hdr.HopLimit = pTunnelEntry->hlim;
			//ip6_hdr.TotalLength = 0; //to be computed for each packet
			ip6_hdr.NextHeader = IPV6_GRE;

			pTunnelEntry->header_size = sizeof(ipv6_hdr_t);
			SFL_memcpy(pTunnelEntry->header, (U8*)&ip6_hdr, pTunnelEntry->header_size);

			/* add GRE header */
			*(U32*)(pTunnelEntry->header + pTunnelEntry->header_size) = htonl(TNL_GRE_HEADER);
			pTunnelEntry->header_size += TNL_GRE_HDRSIZE;
			break;

		default:
			break;
	}
}


/**
 * TNL_handle_CREATE
 *
 *
 */
static int TNL_handle_CREATE(U16 *p, U16 Length)
{
	TNLCommand_create cmd;
	PTnlEntry pTunnelEntry;
	int hash = 0;
	int rc;

	/* Check length */
	if (Length != sizeof(TNLCommand_create))
	{
		rc = ERR_WRONG_COMMAND_SIZE;
		goto err0;
	}


	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(TNLCommand_create));

	if (( (cmd.mode == TNL_MODE_GRE_IPV6) && (tunnel_created_gre >= GRE_MAX_TUNNELS)) ||
		((cmd.mode != TNL_MODE_GRE_IPV6) && (tunnel_created_dmem >= TNL_MAX_TUNNEL_DMEM)))
	{
		rc = ERR_TNL_MAX_ENTRIES;
		goto err0;
	}

	if (get_onif_by_name(cmd.name))
	{
		rc = ERR_TNL_ALREADY_CREATED;
		goto err0;
	}

	/* Get the tunnel entry */
	if((rc = M_tnl_get((cmd.mode == TNL_MODE_GRE_IPV6), &pTunnelEntry)  != 0))
		goto err0;

	switch (cmd.mode)
	{
	case TNL_MODE_ETHERIPV6:
		pTunnelEntry->proto = PROTO_IPV6;
		pTunnelEntry->output_proto = PROTO_NONE;

		break;

	case TNL_MODE_ETHERIPV4:
		pTunnelEntry->proto = PROTO_IPV4;
		pTunnelEntry->output_proto = PROTO_NONE;

		break;

	case TNL_MODE_6O4:
		if (cmd.secure)
		{
			rc = ERR_TNL_NOT_SUPPORTED;
			goto err1;
		}

		pTunnelEntry->proto = PROTO_IPV4;
		pTunnelEntry->output_proto = PROTO_IPV6;
		pTunnelEntry->frag_off = cmd.frag_off;

		break;

	 case TNL_MODE_4O6:
                if (cmd.secure)
                        return ERR_TNL_NOT_SUPPORTED;

                pTunnelEntry->proto = PROTO_IPV6;
                pTunnelEntry->output_proto = PROTO_IPV4;

		break;

	case TNL_MODE_GRE_IPV6:
		pTunnelEntry->proto = PROTO_IPV6;
		pTunnelEntry->output_proto = PROTO_NONE;

		break;

	default:
		rc = ERR_TNL_NOT_SUPPORTED;
		goto err1;
//		break;
	}

	pTunnelEntry->mode = cmd.mode;


	/* For copy we don't care to copy useless data in IPv4 case */
	SFL_memcpy(pTunnelEntry->local, cmd.local, IPV6_ADDRESS_LENGTH);
	SFL_memcpy(pTunnelEntry->remote, cmd.remote, IPV6_ADDRESS_LENGTH);
	pTunnelEntry->secure = cmd.secure;
	pTunnelEntry->fl = cmd.fl;
	pTunnelEntry->hlim = cmd.hlim;
	pTunnelEntry->elim = cmd.elim;
	pTunnelEntry->route_id = cmd.route_id;
	pTunnelEntry->pRtEntry = L2_route_get(pTunnelEntry->route_id);
	pTunnelEntry->tnl_mtu  = cmd.mtu;

	/* Now create a new interface in the Interface Manager */
	if (!add_onif(cmd.name, &pTunnelEntry->itf, NULL, IF_TYPE_TUNNEL))
	{
		rc = ERR_CREATION_FAILED;
		goto err1;
	}
//	pTunnelEntry->output_port_id =  (pTunnelEntry->onif->flags & PHY_PORT_ID) >> PHY_PORT_ID_LOG; /* FIXME */

	M_tnl_build_header(pTunnelEntry);

	pTunnelEntry->state = TNL_STATE_CREATED;

	if(((pTunnelEntry->proto == PROTO_IPV4) && (!pTunnelEntry->remote[0])) ||
			is_ipv6_addr_any(pTunnelEntry->remote))
                 pTunnelEntry->state |= TNL_STATE_REMOTE_ANY;

	if (cmd.enabled)
		pTunnelEntry->state |= TNL_STATE_ENABLED;

	if(cmd.mode == TNL_MODE_GRE_IPV6)
	{
		/* Get the hash */
		hash = HASH_GRE_TNL(pTunnelEntry->remote);
#if defined(COMCERTO_2000)
		if((rc = M_tnl_add_gre(pTunnelEntry, hash)) != 0)
			goto err1;
		tnl_update_gre(pTunnelEntry);
#endif
		tunnel_created_gre++;
	}
	else
	{
#if defined(COMCERTO_2000)
		tnl_update(pTunnelEntry->tunnel_index);
#endif
		tunnel_created_dmem++;
	}
	return NO_ERR;

err1:
	M_tnl_delete(pTunnelEntry);

err0:
	return rc;
}

/**
 * TNL_reset_IPSEC
 *
 *
 */
static void TNL_reset_IPSEC(PTnlEntry pTunnelEntry)
{
        pTunnelEntry->SA_nr =  0;
        pTunnelEntry->SAReply_nr =  0;
        pTunnelEntry->state &= ~TNL_STATE_SA_COMPLETE;
        pTunnelEntry->state &= ~TNL_STATE_SAREPLY_COMPLETE;
}


/**
 * TNL_handle_UPDATE
 *
 *
 */
static int TNL_handle_UPDATE(U16 *p, U16 Length)
{
	POnifDesc onif;
	TNLCommand_create cmd;
	PTnlEntry pTunnelEntry;

	/* Check length */
	if (Length != sizeof(TNLCommand_create))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(TNLCommand_create));

	onif = get_onif_by_name(cmd.name);
	if (!onif)
		return ERR_TNL_ENTRY_NOT_FOUND;

	pTunnelEntry = (PTnlEntry)onif->itf;

	if (pTunnelEntry->mode != cmd.mode)
		return ERR_TNL_NOT_SUPPORTED;

	if (pTunnelEntry->pRtEntry)
	{
		L2_route_put(pTunnelEntry->pRtEntry);

		pTunnelEntry->pRtEntry = NULL;
	}

	pTunnelEntry->state &= ~TNL_STATE_REMOTE_ANY;
	/* For copy we don't care to copy useless data in IPv4 case */
	SFL_memcpy(pTunnelEntry->local, cmd.local, IPV6_ADDRESS_LENGTH);
	SFL_memcpy(pTunnelEntry->remote, cmd.remote, IPV6_ADDRESS_LENGTH);


	if(((pTunnelEntry->proto == PROTO_IPV4) && (!pTunnelEntry->remote[0])) ||
			is_ipv6_addr_any(pTunnelEntry->remote))
                 pTunnelEntry->state |= TNL_STATE_REMOTE_ANY;


	if((!cmd.secure) && (pTunnelEntry->secure))
		TNL_reset_IPSEC(pTunnelEntry);

	pTunnelEntry->secure = cmd.secure;
	pTunnelEntry->fl = cmd.fl;
	pTunnelEntry->hlim = cmd.hlim;
	pTunnelEntry->elim = cmd.elim;
	pTunnelEntry->route_id = cmd.route_id;
	pTunnelEntry->pRtEntry = L2_route_get(pTunnelEntry->route_id);
	pTunnelEntry->tnl_mtu  = cmd.mtu;

	M_tnl_build_header(pTunnelEntry);

	if (cmd.enabled)
		pTunnelEntry->state |= TNL_STATE_ENABLED;
	else
		pTunnelEntry->state &= ~TNL_STATE_ENABLED;


#if defined(COMCERTO_2000)
	if(pTunnelEntry->mode == TNL_MODE_GRE_IPV6)
	{
		tnl_update_gre(pTunnelEntry);
	}
	else
		tnl_update(pTunnelEntry->tunnel_index);
#endif
	return NO_ERR;
}


/**
 * TNL_handle_DELETE
 *
 *
 */
static int TNL_handle_DELETE(U16 *p, U16 Length)
{
	TNLCommand_delete cmd;
	PTnlEntry pTunnelEntry = NULL;

	/* Check length */
	if (Length != sizeof(TNLCommand_delete))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(TNLCommand_delete));

	if((pTunnelEntry = M_tnl_get_by_name(cmd.name)) != NULL)
	{
		/* Tell the Interface Manager to remove the tunnel IF */
		remove_onif_by_index(pTunnelEntry->itf.index);
		if(pTunnelEntry->mode == TNL_MODE_GRE_IPV6)
			tunnel_created_gre--;
		else
			tunnel_created_dmem--;
		M_tnl_delete(pTunnelEntry);
	}
	else
	{
		return ERR_TNL_ENTRY_NOT_FOUND;
	}

	return NO_ERR;
}


/**
 * TNL_handle_IPSEC
 *
 *
 */
static int TNL_handle_IPSEC(U16 *p, U16 Length)
{
	TNLCommand_ipsec cmd;
	PTnlEntry pTunnelEntry = NULL;
	int i;

	/* Check length */
	if (Length != sizeof(TNLCommand_ipsec))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(TNLCommand_ipsec));

	if((pTunnelEntry = M_tnl_get_by_name(cmd.name)) == NULL)
		return ERR_TNL_ENTRY_NOT_FOUND;

	if(pTunnelEntry->secure == 0)
		return ERR_WRONG_COMMAND_PARAM;

	if (cmd.SA_nr > SA_MAX_OP)
			return ERR_CT_ENTRY_TOO_MANY_SA_OP;

	for (i=0;i<cmd.SA_nr;i++) {
		if (M_ipsec_sa_cache_lookup_by_h( cmd.SA_handle[i]) == NULL)
			return ERR_CT_ENTRY_INVALID_SA;
	}

	if (cmd.SAReply_nr > SA_MAX_OP)
		return ERR_CT_ENTRY_TOO_MANY_SA_OP;

	for (i=0;i<cmd.SAReply_nr;i++) {
		if (M_ipsec_sa_cache_lookup_by_h(cmd.SAReply_handle[i]) == NULL)
			return ERR_CT_ENTRY_INVALID_SA;
	}

	for (i=0;i<cmd.SA_nr;i++) {
		pTunnelEntry->hSAEntry_out[i]= cmd.SA_handle[i];
		pTunnelEntry->SA_nr = cmd.SA_nr;
		pTunnelEntry->state |= TNL_STATE_SA_COMPLETE;
	}

	for (i=0;i<cmd.SAReply_nr;i++)  {
		 pTunnelEntry->hSAEntry_in[i]= cmd.SAReply_handle[i];
		 pTunnelEntry->SAReply_nr = cmd.SAReply_nr;
		 pTunnelEntry->state |= TNL_STATE_SAREPLY_COMPLETE;
	}

#ifdef TNL_DBG
	if(cmd.SA_nr)
		gTNLCtx.tunnel_dbg[TNL_DBG_COMPLETE] |= TNL_DBG_SA_COMPLETE;
	if(cmd.SAReply_nr)
		gTNLCtx.tunnel_dbg[TNL_DBG_COMPLETE] |= TNL_DBG_SAREPLY_COMPLETE;
#endif

#if defined(COMCERTO_2000)
	if(pTunnelEntry->mode == TNL_MODE_GRE_IPV6)
        {
                tnl_update_gre(pTunnelEntry);
        }
        else
        {
                tnl_update(pTunnelEntry->tunnel_index);
        }

#endif
	return NO_ERR;
}


void TNL_set_id_conv_seed( sam_port_info_t * sp, U8 IdConvEnable, PTnlEntry t )
{
	t->sam_id_conv_enable = (IdConvEnable) ? SAM_ID_CONV_PSID: SAM_ID_CONV_NONE;
	if(!t->sam_id_conv_enable)
		return;
	// initialize global value
	t->sam_abit     = 0;
	t->sam_abit_len = sp->psid_offset;

	t->sam_kbit     = sp->port_set_id;
	t->sam_kbit_len = sp->port_set_id_length;

	t->sam_mbit     = 0;
	t->sam_mbit_len = 16 - (t->sam_abit_len + t->sam_kbit_len);

	// set the maximum value for a bit and m bit
	t->sam_abit_max = ~(0xffff<<t->sam_abit_len);
	t->sam_mbit_max = ~(0xffff<<t->sam_mbit_len);

	return;
}


/**
 * TNL_handle_IdConv_psid
 *
 *
 */

static int TNL_handle_IdConv_psid(U16 *p, U16 Length)
{
	TNLCommand_IdConvPsid cmd;
	PTnlEntry pTunnelEntry = NULL;

	/* Check length */
	if (Length != sizeof(TNLCommand_IdConvPsid))
		return ERR_WRONG_COMMAND_SIZE;
	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(TNLCommand_IdConvPsid));
	if((pTunnelEntry = M_tnl_get_by_name(cmd.name)) == NULL)
		return ERR_TNL_ENTRY_NOT_FOUND;
	TNL_set_id_conv_seed(&cmd.sam_port_info,cmd.IdConvStatus,pTunnelEntry);
	#if defined(COMCERTO_2000)
	tnl_update(pTunnelEntry - &gTNLCtx.tunnel_table[0]);
	#endif
	return 0;
}


/**
 * TNL_handle_IdConv_dupsport
 *
 *
 */

static int TNL_handle_IdConv_dupsport(U16 *p, U16 Length)
{
	TNLCommand_IdConvDP cmd;
	PTnlEntry pTunnelEntry = NULL;
	int i = 0;

	/* Check length */
	if (Length != sizeof(TNLCommand_IdConvDP))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(TNLCommand_IdConvDP));

	for (i = 0; i < TNL_MAX_TUNNEL_DMEM; i++)
	{
		pTunnelEntry = &gTNLCtx.tunnel_table[i];
		if(pTunnelEntry->mode == TNL_MODE_4O6)
		{
			pTunnelEntry->sam_id_conv_enable = (cmd.IdConvStatus) ? SAM_ID_CONV_DUPSPORT: SAM_ID_CONV_NONE;
			#if defined(COMCERTO_2000)
			tnl_update(i);
			#endif
		}
	}

	return 0;
}

#if defined(COMCERTO_2000)

/**
 * tnl_update
 *
 * Update the hardware tunnel tables
 */

void tnl_update(int tunnel_index)
{
	int j;
	PTnlEntry pTunnelEntry;
	PTnlEntry phw_tnl;
	TnlEntry hw_tnl;
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int id;

	pTunnelEntry = &gTNLCtx.tunnel_table[tunnel_index];
	phw_tnl = &hw_tnl;
	if (pTunnelEntry->state == TNL_STATE_FREE)
	{
		memset(phw_tnl, 0, sizeof(TnlEntry));
		phw_tnl->state = TNL_STATE_FREE;
	}
	else
	{
		/* Construct the hardware entry, converting virtual addresses and endianess where needed */
		memcpy(phw_tnl, pTunnelEntry, offsetof(TnlEntry, first_unused_field));
		if(pTunnelEntry->pRtEntry)
		{
			phw_tnl->route.itf = cpu_to_be32(virt_to_class(pTunnelEntry->pRtEntry->itf));
			memcpy(phw_tnl->route.dstmac, pTunnelEntry->pRtEntry->dstmac, ETHER_ADDR_LEN);
			phw_tnl->route.mtu = cpu_to_be16(pTunnelEntry->pRtEntry->mtu);
			if(pTunnelEntry->mode == TNL_MODE_6O4)
				phw_tnl->route.Daddr_v4 = pTunnelEntry->pRtEntry->Daddr_v4;
		}
		else
		{
			/* mark route as not valid */
			phw_tnl->route.itf = 0xffffffff;
		}
		phw_tnl->itf.phys = (void *)cpu_to_be32(virt_to_class(pTunnelEntry->itf.phys));
		phw_tnl->fl = pTunnelEntry->fl;
		phw_tnl->frag_off = pTunnelEntry->frag_off;
		phw_tnl->SAReply_nr = cpu_to_be16(pTunnelEntry->SAReply_nr);
		phw_tnl->SA_nr = cpu_to_be16(pTunnelEntry->SA_nr);
		phw_tnl->sam_abit = cpu_to_be16(pTunnelEntry->sam_abit);
		phw_tnl->sam_abit_max = cpu_to_be16(pTunnelEntry->sam_abit_max);
		phw_tnl->sam_kbit = cpu_to_be16(pTunnelEntry->sam_kbit);
		phw_tnl->sam_mbit = cpu_to_be16(pTunnelEntry->sam_mbit);
		phw_tnl->sam_mbit_max = cpu_to_be16(pTunnelEntry->sam_mbit_max);
		phw_tnl->tnl_mtu = cpu_to_be16(pTunnelEntry->tnl_mtu);

		for (j = 0; j < SA_MAX_OP; j++)
		{
			phw_tnl->hSAEntry_in[j] = cpu_to_be16(pTunnelEntry->hSAEntry_in[j]);
			phw_tnl->hSAEntry_out[j] = cpu_to_be16(pTunnelEntry->hSAEntry_out[j]);
		}
	}

	pe_sync_stop(ctrl, CLASS_MASK);
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_memcpy_to32(id, virt_to_class_dmem(pTunnelEntry), phw_tnl, sizeof(TnlEntry));
	pe_start(ctrl, CLASS_MASK);
}



/**
 * tnl_update_gre
 *
 * Update the hardware tunnel tables
 */

void tnl_update_gre(PTnlEntry pTunnelEntry)
{
	int j;
	PHw_TnlEntry_gre phw_tnl = pTunnelEntry->hw_tnl_entry;

	/* Construct the hardware entry, converting virtual addresses and endianess where needed */
	memcpy(phw_tnl, pTunnelEntry, offsetof(TnlEntry, first_unused_field));
	if(pTunnelEntry->pRtEntry)
	{
		phw_tnl->route.itf = cpu_to_be32(virt_to_class_dmem(pTunnelEntry->pRtEntry->itf)); /* Physical interface present in DMEM */
		memcpy(phw_tnl->route.dstmac, pTunnelEntry->pRtEntry->dstmac, ETHER_ADDR_LEN);
		phw_tnl->route.mtu = cpu_to_be16(pTunnelEntry->pRtEntry->mtu);
	}
	else
	{
		/* mark route as not valid */
		phw_tnl->route.itf = 0xffffffff;
	}
	/* This is basically never used for tunnels, the physical interfacae is picked up from the route. */
	phw_tnl->itf.phys = (void *)cpu_to_be32(virt_to_class_dmem(pTunnelEntry->itf.phys));
	phw_tnl->fl = pTunnelEntry->fl;
	phw_tnl->frag_off = pTunnelEntry->frag_off;
	phw_tnl->SAReply_nr = cpu_to_be16(pTunnelEntry->SAReply_nr);
	phw_tnl->SA_nr = cpu_to_be16(pTunnelEntry->SA_nr);
	phw_tnl->tnl_mtu = cpu_to_be16(pTunnelEntry->tnl_mtu);

	for (j = 0; j < SA_MAX_OP; j++)
	{
		phw_tnl->hSAEntry_in[j] = cpu_to_be16(pTunnelEntry->hSAEntry_in[j]);
		phw_tnl->hSAEntry_out[j] = cpu_to_be16(pTunnelEntry->hSAEntry_out[j]);
	}
}

#endif	// defined(COMCERTO_2000)

/**
 * M_tnl_cmdproc
 *
 *
 *
 */
static U16 M_tnl_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 retlen = 2;

	//printk(KERN_INFO "%s: cmd_code=0x%x, cmd_len=%d\n", __func__, cmd_code, cmd_len);
	switch (cmd_code)
	{
		case CMD_TNL_CREATE:
			rc = TNL_handle_CREATE(pcmd, cmd_len);
			break;

		case CMD_TNL_UPDATE:
			rc = TNL_handle_UPDATE(pcmd, cmd_len);
			break;

		case CMD_TNL_DELETE:
			rc = TNL_handle_DELETE(pcmd, cmd_len);
			break;

		case CMD_TNL_IPSEC:
			rc = TNL_handle_IPSEC(pcmd, cmd_len);
			break;

		case CMD_TNL_4o6_ID_CONVERSION_dupsport:
                        rc = TNL_handle_IdConv_dupsport(pcmd, cmd_len);
                        break;

		case CMD_TNL_4o6_ID_CONVERSION_psid:
                        rc = TNL_handle_IdConv_psid(pcmd, cmd_len);
                        break;

		case CMD_TNL_QUERY:
		case CMD_TNL_QUERY_CONT:
		{
			PTNLCommand_query ptnl_cmd_qry = (PTNLCommand_query) (pcmd);
			rc = Tnl_Get_Next_Hash_Entry(ptnl_cmd_qry, cmd_code == CMD_TNL_QUERY);
			if (rc == NO_ERR)
				retlen = sizeof(TNLCommand_query);
			break;
		}

		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;
	return retlen;
}


#if !defined(COMCERTO_2000)
/**
 * M_tnl_in_init
 *
 *
 *
 */
BOOL M_tnl_in_init(PModuleDesc pModule)
{
	int i = 0;

	/* module entry point and channels registration */
	pModule->entry = &M_tnl_entry_in;
	pModule->cmdproc = M_tnl_cmdproc;

	tunnel_created_dmem = 0;
	tunnel_created_gre = 0;
	return 0;
}


/**
 * M_tnl_out_init
 *
 *
 *
 */
BOOL M_tnl_out_init(PModuleDesc pModule)
{

	/* module entry point and channels registration */
	pModule->entry = &M_tnl_entry_out;
	pModule->cmdproc = NULL;

	return 0;
}

#else // defined(COMCERTO_2000)

int tunnel_init(void)
{
	int i = 0;
	set_cmd_handler(EVENT_TNL_IN, M_tnl_cmdproc);

	for (i = 0; i < NUM_GRE_TUNNEL_ENTRIES; i++)
	{
		slist_head_init(&gre_tunnel_cache[i]);
		dlist_head_init(&hw_gre_tunnel_active_list[i]);
	}
	dlist_head_init(&hw_gre_tunnel_removal_list);


	timer_init(&gre_tnl_timer, gre_tunnel_delayed_remove);
	timer_add(&gre_tnl_timer, CT_TIMER_INTERVAL);
	return 0;
}


void tunnel_exit(void)
{
	int i;
	struct slist_entry *entry;
	PTnlEntry pTunnelEntry;

	for (i = 0; i < TNL_MAX_TUNNEL_DMEM; i++)
	{
		pTunnelEntry = &gTNLCtx.tunnel_table[i];
		if (pTunnelEntry->state != TNL_STATE_FREE)
		{
			M_tnl_delete(pTunnelEntry);
			tnl_update(i);
		}

	}

	for (i = 0; i < NUM_GRE_TUNNEL_ENTRIES; i++)
	{
		slist_for_each_safe(pTunnelEntry, entry, &gre_tunnel_cache[i], list) {
			M_tnl_delete(pTunnelEntry);
		}
	}

	timer_del(&gre_tnl_timer);
}
#endif /* !defined(COMCERTO_2000) */
