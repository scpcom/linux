
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


#if !defined (COMCERTO_2000)
#include "system.h"
#include "heapmgr.h"
#include "fpp.h"
#include "gemac.h"
#include "modules.h"
#include "checksum.h"
#include "module_ethernet.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_hidrv.h"
#include "layer2.h"
#include "module_timer.h"
#include "module_expt.h"
#include "fe.h"
#include "module_ipsec.h"
#include "module_tunnel.h"
#include "module_qm.h"
#include "module_stat.h"

#else 

#include "control_common.h"
#include "module_hidrv.h"
#include "module_ipsec.h"
#include "module_socket.h"

#endif

#define SOCKET_NATT	0

int IPsec_Get_Next_SAEntry(PSAQueryCommand  pSAQueryCmd, int reset_action);
static int IPsec_Free_Natt_socket_v6(PSAEntry sa);
static int IPsec_Free_Natt_socket_v4(PSAEntry sa);

#if defined(COMCERTO_2000)

/* The following definitions of caches are used to store the
   pointers in CLASS and UTIL PE respectively */

extern TIMER_ENTRY sa_timer;
PVOID CLASS_DMEM_SH2(sa_cache_by_h)[NUM_SA_ENTRIES] __attribute__((aligned(32)));
PVOID CLASS_DMEM_SH2(sa_cache_by_spi)[NUM_SA_ENTRIES] __attribute__((aligned(32)));

PVOID UTIL_DMEM_SH2(sa_cache_by_h)[NUM_SA_ENTRIES] __attribute__((aligned(32)));

struct dlist_head hw_sa_removal_list; 
struct dlist_head hw_sa_active_list_h[NUM_SA_ENTRIES];
struct dlist_head hw_sa_active_list_spi[NUM_SA_ENTRIES];


/** Allocates the S/W SA 
 */
static PSAEntry sa_alloc(void)
{
	return pfe_kzalloc(sizeof(SAEntry), GFP_KERNEL);
}

/** Frees the S/W SA 
 *@param sa  S/W SA to be freed.
 */
static void sa_free(PSAEntry sa)
{
	pfe_kfree(sa);
	return;
}

/** Update the SA cache array to the class 
 *@param host_addr  - Address of the sa_cache in the HOST
 *@param pe_addr  - Address of the sa_cache in the CLASSPE
 */
static void sa_add_to_class(u32 host_addr, u32 *pe_addr)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int id;

	pe_sync_stop(ctrl, CLASS_MASK);

	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
	{
		pe_dmem_write(id, host_addr,virt_to_class_dmem(pe_addr), 4);
	}
	pe_start(ctrl, CLASS_MASK);
	return;
}

/** Update the SA cache array to the util 
 *@param host_addr  - Address of the sa_cache in the HOST
 *@param pe_addr  - Address of the sa_cache in the UTILPE
 */

static void sa_add_to_util(u32 host_addr, u32 *pe_addr)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	pe_sync_stop(ctrl, (1 << UTIL_ID));

	pe_dmem_write(UTIL_ID, host_addr, virt_to_util_dmem(pe_addr), 4);

	pe_start(ctrl, (1 << UTIL_ID));
	return;
}

/** Update the SA cache array by handle to the PEs 
 *@param host_addr  - Address of the sa_cache in the HOST
 *@param hash  - hash index to the array. 
 */
static void sa_update_pe_sa_cache_by_h(u32 host_addr, u32 hash)
{
	u32 *pe_addr;

	pe_addr = (u32*)&class_sa_cache_by_h[hash];
	sa_add_to_class(host_addr, pe_addr);

	pe_addr = (u32*)&util_sa_cache_by_h[hash];
	sa_add_to_util(host_addr, pe_addr);
}

/** Update the SA cache array by spi to the PEs 
 *@param host_addr  - Address of the sa_cache in the HOST
 *@param hash  - hash index to the array. 
 */
static void sa_update_pe_sa_cache_by_spi(u32 host_addr, u32 hash)
{
	u32 *pe_addr;

	pe_addr = (u32*)&class_sa_cache_by_spi[hash];
	sa_add_to_class(host_addr, pe_addr);
}


/** Schedules an hardware sa entry for removal.
 * The entry is added to a removal list and it will be free later from a timer.
 * The removal time must be bigger than the worst case PE processing time for tens of packets.
 *
 * @param hw_sa                pointer to the hardware SA entry
 *
 */
static void hw_sa_schedule_remove(struct _t_hw_sa_entry *hw_sa)
{
	hw_sa->removal_time = jiffies + 2;
	dlist_add(&hw_sa_removal_list, &hw_sa->list_h);
}

/** Processes hardware SA delayed removal list.
 * Free all hardware SA in the removal list that have reached their removal time.
 *
 *
 */
static void hw_sa_delayed_remove(void)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct dlist_head *entry;
	struct _t_hw_sa_entry *hw_sa;

	dlist_for_each_safe(hw_sa, entry, &hw_sa_removal_list, list_h)
	{
		if (!time_after(jiffies, hw_sa->removal_time))
			continue;

		if (hw_sa->elp_virt_sa)
			freeElpSA(hw_sa->elp_virt_sa, be32_to_cpu(hw_sa->elp_sa));
		dlist_remove(&hw_sa->list_h);

		dma_pool_free(ctrl->dma_pool, hw_sa, be32_to_cpu(hw_sa->dma_addr));

	}
}

/** Updates the hardware sa entry .
 * The function updates/fills the H/W SA from the S/W SA and 
 * @param pSA                pointer to the software SA entry
 *
 */
static void sa_update(PSAEntry pSA)
{
	struct _t_hw_sa_entry *hw_sa = pSA->hw_sa;
	u8* src, *dst;
	int i;

	if(!pSA->hw_sa)
		return;

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s updating sa \n", __func__);
#endif
	hw_entry_set_flags(&hw_sa->hw_sa_flags, HW_SA_UPDATING | HW_SA_VALID);

	/* Fill the Hardware SA details here */

	src = (u8*)&pSA->tunnel.ip4;
	dst = (u8*)&hw_sa->tunnel.ip4; 
	for (i = 0 ; i < pSA->header_len; i++)
		dst[i] = src[i];

	src = (u8*) &pSA->id;
	dst = (u8*) &hw_sa->id; 
	for (i = 0 ; i < sizeof( struct _tSAID); i++)
		dst[i] = src[i];

	if(pSA->pRtEntry)
	{
		hw_sa->route.itf = cpu_to_be32(virt_to_class(pSA->pRtEntry->itf));
		memcpy(hw_sa->route.dstmac, pSA->pRtEntry->dstmac, ETHER_ADDR_LEN);
		hw_sa->route.mtu = cpu_to_be16(pSA->pRtEntry->mtu);
	}
	else
	{
		/* mark route as not valid */
		hw_sa->route.itf = 0xffffffff;
	}


	hw_sa->handle = cpu_to_be16(pSA->handle);
	hw_sa->dev_mtu = cpu_to_be16(pSA->dev_mtu);
	hw_sa->mtu = cpu_to_be16(pSA->mtu);
	hw_sa->family = pSA->family;
	hw_sa->header_len = pSA->header_len;
	hw_sa->mode = pSA->mode;
	hw_sa->flags = pSA->flags;
	hw_sa->state = pSA->state;

	hw_sa->elp_sa =(struct _tElliptic_SA*) cpu_to_be32(pSA->elp_sa_dma_addr);

	hw_sa->natt.sport = cpu_to_be16(pSA->natt.sport);
	hw_sa->natt.dport = cpu_to_be16(pSA->natt.dport);
	hw_sa->natt.socket = (void*)cpu_to_be32((u32)pSA->natt.socket);

	/* Update the lft_conf fields */

	hw_sa->lft_conf.soft_byte_limit = cpu_to_be64(pSA->lft_conf.soft_byte_limit);
	hw_sa->lft_conf.hard_byte_limit = cpu_to_be64(pSA->lft_conf.hard_byte_limit);
	hw_sa->lft_conf.soft_packet_limit = cpu_to_be64(pSA->lft_conf.soft_packet_limit);
	hw_sa->lft_conf.hard_packet_limit = cpu_to_be64(pSA->lft_conf.hard_packet_limit);
	hw_entry_set_flags(&hw_sa->hw_sa_flags, HW_SA_VALID);



	return;
}

static void sa_route_update(PSAEntry pSA)
{
        struct _t_hw_sa_entry *hw_sa = pSA->hw_sa;

        if(!hw_sa)
		return;

#ifdef CONTROL_IPSEC_DEBUG
        printk(KERN_INFO "%s updating sa route \n", __func__);
#endif
        hw_entry_set_flags(&hw_sa->hw_sa_flags, HW_SA_UPDATING | HW_SA_VALID);

        if(pSA->pRtEntry)
        {
                hw_sa->route.itf = cpu_to_be32(virt_to_class(pSA->pRtEntry->itf));
                memcpy(hw_sa->route.dstmac, pSA->pRtEntry->dstmac, ETHER_ADDR_LEN);
                hw_sa->route.mtu = cpu_to_be16(pSA->pRtEntry->mtu);
        }
        else
        {
                /* mark route as not valid */
                hw_sa->route.itf = 0xffffffff;
        }
	hw_entry_set_flags(&hw_sa->hw_sa_flags, HW_SA_VALID);
}

/** Add the pe's hw sa entry from software sa entry .
 * This function links the software sa to the hash tables (based on h, spi)
 * And also allocates the hw sa from lmem or ddr and fills the h/w sa entry 
 * with the information from the s/w sa entry
 * @param pSA                pointer to the software SA entry
 *
 */
static int sa_add(PSAEntry pSA)
{
	dma_addr_t dma_addr;
	struct _t_hw_sa_entry *hw_sa , *hw_sa_first;
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int rc = NO_ERR;

	/* Allocate Hardware entry */

	hw_sa = dma_pool_alloc(ctrl->dma_pool, GFP_ATOMIC, &dma_addr);
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s H/W DDR SA allocated virtual:%x physical:%x\n", __func__,(u32)hw_sa,(u32)dma_addr);
#endif
	if (!hw_sa)
	{
		printk(KERN_ERR "%s H/W SA allocation error \n", __func__);
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err;
	}

	memset(hw_sa,0,sizeof(struct _t_hw_sa_entry));
	slist_add(&sa_cache_by_h[pSA->hash_by_h], &pSA->list_h);
	slist_add(&sa_cache_by_spi[pSA->hash_by_spi], &pSA->list_spi);

	hw_sa->dma_addr = cpu_to_be32(dma_addr);
	/* Link Software SA to Hardware SA */
	pSA->hw_sa = hw_sa;
	hw_sa->sw_sa = pSA;

	/* Link Elp SA to Hardware SA */

	hw_sa->elp_virt_sa = pSA->elp_sa;

	/* add hw entry to active list and update next pointer */
	if(!dlist_empty(&hw_sa_active_list_h[pSA->hash_by_h]))
	{
		/* list is not empty, and we'll be added at head, so current first will become our next pointer */
		hw_sa_first = container_of(dlist_first(&hw_sa_active_list_h[pSA->hash_by_h]), typeof(struct _t_hw_sa_entry), list_h);
		hw_entry_set_field(&hw_sa->next_h, hw_entry_get_field(&hw_sa_first->dma_addr));
	}
	else
	{
		/* entry is empty, so we'll be the first and only one entry */
		hw_entry_set_field(&hw_sa->next_h, 0);
	}
	dlist_add(&hw_sa_active_list_h[pSA->hash_by_h], &hw_sa->list_h);

	/* item for the spi list */
	if(!dlist_empty(&hw_sa_active_list_spi[pSA->hash_by_spi]))
	{
		hw_sa_first = container_of(dlist_first(&hw_sa_active_list_spi[pSA->hash_by_spi]), typeof(struct _t_hw_sa_entry), list_spi);
		hw_entry_set_field(&hw_sa->next_spi, hw_entry_get_field(&hw_sa_first->dma_addr));
	}
	else
	{
		hw_entry_set_field(&hw_sa->next_spi, 0);
	}
	dlist_add(&hw_sa_active_list_spi[pSA->hash_by_spi], &hw_sa->list_spi);

	/* Fill the H/W Entry for this SA */

	sa_update(pSA);

#ifdef CFG_STATS
	hw_sa->stats.total_pkts_processed = 0;
	hw_sa->stats.last_pkts_processed = 0;
	hw_sa->stats.total_bytes_processed = 0;
	hw_sa->stats.last_bytes_processed = 0;
#endif

	/* Update PE with SA cache entries with the H/W entries DDR address */
	sa_update_pe_sa_cache_by_h(hw_sa->dma_addr, pSA->hash_by_h);
	sa_update_pe_sa_cache_by_spi(hw_sa->dma_addr, pSA->hash_by_spi);

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s H/W SA added  \n", __func__);
#endif
	return NO_ERR;
err:
	disableElpSA(pSA->elp_sa);
	freeElpSA(pSA->elp_sa, pSA->elp_sa_dma_addr);
	sa_free(pSA);
	return rc;
}

/** Removes the pe's hw sa entry and software sa entry .
 * This function unlinks the h/w sa from the cache list and 
 * frees the h/w sa and s/w sa.
 * @param pSA		pointer to the software SA entry
 * @param hash_by_h	hash index for sa_cache_by_h	
 * @param hash_by_spi	hash index for sa_cache_by_spi	
 */
static void sa_remove(PSAEntry pSA, u32 hash_by_h, u32 hash_by_spi)
{
	struct _t_hw_sa_entry *hw_sa;
	struct _t_hw_sa_entry *hw_sa_prev;

	/* Check if there is a hardware sa */
	if ((hw_sa = pSA->hw_sa))
	{
		/* detach from software sa */
		pSA->hw_sa = NULL;

		/* if the removed entry is first in hash slot then only PE dmem hash need to be updated */
		if (&hw_sa->list_h == dlist_first(&hw_sa_active_list_h[hash_by_h]))
		{
			sa_update_pe_sa_cache_by_h(hw_entry_get_field(&hw_sa->next_h), hash_by_h);
		}
		else
		{
			hw_sa_prev = container_of(hw_sa->list_h.prev, typeof(struct _t_hw_sa_entry), list_h);
			hw_entry_set_field(&hw_sa_prev->next_h, hw_entry_get_field(&hw_sa->next_h));
		}
		dlist_remove(&hw_sa->list_h);

		if (&hw_sa->list_spi == dlist_first(&hw_sa_active_list_spi[hash_by_spi]))
		{
			sa_update_pe_sa_cache_by_spi(hw_entry_get_field(&hw_sa->next_spi), hash_by_spi);
		}
		else
		{
			hw_sa_prev = container_of(hw_sa->list_spi.prev, typeof(struct _t_hw_sa_entry), list_spi);
			hw_entry_set_field(&hw_sa_prev->next_spi, hw_entry_get_field(&hw_sa->next_spi));
		}
		dlist_remove(&hw_sa->list_spi);

		/* now switching hw entry from active to delayed removal list */
		hw_sa_schedule_remove(hw_sa);
	}

	/* Delete NATT Socket attached */
	if ((pSA->header_len == IPV6_HDR_SIZE) && (pSA->natt.socket))
		IPsec_Free_Natt_socket_v6(pSA);
	/* Delete NATT Socket attached */
	else if ((pSA->header_len == IPV4_HDR_SIZE) && (pSA->natt.socket))
		IPsec_Free_Natt_socket_v4(pSA);

	/* Delete Tunnel route if available */
	if (pSA->pRtEntry)
	{
		L2_route_put(pSA->pRtEntry);
		pSA->pRtEntry = NULL;
	}

	/* Disable ElpSA */
	disableElpSA(pSA->elp_sa);
	/* Delete ElpSA */
	//freeElpSA(pSA->elp_sa, pSA->elp_sa_dma_addr);


	/* Unlink from software list */
	slist_remove(&sa_cache_by_h[hash_by_h], &pSA->list_h);
	slist_remove(&sa_cache_by_spi[hash_by_spi], &pSA->list_spi);

	sa_free(pSA);
}

static void ipsec_set_pre_frag(u8 enable)
{
#if defined (COMCERTO_2000)
	int id;

	/* update the DMEM in class-pe */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_writeb(id, enable, virt_to_class_dmem(&class_gFppGlobals.FG_ipsec_pre_frag));
#else
	ipsec_pre_frag = enable;
#endif
}

#else
static PSAEntry sa_alloc(void)
{
	PSAEntry pSA = NULL;
	pSA = Heap_Alloc_ARAM(sizeof(SAEntry));	

	return (pSA);
}

static void sa_free(PSAEntry pSA)
{
	Heap_Free(pSA);
}

static int sa_add(PSAEntry pSA)
{
	slist_add(&sa_cache_by_h[pSA->hash_by_h], &pSA->list_h);
	slist_add(&sa_cache_by_spi[pSA->hash_by_spi], &pSA->list_spi);

	return NO_ERR;
}

static void sa_remove(PSAEntry pSA, U32 hash_by_h, U32 hash_by_spi)
{

	slist_remove(&sa_cache_by_h[hash_by_h], &pSA->list_h);
	slist_remove(&sa_cache_by_spi[hash_by_spi], &pSA->list_spi);

	/* Delete NATT Socket attached */
	if ((pSA->header_len == IPV6_HDR_SIZE) && (pSA->natt.socket))
		IPsec_Free_Natt_socket_v6(pSA);
	/* Delete NATT Socket attached */
	else if ((pSA->header_len == IPV4_HDR_SIZE) && (pSA->natt.socket))
		IPsec_Free_Natt_socket_v4(pSA);

	/* Free the ELPSA */
	freeElpSA(pSA->elp_sa);
	sa_free(pSA);
}

#endif

void*  M_ipsec_sa_cache_lookup_by_h( U16 handle)
{
	U16 hash = handle & (NUM_SA_ENTRIES -1);
	PSAEntry pEntry;
	PSAEntry pSA = NULL;
	struct slist_entry *entry;

	slist_for_each(pEntry, entry, &sa_cache_by_h[hash], list_h)
	{
		if (pEntry->handle == handle)
			pSA = pEntry;
	}

	return pSA;
}

void* M_ipsec_sa_cache_lookup_by_spi(U32 *daddr, U32 spi, U8 proto, U8 family)
{
	U32     hash_key_sa;
	PSAEntry pSA = NULL;
	PSAEntry pEntry;
	struct slist_entry *entry;

	hash_key_sa = HASH_SA(daddr, spi, proto, family);
	slist_for_each(pEntry, entry, &sa_cache_by_spi[hash_key_sa], list_spi)
	{
		if ( (pEntry->id.proto == proto) &&
				(pEntry->id.spi == spi) &&
				(pEntry->id.daddr.a6[0] == daddr[0]) &&
				(pEntry->id.daddr.a6[1] == daddr[1]) &&
				(pEntry->id.daddr.a6[2] == daddr[2]) &&
				(pEntry->id.daddr.a6[3] == daddr[3])&&
				(pEntry->family != family))
		{
			pSA = pEntry;
		}


	}

	return pSA;
}


static int M_ipsec_sa_set_digest_key(PSAEntry sa, U16 key_alg, U16 key_bits, U8* key)
{
	struct _tElliptic_SA *elp_sa = sa->elp_sa;    // elliptic SA descriptor
	U8      algo;

	switch (key_alg) {
		case SADB_AALG_MD5HMAC:
			algo = ELP_HMAC_MD5;
			break;
		case SADB_AALG_SHA1HMAC:
			algo = ELP_HMAC_SHA1;
			break;
		case SADB_X_AALG_SHA2_256HMAC:
			algo = ELP_HMAC_SHA2;
			break;
		case SADB_X_AALG_NULL:
			algo  = ELP_HMAC_NULL;
			break;
		default:
			return -1;
	}

	hw_sa_set_digest_key(sa,  key_bits,  key);

	elp_sa->algo |= algo;

	consistent_elpctl(elp_sa,0);

	return 0;
}

static int M_ipsec_sa_set_cipher_key(PSAEntry sa, U16 key_alg, U16 key_bits, U8* key)
{
	struct _tElliptic_SA *elp_sa = sa->elp_sa;              // elliptic SA descriptor
	U8      algo;

	switch (key_alg) {
		case SADB_X_EALG_AESCTR:
			if (hw_sa_set_cipher_ALG_AESCTR(sa, key_bits,key,&algo) < 0)
				return -1;
		case SADB_X_EALG_AESCBC:
			if (key_bits == 128)
				algo = ELP_CIPHER_AES128;
			else if (key_bits == 192)
				algo = ELP_CIPHER_AES192;
			else if (key_bits == 256)
				algo = ELP_CIPHER_AES256;
			else
				return -1;
			sa->blocksz = 16;
			break;
		case SADB_EALG_3DESCBC:
			algo = ELP_CIPHER_3DES;
			sa->blocksz = 8;
			break;
		case SADB_EALG_DESCBC:
			algo = ELP_CIPHER_DES;
			sa->blocksz = 8;
			break;
		case SADB_EALG_NULL:
			algo  = ELP_HMAC_NULL;
			sa->blocksz = 0;
			break;
		default:
			return -1;
	}

	hw_sa_set_cipher_key(sa,key);

	elp_sa->algo |= (algo << 4);

	consistent_elpctl(elp_sa, 0);

	return 0;
}

/* NAT-T modifications*/
static PSock6Entry IPsec_create_Natt_socket_v6(PSAEntry sa)
{
	PSock6Entry natt_socket;
	int res;

	//natt_socket = (PNatt_Socket_v6)Heap_Alloc(sizeof (Natt_Socket_v6));
	natt_socket = socket6_alloc();

	if(natt_socket == NULL)
		return NULL;

	memset(natt_socket , 0, sizeof(Sock6Entry));
	natt_socket->SocketFamily = PROTO_IPV6;
	natt_socket->SocketType = SOCKET_TYPE_FPP;
	natt_socket->owner_type = SOCK_OWNER_NATT;
	natt_socket->Dport = cpu_to_be16(sa->natt.dport);
	natt_socket->Sport = cpu_to_be16(sa->natt.sport);
	natt_socket->proto = IPPROTOCOL_UDP;
	natt_socket->connected = 1; /* Connected socket use 5 tuples */
	natt_socket->SocketID = SOCKET_NATT; /* Need to use some socket which is not used at all */
	if (sa->mode == SA_MODE_TUNNEL) {
		SFL_memcpy((U8*)&natt_socket->Saddr_v6[0],(U8*) &sa->tunnel.ip6.SourceAddress[0], IPV6_ADDRESS_LENGTH);
		SFL_memcpy((U8*)&natt_socket->Daddr_v6[0], (U8*)&sa->tunnel.ip6.DestinationAddress[0], IPV6_ADDRESS_LENGTH);
	}
	else if (sa->mode == SA_MODE_TRANSPORT) {
		SFL_memcpy((U8*)&natt_socket->Saddr_v6[0],(U8*) &sa->id.saddr[0], IPV6_ADDRESS_LENGTH);
		SFL_memcpy((U8*)&natt_socket->Daddr_v6[0], (U8*)&sa->id.daddr.a6[0], IPV6_ADDRESS_LENGTH);
	}
	natt_socket->hash = HASH_SOCK6( natt_socket->Daddr_v6[IP6_LO_ADDR], natt_socket->Dport, natt_socket->proto);
	natt_socket->hash_by_id = HASH_SOCKID( natt_socket->SocketID);

	res = socket6_add(natt_socket);
	if (res != NO_ERR)
	{
		socket6_free(natt_socket);
		return NULL;
	}
	return (natt_socket);
}


static int IPsec_Free_Natt_socket_v6(PSAEntry sa)
{
	PSock6Entry natt_socket;
	U32 hash, hash_by_id;

	natt_socket = (PSock6Entry)sa->natt.socket;
	if (natt_socket == NULL)
		return ERR_SA_SOCK_ENTRY_NOT_FOUND;

	hash = HASH_SOCK6(natt_socket->Daddr_v6[IP6_LO_ADDR], natt_socket->Dport, natt_socket->proto);
	hash_by_id = HASH_SOCKID(natt_socket->SocketID);

	socket6_remove(natt_socket, hash, hash_by_id);

	return NO_ERR;
}

static PSockEntry IPsec_create_Natt_socket_v4(PSAEntry sa)
{
	PSockEntry natt_socket;
	int res;

	natt_socket = socket4_alloc();

	if (natt_socket == NULL)
		return NULL;

	memset(natt_socket , 0, sizeof(SockEntry));
	natt_socket->SocketFamily = PROTO_IPV4;
	natt_socket->SocketType = SOCKET_TYPE_FPP;
	natt_socket->owner_type = SOCK_OWNER_NATT;
	natt_socket->Dport = cpu_to_be16(sa->natt.dport);
	natt_socket->Sport = cpu_to_be16(sa->natt.sport);
	natt_socket->proto = IPPROTOCOL_UDP;
	natt_socket->connected = 1; /* Connected socket use 5 tuples */
	natt_socket->SocketID = SOCKET_NATT; /* DUMMY SOCKET for NATT */

	if (sa->mode == SA_MODE_TUNNEL) {
		natt_socket->Saddr_v4 = sa->tunnel.ip4.SourceAddress;
		natt_socket->Daddr_v4 = sa->tunnel.ip4.DestinationAddress;
	}
	else if (sa->mode == SA_MODE_TRANSPORT) {
		natt_socket->Saddr_v4 = sa->id.saddr[0];
		natt_socket->Daddr_v4 = sa->id.daddr.a6[0];
	}

	natt_socket->hash = HASH_SOCK(natt_socket->Daddr_v4, natt_socket->Dport, natt_socket->proto);
	natt_socket->hash_by_id = HASH_SOCKID(natt_socket->SocketID);

	res = socket4_add(natt_socket);
	if (res != NO_ERR)
	{
		socket4_free(natt_socket);
		return NULL;
	}
	return (natt_socket);
}

static int IPsec_Free_Natt_socket_v4(PSAEntry sa)
{
	PSockEntry natt_socket;
	U32 hash, hash_by_id;

	natt_socket = (PSockEntry)sa->natt.socket;
	if (natt_socket == NULL)
		return ERR_SA_SOCK_ENTRY_NOT_FOUND;

	hash = HASH_SOCK(natt_socket->Daddr_v4, natt_socket->Dport, natt_socket->proto);
	hash_by_id = HASH_SOCKID(natt_socket->SocketID);

	socket4_remove(natt_socket,hash,hash_by_id);
	return NO_ERR;
}

void* M_ipsec_sa_cache_create(U32 *saddr,U32 *daddr, U32 spi, U8 proto, U8 family, U16 handle, U8 replay, U8 esn, U16 mtu, U16 dev_mtu)
{
	U32     hash_key_sa;
	PSAEntry sa;


#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s .. Started..\n", __func__);
#endif
	//sa = Heap_Alloc_ARAM(sizeof(SAEntry));
	sa = sa_alloc();
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s S/W SA allocated:%x\n", __func__,(u32)sa);
#endif
	if (sa) {
		memset(sa, 0, sizeof(SAEntry));
		hash_key_sa = HASH_SA(daddr, spi, proto, family);
#ifdef CONTROL_IPSEC_DEBUG
		printk(KERN_INFO "%s hash_key_sa:%d\n", __func__,hash_key_sa);
#endif
		sa->id.saddr[0] = saddr[0];
		sa->id.saddr[1] = saddr[1];
		sa->id.saddr[2] = saddr[2];
		sa->id.saddr[3] = saddr[3];

		sa->id.daddr.a6[0] = daddr[0];
		sa->id.daddr.a6[1] = daddr[1];
		sa->id.daddr.a6[2] = daddr[2];
		sa->id.daddr.a6[3] = daddr[3];
		sa->id.spi = spi;
		sa->id.proto = proto;
		sa->family = family;
		sa->handle = handle;
		sa->mtu = mtu;
		sa->dev_mtu = dev_mtu;
		sa->state = SA_STATE_INIT;
		sa->elp_sa = allocElpSA(&sa->elp_sa_dma_addr);
#ifdef CONTROL_IPSEC_DEBUG
		printk(KERN_INFO "%s elp_sa allocated:%x-%x\n", __func__,(u32)sa->elp_sa, (u32)virt_to_phys_iram((void*)sa->elp_sa));
#endif
		if(!sa->elp_sa)
		{
			//Heap_Free(sa);
			sa_free(sa);
			return NULL;
		}

		memset(sa->elp_sa, 0, /* sizeof(struct _tElliptic_SA)*/ELP_SA_ALIGN);
#ifdef COMCERTO_100
		sa->elp_sa->seq = 0;
		sa->elp_sa->thread_id = handle;
#endif
		sa->elp_sa->spi = ntohl(spi);
		sa->elp_sa->anti_replay_mask = 0;
		if (proto == IPPROTOCOL_AH)
			sa->elp_sa->flags = ESPAH_AH_MODE;
		else
			sa->elp_sa->flags = 0;

		// mat-2009-02-20
		// Presently linux is missing controls for extended sequence number checking and
		// allowing sequence number rollower.
		// clp30 engine is implemented to rfc4301 and will autoi-disable on seq_roll unless
		// it is specifically allowed to happen.
		// For now I am putting in heuristical rule:
		//   if (! auto_replay_enable)
		//      allow_seq_rollower
		//  I expect that if/when the lack of api is brought up both seqnum and allow_roll
		//  will be known to linux proper and/or ike implementations which need it.
		//
		if (replay)
			sa->elp_sa->flags |= ESPAH_ANTI_REPLAY_ENABLE;
		else {
			sa->elp_sa->flags |= ESPAH_SEQ_ROLL_ALLOWED;
			sa->flags |= SA_ALLOW_SEQ_ROLL;
		}

		//Per RFC 4304 - Should be used by default for IKEv2, unless specified by SA configuration.

		if(esn)
			sa->elp_sa->flags |= ESPAH_EXTENDED_SEQNUM;

#ifndef COMCERTO_100
		if (family == PROTO_IPV6)
			sa->elp_sa->flags |= ESPAH_IPV6_ENABLE;
#endif

		consistent_elpctl(sa->elp_sa, 1);
#ifndef COMCERTO_100
#if defined(IPSEC_DEBUG) && (IPSEC_DEBUG)
		gIpSecHWCtx.flush_create_count += 1;
#endif
#endif
		sa->hash_by_spi = hash_key_sa;
		sa->hash_by_h   =  handle & (NUM_SA_ENTRIES - 1);
		if (sa_add(sa) != NO_ERR)
		{
#ifdef CONTROL_IPSEC_DEBUG
			printk(KERN_INFO "%s sa_add failed\n", __func__);
#endif
			return NULL;

		}

	}
	return sa;
}

static int M_ipsec_sa_cache_delete(U16 handle)
{
	U32     hash_key_sa_by_spi;
	U32	hash_key_sa_by_h = handle & (NUM_SA_ENTRIES-1);
	PSAEntry pSA;


	pSA = M_ipsec_sa_cache_lookup_by_h(handle);
	if (!pSA)
		return ERR_SA_UNKNOWN;

	hash_key_sa_by_spi = HASH_SA(pSA->id.daddr.top, pSA->id.spi, pSA->id.proto, pSA->family);

	sa_remove(pSA , hash_key_sa_by_h , hash_key_sa_by_spi);
	return NO_ERR;
}


int IPsec_handle_CREATE_SA(U16 *p, U16 Length)
{
	CommandIPSecCreateSA cmd;
	U8 family;

	/* Check length */
	if (Length != sizeof(CommandIPSecCreateSA))
		return ERR_WRONG_COMMAND_SIZE;

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s .. Started..\n", __func__);
#endif
	memset(&cmd, 0, sizeof(CommandIPSecCreateSA));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	family = (cmd.said.proto_family == PROTO_FAMILY_IPV4) ? PROTO_IPV4 : PROTO_IPV6;
	if (M_ipsec_sa_cache_lookup_by_spi((U32*) cmd.said.dst_ip , cmd.said.spi, cmd.said.sa_type , family)) {
		return ERR_SA_DUPLICATED;
	}
	if (M_ipsec_sa_cache_lookup_by_h(cmd.sagd)) {
		return ERR_SA_DUPLICATED;
	}

	if (M_ipsec_sa_cache_create((U32*)cmd.said.src_ip, (U32*)cmd.said.dst_ip , cmd.said.spi, cmd.said.sa_type , family, cmd.sagd, cmd.said.replay_window, (cmd.said.flags & NLKEY_SAFLAGS_ESN), cmd.said.mtu, cmd.said.dev_mtu)) {

		return NO_ERR;
	}
	else
		return ERR_CREATION_FAILED;

}



static int IPsec_handle_DELETE_SA(U16 *p, U16 Length)
{
	CommandIPSecDeleteSA cmd;
	int rc;

	/* Check length */
	if (Length != sizeof(CommandIPSecDeleteSA))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(CommandIPSecDeleteSA));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	rc = M_ipsec_sa_cache_delete(cmd.sagd);

	return (rc);

}

static int IPsec_handle_FLUSH_SA(U16 *p, U16 Length)
{
	PSAEntry pEntry;
	int i;

	// scan sa_cache and delete sa
	for(i = 0; i < NUM_SA_ENTRIES; i++)
	{
		struct slist_entry *entry;
		slist_for_each_safe(pEntry, entry, &sa_cache_by_h[i], list_h)
		{
			U32  hash_key_sa_by_h = pEntry->handle & (NUM_SA_ENTRIES-1);
			U32  hash_key_sa_by_spi = HASH_SA(pEntry->id.daddr.top, pEntry->id.spi, pEntry->id.proto, pEntry->family);

			sa_remove(pEntry, hash_key_sa_by_h, hash_key_sa_by_spi);
		}
	}

	memset(sa_cache_by_h, 0, sizeof(struct slist_head)*NUM_SA_ENTRIES);
	memset(sa_cache_by_spi, 0, sizeof(struct slist_head)*NUM_SA_ENTRIES);
	return NO_ERR;
}

int IPsec_handle_SA_SET_KEYS(U16 *p, U16 Length)
{
	CommandIPSecSetKey cmd;
	PIPSec_key_desc key;
	PSAEntry sa;
	int i;

	/* Check length */
	if (Length != sizeof(CommandIPSecSetKey))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(CommandIPSecSetKey));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	sa = M_ipsec_sa_cache_lookup_by_h(cmd.sagd);

	if (sa == NULL)
		return ERR_SA_UNKNOWN;
	for (i = 0;i<cmd.num_keys;i++) {
		key = (PIPSec_key_desc)&cmd.keys[i];
		if (key->key_type) {
			if (M_ipsec_sa_set_cipher_key(sa, key->key_alg, key->key_bits, key->key))
				return ERR_SA_INVALID_CIPHER_KEY;
		}
		else if (M_ipsec_sa_set_digest_key(sa, key->key_alg, key->key_bits, key->key))
			return ERR_SA_INVALID_DIGEST_KEY;
	}

	return NO_ERR;
}

int IPsec_handle_SA_SET_TUNNEL(U16 *p, U16 Length)
{
	CommandIPSecSetTunnel cmd;
	PSAEntry sa;

	/* Check length */
	if (Length != sizeof(CommandIPSecSetTunnel))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(CommandIPSecSetTunnel));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	sa = M_ipsec_sa_cache_lookup_by_h(cmd.sagd);

	if (sa == NULL)
		return ERR_SA_UNKNOWN;
	if (cmd.proto_family == PROTO_FAMILY_IPV4) {
		sa->header_len = IPV4_HDR_SIZE;
		SFL_memcpy(&sa->tunnel.ip4, &cmd.h.ipv4h, sa->header_len);
	}
	else {
		sa->header_len = IPV6_HDR_SIZE;
		SFL_memcpy(&sa->tunnel.ip6, &cmd.h.ipv6h, sa->header_len);
	}

	sa->mode = SA_MODE_TUNNEL;

	sa_update(sa);

	return NO_ERR;

}

static int IPsec_handle_SA_SET_NATT(U16 *p, U16 Length)
{
	CommandIPSecSetNatt  cmd;
	PSAEntry sa;

	/* Check length */
	if (Length != sizeof(CommandIPSecSetNatt))
		return ERR_WRONG_COMMAND_SIZE;

	// NAT-T modifications
	memset(&cmd, 0, sizeof(CommandIPSecSetNatt));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	sa = M_ipsec_sa_cache_lookup_by_h(cmd.sagd);

	if (sa == NULL)
		return ERR_SA_UNKNOWN;

	// Add the socket information
	sa->natt.sport = htons(cmd.sport);
	sa->natt.dport = htons(cmd.dport);
	sa->natt.socket = NULL;

	if ((sa->family == PROTO_IPV6) && (sa->natt.sport) && (sa->natt.dport))
	{
		sa->natt.socket = IPsec_create_Natt_socket_v6(sa);
		if (sa->natt.socket == NULL)
		{
			sa->natt.sport = 0;
			sa->natt.dport = 0;
			return ERR_CREATION_FAILED;
		}
	}
	else if ((sa->family == PROTO_IPV4) && (sa->natt.sport) &&(sa->natt.dport))
	{
		sa->natt.socket = IPsec_create_Natt_socket_v4(sa);
		if (sa->natt.socket == NULL)
		{
			sa->natt.sport = 0;
			sa->natt.dport = 0;
			return ERR_CREATION_FAILED;
		}
	}

	sa_update(sa);
	// NAT-T configuration ends.
	return NO_ERR;
}

static int IPsec_handle_SA_SET_TNL_ROUTE(U16 *p, U16 Length)
{
	CommandIPSecSetTunnelRoute  cmd;
	PSAEntry sa;

	/* Check length */
	if (Length != sizeof(CommandIPSecSetTunnelRoute))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(CommandIPSecSetTunnelRoute));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	sa = M_ipsec_sa_cache_lookup_by_h(cmd.sagd);

	if (sa == NULL)
		return ERR_SA_UNKNOWN;

	if (sa->mode != SA_MODE_TUNNEL)
		return ERR_SA_INVALID_MODE;

	if (sa->pRtEntry)
	{
		L2_route_put(sa->pRtEntry);
		sa->pRtEntry = NULL;
	}

	sa->route_id = cmd.route_id;
	sa->pRtEntry = L2_route_get(sa->route_id);

	sa_route_update(sa);

	return NO_ERR;
}

int IPsec_handle_SA_SET_STATE(U16 *p, U16 Length)
{
	CommandIPSecSetState cmd;
	PSAEntry sa;

	/* Check length */
	if (Length != sizeof(CommandIPSecSetState))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(CommandIPSecSetState));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	sa = M_ipsec_sa_cache_lookup_by_h(cmd.sagd);

	if (sa == NULL)
		return ERR_SA_UNKNOWN;
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "set_state:%x-%x\n", cmd.state , sa->state);
#endif
	if ((cmd.state == XFRM_STATE_VALID) &&  (sa->state == SA_STATE_INIT)) {
#ifdef CONTROL_IPSEC_DEBUG
		printk(KERN_INFO "valid:\n");
#endif
		sa->state = SA_STATE_VALID;
		sa->elp_sa->flags |= ESPAH_ENABLED;
#ifndef COMCERTO_100
		sa->flags |= SA_ENABLED;
#if	!defined(ELP_HW_BYTECNT) || (ELP_HW_BYTECNT == 0)
		sa->lft_cur.bytes = 0;
#endif
#else
		sa->lft_cur.bytes = 0;
#endif

		sa->lft_cur.packets = 0;
	}
	else if (cmd.state != XFRM_STATE_VALID) {
#ifdef CONTROL_IPSEC_DEBUG
		printk(KERN_INFO "not valid:\n");
#endif
		sa->state = SA_STATE_DEAD;
		sa->elp_sa->flags &= ~ESPAH_ENABLED;
#ifndef COMCERTO_100
		sa->flags &= ~SA_ENABLED;
#endif
		M_ipsec_sa_cache_delete(sa->handle);
		return NO_ERR;
	}
	consistent_elpctl(sa->elp_sa, 1);
#ifndef COMCERTO_100
#if defined(IPSEC_DEBUG) && (IPSEC_DEBUG) 
	if (sa->elp_sa->flags & ESPAH_ENABLED)
		gIpSecHWCtx.flush_enable_count += 1;
	else
		gIpSecHWCtx.flush_disable_count += 1;
#endif
#endif

	sa_update(sa);
	return NO_ERR;
}


int IPsec_handle_SA_SET_LIFETIME(U16 *p, U16 Length)
{
	CommandIPSecSetLifetime cmd;
	PSAEntry sa;

	/* Check length */
	if (Length != sizeof(CommandIPSecSetLifetime))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(CommandIPSecSetLifetime));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	sa = M_ipsec_sa_cache_lookup_by_h(cmd.sagd);

	if (sa == NULL)
		return ERR_SA_UNKNOWN;

	sa->lft_conf.soft_byte_limit =  (U64)cmd.soft_time.bytes[0] + ((U64)cmd.soft_time.bytes[1] << 32);
	sa->lft_conf.soft_packet_limit = cmd.soft_time.allocations;
	sa->lft_conf.hard_byte_limit =  (U64)cmd.hard_time.bytes[0] + ((U64)cmd.hard_time.bytes[1] << 32);
	sa->lft_conf.hard_packet_limit = cmd.hard_time.allocations;

#ifdef CONTROL_IPSEC_DEBUG
	printk (KERN_INFO "set_lifetime:bytes:%llu - %llu\n",sa->lft_conf.soft_byte_limit, sa->lft_conf.hard_byte_limit);
#endif

	sa_update(sa);
	hw_sa_set_lifetime(&cmd,sa);

	return NO_ERR;
}

static int IPsec_handle_FRAG_CFG(U16 *p, U16 Length)
{
	CommandIPSecSetPreFrag cmd;

	/* Check length */
	if (Length != sizeof(CommandIPSecSetPreFrag))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(CommandIPSecSetPreFrag));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);

	ipsec_set_pre_frag(cmd.pre_frag_en);

	return NO_ERR;

}

/**
 * M_ipsec_cmdproc
 *
 *
 *
 */
U16 M_ipsec_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 retlen = 2;

	switch (cmd_code)
	{
		case CMD_IPSEC_SA_CREATE:
			rc = IPsec_handle_CREATE_SA(pcmd, cmd_len);
			break;

		case CMD_IPSEC_SA_DELETE:
			rc = IPsec_handle_DELETE_SA(pcmd, cmd_len);
			break;

		case CMD_IPSEC_SA_FLUSH:
			rc = IPsec_handle_FLUSH_SA(pcmd, cmd_len);
			break;

		case CMD_IPSEC_SA_SET_KEYS:
			rc = IPsec_handle_SA_SET_KEYS(pcmd, cmd_len);
			break;

		case CMD_IPSEC_SA_SET_TUNNEL:
			rc = IPsec_handle_SA_SET_TUNNEL(pcmd, cmd_len);
			break;
		case CMD_IPSEC_SA_SET_TNL_ROUTE:
			rc = IPsec_handle_SA_SET_TNL_ROUTE(pcmd, cmd_len);
			break;

		case CMD_IPSEC_SA_SET_NATT:
			rc = IPsec_handle_SA_SET_NATT(pcmd, cmd_len);
			break;

		case CMD_IPSEC_SA_SET_STATE:
			rc = IPsec_handle_SA_SET_STATE(pcmd, cmd_len);
			break;

		case CMD_IPSEC_SA_SET_LIFETIME:
			rc = IPsec_handle_SA_SET_LIFETIME(pcmd, cmd_len);
			break;
		case CMD_IPSEC_SA_ACTION_QUERY:
		case CMD_IPSEC_SA_ACTION_QUERY_CONT:
			rc = IPsec_Get_Next_SAEntry((PSAQueryCommand)pcmd, cmd_code == CMD_IPSEC_SA_ACTION_QUERY);
			if (rc == NO_ERR)
				retlen += sizeof (SAQueryCommand);
			break;

		case CMD_IPSEC_FRAG_CFG:
			rc = IPsec_handle_FRAG_CFG(pcmd, cmd_len);
			break;

		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;

	return retlen;
}


#if !defined(COMCERTO_2000)
static void ipsec_common_soft_init(IPSec_hw_context *sc) {
	// Local portion of initialization
	// Only do things, which can be undone,
	// such as init of private memory

#if     defined(IPSEC_DEBUG) && (IPSEC_DEBUG)
	memset(sc->inbound_counters,0,64);
	memset(sc->outbound_counters,0,64);
#endif
#if	defined(IPSEC_DDRC_WA) && (IPSEC_DDRC_WA)
	L1_dc_invalidate(DDR_FLUSH_ADDR, DDR_FLUSH_ADDR);
#endif	/* defined(IPSEC_DDRC_WA) && (IPSEC_DDRC_WA) */
	sc->in_pe.wq_avail = 1; // Cause inbound processing to be available for passthrough to exception path
}

BOOL M_ipsec_pre_inbound_init(void)
{
	set_event_handler(EVENT_IPS_IN, M_ipsec_inbound_entry);
	set_cmd_handler(EVENT_IPS_IN, M_ipsec_cmdproc);

	gIpsec_available = 0;
	ipsec_common_soft_init(&gIpSecHWCtx);

	return 0;
}

BOOL M_ipsec_post_inbound_init(void)
{
	set_event_handler(EVENT_IPS_IN_CB, M_ipsec_inbound_callback);
	set_cmd_handler(EVENT_IPS_IN_CB, NULL);

	//  ipsec_common_soft_init(&gIpSecHWCtx);
	return 0;
}

BOOL M_ipsec_pre_outbound_init(void)
{
	set_event_handler(EVENT_IPS_OUT, M_ipsec_outbound_entry);
	set_cmd_handler(EVENT_IPS_OUT, M_ipsec_debug);

	return 0;
}


BOOL M_ipsec_post_outbound_init(void)
{
	set_event_handler(EVENT_IPS_OUT_CB, M_ipsec_outbound_callback);
	set_cmd_handler(EVENT_IPS_OUT_CB, NULL);

	return 0;
}
#endif
#if defined(COMCERTO_2000)
static __inline int M_ipsec_sa_expire_notify(PSAEntry sa, int hard)
{
	struct _tCommandIPSecExpireNotify *message;
	HostMessage *pmsg;

	pmsg = msg_alloc();
	if (!pmsg)
		goto err;

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "sending an event:%x:%x\n",hard,sa->handle);
#endif
	message = (struct _tCommandIPSecExpireNotify *)	pmsg->data;

	/*Prepare indication message*/
	message->sagd = sa->handle;
	message->action = (hard) ? IPSEC_HARD_EXPIRE : IPSEC_SOFT_EXPIRE;
	pmsg->code = CMD_IPSEC_SA_NOTIFY;
	pmsg->length = sizeof(*message);

	if (msg_send(pmsg) < 0)
		goto err;

	return 0;

err:
	return 1;
}
#endif


static void M_ipsec_sa_timer(void)
{
	u8 state;
	PSAEntry pEntry;
	struct _t_hw_sa_entry *hw_sa;
	int i;

#if defined(COMCERTO_2000)
	hw_sa_delayed_remove();
#endif

	/* Check if any of the SA's are moved to Expired state in PFE*/
	for(i = 0; i < NUM_SA_ENTRIES; i++)
	{
		struct slist_entry *entry;

		slist_for_each(pEntry, entry, &sa_cache_by_h[i], list_h)
		{
			if ((hw_sa = pEntry->hw_sa))
			{
				state = (*(volatile u8 *)(&hw_sa->state));
				if (((pEntry->state == SA_STATE_VALID) || (pEntry->state == SA_STATE_DYING)) && (state == SA_STATE_EXPIRED))
				{
#ifdef CONTROL_IPSEC_DEBUG
					printk(KERN_INFO "E");
#endif
					pEntry->state = SA_STATE_EXPIRED;
					pEntry->notify = 1;
				}
				if ((pEntry->state == SA_STATE_VALID) && (state == SA_STATE_DYING))
				{
#ifdef CONTROL_IPSEC_DEBUG
					printk(KERN_INFO "D");
#endif
					pEntry->state = SA_STATE_DYING;
					pEntry->notify = 1;
				}
			}

			if (pEntry->notify)
			{
				int rc;

				if (pEntry->state == SA_STATE_EXPIRED)
					rc = M_ipsec_sa_expire_notify(pEntry, 1);
				else if (pEntry->state == SA_STATE_DYING)
					rc = M_ipsec_sa_expire_notify(pEntry, 0);
				else
					rc = 0;

				if (rc == 0)
					pEntry->notify = 0;
			}
		}

	}

}

BOOL ipsec_init(void)
{
	int i;
#if !defined(COMCERTO_2000)
	M_ipsec_pre_inbound_init();
	M_ipsec_post_inbound_init();
	M_ipsec_pre_outbound_init();
	M_ipsec_post_outbound_init();
#else
	/** Initialize  all SA lists .
	* This function initializes the h/w and s/w hash tables, timers.
	* and initializes the lists maintained for the h/w and s/w SA maintenance.
	*/
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s started \n", __func__);
#endif

	for (i = 0; i < NUM_SA_ENTRIES; i++)
	{
		slist_head_init(&sa_cache_by_h[i]);
		slist_head_init(&sa_cache_by_spi[i]);
		dlist_head_init(&hw_sa_active_list_h[i]);
		dlist_head_init(&hw_sa_active_list_spi[i]);
	}

	dlist_head_init(&hw_sa_removal_list);
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s lmem_sa is initialized \n", __func__);
#endif
	//lmem_sa_init();

	/* start the elp_engine and give the info to utilpe */
	/* This is moved here as packets has to be dequeued in utilpe even 
	   if SAs are not configured. This is required to send the
	   ESP packets (when no SA's are configured) to HOST */
	if (gIpsec_available == 0) {
		// Initialize hardware and dedicated aram
		ipsec_common_hard_init(&gIpSecHWCtx);
		gIpsec_available = 1;
	}

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s timer is initialized \n", __func__);
#endif
	timer_init(&sa_timer, M_ipsec_sa_timer);
	timer_add(&sa_timer, CT_TIMER_INTERVAL);

	//ipsec_standalone_init();

	/* ipsec command parser */
	set_cmd_handler(EVENT_IPS_IN, M_ipsec_cmdproc);

	return 0;

#endif
}

void ipsec_exit(void)
{
#if defined(COMCERTO_2000)
	/** Initialize  all SA lists .
	* This function cleans/frees the h/w and s/w hash tables, timers.
	* and removes the pointers from  the lists of the h/w and s/w SAs .
	*/
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct dlist_head *entry;
	struct _t_hw_sa_entry *hw_sa;
	int i;

	timer_del(&sa_timer);

	/* pe's must be stopped by now, remove all pending entries */
	for (i = 0; i < NUM_SA_ENTRIES; i++)
	{
		dlist_for_each_safe(hw_sa, entry, &hw_sa_active_list_h[i], list_h)
		{
			dlist_remove(&hw_sa->list_h);
			dma_pool_free(ctrl->dma_pool, hw_sa, be32_to_cpu(hw_sa->dma_addr));
		}
	}

	dlist_for_each_safe(hw_sa, entry, &hw_sa_removal_list, list_h)
	{
		dlist_remove(&hw_sa->list_h);
		dma_pool_free(ctrl->dma_pool, hw_sa, be32_to_cpu(hw_sa->dma_addr));
	}
#endif
}

U16 M_ipsec_debug(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16   rc;
	U16   retlen = 2;
	U16   i16;

	switch (cmd_code)
	{
		case  CMD_TRC_DMEM:
			{
				PDMCommand dmcmd = (PDMCommand) pcmd;
				i16 = dmcmd->length;            // Length;
				if (i16 > 224)
					i16 = 224;
				if (i16) {
					dmcmd->length = i16; // Send back effective length
					SFL_memcpy(&(pcmd[sizeof(*dmcmd)/sizeof(unsigned short)]),(void*) dmcmd->address, i16);
					rc = CMD_OK;
					retlen = i16 + sizeof(DMCommand);

				} else {
					rc = CMD_TRC_ERR;
				}
			}
			break;

		default:
			rc = CMD_TRC_UNIMPLEMENTED;
			break;
	}
	*pcmd = rc;
	return retlen;
}
