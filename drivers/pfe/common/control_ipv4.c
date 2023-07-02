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

#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_hidrv.h"
#include "module_timer.h"
#include "alt_conf.h"
#include "fpp.h"
#include "module_ipsec.h"
#include "module_rtp_relay.h"
#include "Module_ipv4frag.h"
#include "layer2.h"
#ifdef COMCERTO_1000
#include "module_msp.h"
#endif
#include "control_socket.h"
#include "fppdiag_lib.h"
#include "module_stat.h"

int IPv4_Get_Next_Hash_CTEntry(PCtExCommand pCtCmd, int reset_action);
int IPV4_Get_Next_Hash_RtEntry(PRtCommand pRtCmd, int reset_action);

extern TIMER_ENTRY ip_timer;

#if !defined(COMCERTO_2000) 
U8 ct_proto[MAX_CONNTRACK_PROTO] = {IPPROTOCOL_TCP,IPPROTOCOL_UDP,IPPROTOCOL_IPIP};
#endif

#if defined(COMCERTO_2000)

struct dlist_head ct_removal_list;

/** Gets the flags field from an hardware conntrack.
* The function converts the value read from PFE/DDR format to host format (endianess conversion).
*
* @param ct		pointer to an hardware conntrack
*
* @return		flags field value
*
*/
U32 hw_ct_get_flags(struct hw_ct *ct)
{
	return be32_to_cpu(readl(&ct->flags));
}


/** Sets the flags field in an hardware conntrack.
* The function converts the value written from host format to PFE/DDR format (endianess conversion).
*
* @param ct		pointer to an hardware conntrack
* @param flags		flags value to set
*
*/
void hw_ct_set_flags(struct hw_ct *ct, U32 flags)
{
	/* FIXME if the entire conntrack is in non-cacheable/non-bufferable memory,
	  there should be no need for the memory barrier */
	wmb();
	writel(cpu_to_be32(flags), &ct->flags);
}


/** Gets the next field in an hardware conntrack.
* The function converts the value read from PFE/DDR format to host format.
*
* @param ct		pointer to an hardware conntrack
*
* @return		next field value (physical address)
*
*/
U32 hw_ct_get_next(struct hw_ct *ct)
{
	return be32_to_cpu(readl(&ct->next));
}


/** Sets the next field in an hardware conntrack.
* The function converts the value written from host format to PFE/DDR format (endianess conversion).
*
* @param ct		pointer to an hardware conntrack
* @param next		next field value (physical address)
*
*/
void hw_ct_set_next(struct hw_ct *ct, U32 next)
{
	/* FIXME if the entire conntrack is in non-cacheable/non-bufferable memory,
	  there should be no need for the memory barrier */
	wmb();
	writel(cpu_to_be32(next), &ct->next);
}


/** Gets the active field in an hardware conntrack.
* The function converts the value read from PFE/DDR format to host format.
*
* @param ct		pointer to an hardware conntrack
*
* @return		active field value
*
*/
U32 hw_ct_get_active(struct hw_ct *ct)
{
	return be32_to_cpu(readl(&ct->active));
}


/** Sets the active field in an hardware conntrack.
* The function converts the value written from host format to PFE/DDR format (endianess conversion).
*
* @param ct		pointer to an hardware conntrack
* @param active		active field value
*
*/
void hw_ct_set_active(struct hw_ct *ct, U32 active)
{
	writel(cpu_to_be32(active), &ct->active);
}

/** Allocates a new software conntrack.
* The originator and replier conntracks are allocated as a single object
*
* @return 		pointer to the new conntrack, NULL of error
*
*/
PCtEntry ct_alloc(void)
{
	PCtEntry pEntry_orig;
	PCtEntry pEntry_rep;

	/* Allocate local entry */
	pEntry_orig = pfe_kzalloc(2 * sizeof(CtEntry), GFP_KERNEL);
	if (!pEntry_orig)
		return NULL;

	pEntry_rep = pEntry_orig + 1;

	pEntry_orig->twin = pEntry_rep;
	pEntry_rep->twin = pEntry_orig;

	return pEntry_orig;
}


/** Frees a software conntrack.
* The function fress both originator and replier conntracks
*
* @param pEntry_orig	pointer to the originator conntrack
*/
void ct_free(PCtEntry pEntry_orig)
{
	/* Free local entry */
	pfe_kfree(pEntry_orig);
}


struct dlist_head *ct_dlist_head(U32 hash)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct hw_ct *ct = ctrl->hash_array_baseaddr + hash * CLASS_ROUTE_SIZE;

	return &ct->list;
}


static struct hw_ct *ct_container(struct dlist_head *entry)
{
	return container_of(entry, typeof(struct hw_ct), list);
}


/** Adds a software conntrack to the hardware hash.
* The VALID flag is not set in this routine (must be set by the caller).
* In case of memory allocation error the conntrack is not added to the hash.
*
* @param pEntry		pointer to the software conntrack
* @param hash		hash index where to add the conntrack
*
* @return		NO_ERR in case of success, ERR_xxx in case of error
*/
static int hw_ct_add_one(PCtEntry pEntry, U32 hash)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct hw_ct *ct, *prev = NULL;
	int i;

	/* FIXME add workaround for hardware fetch mismatch, if IPv4 add to head of list, otherwise to tail */
	/* If using software fetch this can be avoided, otherwise it's a big mess since it may force us to take out
	one existing entry from the hash array */

	/* Allocate hardware entry */
	/* If hash array bucket is empty, use it, otherwise allocate a new entry dynamically in the dma pool */
	if (hw_ct_get_flags(ctrl->hash_array_baseaddr + hash * CLASS_ROUTE_SIZE) & CT_USED)
	{
		dma_addr_t dma_addr;

//		printk(KERN_INFO "ct add collision\n");

		ct = dma_pool_alloc(ctrl->dma_pool_128, GFP_ATOMIC, &dma_addr);
		if (!ct)
		{
			printk(KERN_ERR "%s: dma_pool_alloc() failed\n", __func__);
			goto err_alloc;
		}

		ct->dma_addr = cpu_to_be32(dma_addr);

		/* Add new entry just after the hash array */
		dlist_add(ct_dlist_head(hash), &ct->list);

		prev = ct_container(dlist_prev(&ct->list));
		hw_ct_set_next(ct, hw_ct_get_next(prev));
	}
	else
	{
		ct = ctrl->hash_array_baseaddr + hash * CLASS_ROUTE_SIZE;
	}

//	printk(KERN_INFO "%s %p\n", __func__, ct);

	/* Update hardware entry */
	ct->Sport = pEntry->Sport;
	ct->Dport = pEntry->Dport;

	ct->proto = pEntry->proto;
	ct->hash = cpu_to_be16(pEntry->hash);
	ct->twin_dma_addr = 0;

	memcpy(ct->Saddr_v6, pEntry->Saddr_v6, 4 * 2 * sizeof(U32));

	ct->fwmark = cpu_to_be16(pEntry->fwmark);
	ct->status = cpu_to_be16(pEntry->status);

	for (i = 0; i < SA_MAX_OP; i++)
	{
		ct->hSAEntry[i] = cpu_to_be16(pEntry->hSAEntry[i]);
	}

	hw_ct_set_active(ct, 0);

	ct->ip_chksm_corr = pEntry->ip_chksm_corr;
	ct->tcp_udp_chksm_corr = pEntry->tcp_udp_chksm_corr;

	if(!(pEntry->status & CONNTRACK_IPv6))
	{
		ct->twin_Saddr = pEntry->twin_Saddr;
		ct->twin_Daddr = pEntry->twin_Daddr;
		ct->twin_Sport = pEntry->twin_Sport;
		ct->twin_Dport = pEntry->twin_Dport;
	}
	if (IS_NULL_ROUTE(pEntry->pRtEntry))
	{
		/* Attach to route */
		IP_Check_Route(pEntry);
	}

	if (!IS_NULL_ROUTE(pEntry->pRtEntry))
	{
		ct->route.itf = cpu_to_be32(virt_to_class(pEntry->pRtEntry->itf));
		memcpy(ct->route.dstmac, pEntry->pRtEntry->dstmac, ETHER_ADDR_LEN);
		ct->route.mtu = cpu_to_be16(pEntry->pRtEntry->mtu);
//		ct->route.flags = pEntry->pRtEntry->flags;
		ct->route.Daddr_v4= pEntry->pRtEntry->Daddr_v4;
	}
	else
	{
		ct->route.itf = 0xffffffff;
	}

	if (!IS_NULL_ROUTE(pEntry->tnl_route))
	{

		if(pEntry->status & CONNTRACK_4O6)
		{
			ct->tnl4o6_route.itf = cpu_to_be32(virt_to_class(pEntry->tnl_route->itf));
			memcpy(ct->tnl4o6_route.dstmac, pEntry->tnl_route->dstmac, ETHER_ADDR_LEN);
			ct->tnl4o6_route.mtu = cpu_to_be16(pEntry->tnl_route->mtu);
//		ct->tnl_route.flags = pEntry->tnl_route->flags;
			ct->tnl4o6_route.Daddr_v6[0] = pEntry->tnl_route->Daddr_v6[0];
			ct->tnl4o6_route.Daddr_v6[1] = pEntry->tnl_route->Daddr_v6[1];
			ct->tnl4o6_route.Daddr_v6[2] = pEntry->tnl_route->Daddr_v6[2];
			ct->tnl4o6_route.Daddr_v6[3] = pEntry->tnl_route->Daddr_v6[3];
		}
		else
		{
			ct->tnl6o4_route.itf = cpu_to_be32(virt_to_class(pEntry->tnl_route->itf));
			memcpy(ct->tnl6o4_route.dstmac, pEntry->tnl_route->dstmac, ETHER_ADDR_LEN);
			ct->tnl6o4_route.mtu = cpu_to_be16(pEntry->tnl_route->mtu);
			ct->tnl6o4_route.Daddr_v4 = pEntry->tnl_route->Daddr_v4;
		}
	}
	else
	{
		if(pEntry->status & CONNTRACK_IPv6)
			ct->tnl6o4_route.itf = 0xffffffff;
		else
			ct->tnl4o6_route.itf = 0xffffffff;
	}

	/* Link to hardware list */
	if (!IS_HASH_ARRAY(ctrl, ct))
		hw_ct_set_next(prev, be32_to_cpu(ct->dma_addr));

	/* Link software conntrack to hardware conntrack */
	pEntry->ct = ct;
	ct->pCtEntry = pEntry;

	/* Mark entry in use (valid flag will be set after twin entry is created) */
	hw_ct_set_flags(ct, CT_USED);

	return NO_ERR;

err_alloc:
	IP_delete_CT_route(pEntry);
	return ERR_NOT_ENOUGH_MEMORY;
}


/** Schedules an hardware conntrack for removal.
* The entry is added to a removal list and it will be free later from a timer.
* The removal time must be bigger than the worst case PE processing time for tens of packets.
*
* @param ct		pointer to the hardware conntrack
*
*/
static void hw_ct_schedule_remove(struct hw_ct *ct)
{
	ct->pCtEntry = NULL;

	ct->removal_time = jiffies + 2;
	dlist_add(&ct_removal_list, &ct->rlist);
}


/** Processes hardware conntrack delayed removal list.
* Free all hardware conntracks in the removal list that have reached their removal time.
* If the entry being free is in the hash array, then move the next entry (if any) to the hash array.
*
* @param ct		pointer to the hardware conntrack
*
*/
static void hw_ct_delayed_remove(void)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct dlist_head *entry;
	struct hw_ct *ct;

	dlist_for_each_safe(ct, entry, &ct_removal_list, rlist)
	{
//		printk(KERN_INFO "%s %p\n", __func__, ct);

		if (!time_after(jiffies, ct->removal_time))
			continue;

		dlist_remove(&ct->rlist);

		if (IS_HASH_ARRAY(ctrl, ct))
		{
			hw_ct_set_flags(ct, 0);

			/* If the hash bucket is not empty try to move one entry to the hash array */
			/* This is a performance optimization, to avoid a hash collision even after
			   the entry in the hash array has been removed */
			if (!dlist_empty(&ct->list))
			{
				struct hw_ct *next = ct_container(dlist_first(&ct->list));

//				printk(KERN_INFO "moving next to hash array %p %p\n", next, &ct->list);

				/* Copy contents except flags, next at the beginning and dma_addr, list, ... at the end */
				memcpy((void *)&ct->Sport, (void *)&next->Sport, (void *)&ct->dma_addr - (void *)&ct->Sport);

				hw_ct_set_flags(ct, CT_USED | CT_VALID);

				/* From now on the entry is valid and it will match packets, but the next still points to it's old copy */
				/* Also, there may be packets pending still using the old copy */

				hw_ct_set_next(ct, hw_ct_get_next(next));

				/* Link software conntrack to new hardware conntrack */
				if (next->pCtEntry)
				{
					PCtEntry twin;
					ct->pCtEntry = next->pCtEntry;
					ct->pCtEntry->ct = ct;
					// Update our twin's twin_dma_addr field
					twin = ct->pCtEntry->twin;
					if (twin && twin->ct)
						twin->ct->twin_dma_addr = ct->dma_addr;
				}

				/* Unlink old copy from software list and schedule removal */
				dlist_remove(&next->list);

				hw_ct_schedule_remove(next);
			}
		}
		else
		{
			dma_pool_free(ctrl->dma_pool_128, ct, be32_to_cpu(ct->dma_addr));
		}
	}
}


/** Removes a software conntrack from the local hash and hardware hash
* The hardware conntrack is marked invalid/in use and scheduled to delayed removal.
*
* @param pEntry		pointer to the software conntrack
* @param hash		hash index where to remove the conntrack
*
*/
static void hw_ct_remove_one(PCtEntry pEntry, U32 hash)
{
	struct hw_ct *ct, *prev;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	/* Check if there is a hardware conntrack */
	if ((ct = pEntry->ct))
	{
		/* Unlink from software conntrack */
		pEntry->ct = NULL;

//		printk(KERN_INFO "%s %p\n", __func__, ct);

		if (IS_HASH_ARRAY(ctrl, ct))
		{
			/* Mark invalid but still in use */
			/* From this point on, the entry no longer matches but hardware/software still follows pointer
			to next entry */
			hw_ct_set_flags(ct, CT_USED);
		}
		else
		{
			/* Unlink from hardware list */
			prev = ct_container(dlist_prev(&ct->list));

			hw_ct_set_next(prev, hw_ct_get_next(ct));

			/* Unlink from software list */
			dlist_remove(&ct->list);
		}

		hw_ct_schedule_remove(ct);
	}

	IP_delete_CT_route(pEntry);
}


/** Adds a software conntrack (both directions)
*
* @param pEntry_orig	pointer to the originator software conntrack
* @param pEntry_rep	pointer to the replier software conntrack
* @param hash_orig	hash index where to add the originator conntrack
* @param hash_rep	hash index where to add the replier conntrack
*
* @return		NO_ERR in case of success, ERR_xxx in case of error
*/
int ct_add(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep)
{
	int rc;

	if ((rc = hw_ct_add_one(pEntry_orig, hash_orig)) != NO_ERR)
		goto err0;

	if ((rc = hw_ct_add_one(pEntry_rep, hash_rep)) != NO_ERR)
		goto err1;

	/* Cross-link the two hw entries */
	pEntry_orig->ct->twin_dma_addr = pEntry_rep->ct->dma_addr;
	pEntry_rep->ct->twin_dma_addr = pEntry_orig->ct->dma_addr;

	if(IS_IPV4(pEntry_orig))
	{
		/* check if rtp stats entry is created for this conntrack, if found link the two object and mark the conntrack 's status field for RTP stats */
		rtpqos_ipv4_link_stats_entry_by_tuple(pEntry_orig, pEntry_orig->Saddr_v4, pEntry_orig->Daddr_v4, pEntry_orig->Sport, pEntry_orig->Dport);
		rtpqos_ipv4_link_stats_entry_by_tuple(pEntry_rep, pEntry_rep->Saddr_v4, pEntry_rep->Daddr_v4, pEntry_rep->Sport, pEntry_rep->Dport);
	}
	else
	{
		/* check if rtp stats entry is created for this conntrack, if found link the two object and mark the conntrack 's status field for RTP stats */
		rtpqos_ipv6_link_stats_entry_by_tuple(pEntry_orig, pEntry_orig->Saddr_v6, pEntry_orig->Daddr_v6, pEntry_orig->Sport, pEntry_orig->Dport);
		rtpqos_ipv6_link_stats_entry_by_tuple(pEntry_rep, pEntry_rep->Saddr_v6, pEntry_rep->Daddr_v6, pEntry_rep->Sport, pEntry_rep->Dport);
	}

	/* Add to PFE hash atomically */
	hw_ct_set_flags(pEntry_orig->ct, CT_USED | CT_VALID);
	hw_ct_set_flags(pEntry_rep->ct, CT_USED | CT_VALID);

	/* Add to local hash */
	slist_add(&ct_cache[hash_orig], &pEntry_orig->list);
	slist_add(&ct_cache[hash_rep], &pEntry_rep->list);

#ifdef CFG_STATS
	STATISTICS_CTRL_INCREMENT(num_active_connections);
#endif

	return NO_ERR;

err1:
	hw_ct_remove_one(pEntry_orig, hash_orig);

err0:
	L2_route_put(pEntry_orig->tnl_route);
	L2_route_put(pEntry_rep->tnl_route);
	ct_free(pEntry_orig);

	return rc;
}


/** Removes a software conntrack (both directions)
*
* @param pEntry_orig	pointer to the originator software conntrack
* @param pEntry_rep	pointer to the replier software conntrack
* @param hash_orig	hash index where to remove the originator conntrack
* @param hash_rep	hash index where to remove the replier conntrack
*
* @return		NO_ERR in case of success, ERR_xxx in case of error
*/
void ct_remove(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep)
{
	L2_route_put(pEntry_orig->tnl_route);
	pEntry_orig->tnl_route = NULL;
	L2_route_put(pEntry_rep->tnl_route);
	pEntry_rep->tnl_route = NULL;

	hw_ct_remove_one(pEntry_orig, hash_orig);
	hw_ct_remove_one(pEntry_rep, hash_rep);

	slist_remove(&ct_cache[hash_orig], &pEntry_orig->list);
	slist_remove(&ct_cache[hash_rep], &pEntry_rep->list);

	/* Free local entry */
	ct_free(pEntry_orig);

#ifdef CFG_STATS
	STATISTICS_CTRL_DECREMENT(num_active_connections);
#endif
}


/** Updates a software conntrack
* Updates an hardware conntrack based on an updated software conntrack.
* We assume the hardware conntrack exists already, and update it in place.
*
* @param pEntry		pointer to the software conntrack
* @param hash		hash index where to update the conntrack
*/
void ct_update_one(PCtEntry pEntry, U32 hash)
{
	struct hw_ct *ct = pEntry->ct;
	int i;

	/* Flag entry as being updated, CLASS software will stop and poll update bit until it's clear */
	hw_ct_set_flags(ct, CT_USED | CT_UPDATING | CT_VALID);

	/* Update hw entry */
	/* 5-tuple can not change, no need to update all fields */
	ct->fwmark = cpu_to_be16(pEntry->fwmark);
	ct->status = cpu_to_be16(pEntry->status);

	if (IS_NULL_ROUTE(pEntry->pRtEntry))
	{
		IP_Check_Route(pEntry);
	}

	if (!IS_NULL_ROUTE(pEntry->pRtEntry))
	{
		ct->route.itf = cpu_to_be32(virt_to_class(pEntry->pRtEntry->itf));
		memcpy(ct->route.dstmac, pEntry->pRtEntry->dstmac, ETHER_ADDR_LEN);
		ct->route.mtu = cpu_to_be16(pEntry->pRtEntry->mtu);
		//ct->route.flags = pEntry->pRtEntry->flags;
		ct->route.Daddr_v4= pEntry->pRtEntry->Daddr_v4;
	}
	else
		ct->route.itf = 0xffffffff;

	if (!IS_NULL_ROUTE(pEntry->tnl_route))
	{
		if(pEntry->status & CONNTRACK_4O6)
		{
			ct->tnl4o6_route.itf = cpu_to_be32(virt_to_class(pEntry->tnl_route->itf));
			memcpy(ct->tnl4o6_route.dstmac, pEntry->tnl_route->dstmac, ETHER_ADDR_LEN);
			ct->tnl4o6_route.mtu = cpu_to_be16(pEntry->tnl_route->mtu);
//		ct->tnl_route.flags = pEntry->tnl_route->flags;
			ct->tnl4o6_route.Daddr_v6[0] = pEntry->tnl_route->Daddr_v6[0];
			ct->tnl4o6_route.Daddr_v6[1] = pEntry->tnl_route->Daddr_v6[1];
			ct->tnl4o6_route.Daddr_v6[2] = pEntry->tnl_route->Daddr_v6[2];
			ct->tnl4o6_route.Daddr_v6[3] = pEntry->tnl_route->Daddr_v6[3];
		}
		else
		{
			ct->tnl6o4_route.itf = cpu_to_be32(virt_to_class(pEntry->tnl_route->itf));
			memcpy(ct->tnl6o4_route.dstmac, pEntry->tnl_route->dstmac, ETHER_ADDR_LEN);
			ct->tnl6o4_route.mtu = cpu_to_be16(pEntry->tnl_route->mtu);
			ct->tnl6o4_route.Daddr_v4 = pEntry->tnl_route->Daddr_v4;
		}
	}
	else
	{

		if(pEntry->status & CONNTRACK_IPv6)
			ct->tnl6o4_route.itf = 0xffffffff;
		else
			ct->tnl4o6_route.itf = 0xffffffff;
	}

	for (i = 0; i < SA_MAX_OP; i++)
	{
		ct->hSAEntry[i] = cpu_to_be16(pEntry->hSAEntry[i]);
	}

	hw_ct_set_flags(ct, CT_USED | CT_VALID);
}


/** Updates a software conntrack (both directions)
*
* @param pEntry_orig	pointer to the originator software conntrack
* @param pEntry_rep	pointer to the replier software conntrack
* @param hash_orig	hash index where to remove the originator conntrack
* @param hash_rep	hash index where to remove the replier conntrack
*/
void ct_update(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep)
{
	ct_update_one(pEntry_orig, hash_orig);
	ct_update_one(pEntry_rep, hash_rep);
}

#else

PCtEntry ct_alloc(void)
{
	PCtEntry pEntry_orig;
	
	pEntry_orig = Heap_Alloc(CTENTRY_BIDIR_SIZE);
	if (pEntry_orig)
		memset(pEntry_orig, 0, CTENTRY_BIDIR_SIZE);

	return pEntry_orig;
}


void ct_free(PCtEntry pEntry_orig)
{
	Heap_Free(pEntry_orig);
}


int ct_add(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep)
{
	slist_add(&ct_cache[hash_orig], &pEntry_orig->list);
	slist_add(&ct_cache[hash_rep], &pEntry_rep->list);

#ifdef CFG_STATS
	STATISTICS_CTRL_INCREMENT(num_active_connections);
#endif

	if(IS_IPV4(pEntry_orig))
	{
		/* check if rtp stats entry is created for this conntrack, if found link the two object and mark the conntrack 's status field for RTP stats */
		rtpqos_ipv4_link_stats_entry_by_tuple(pEntry_orig, pEntry_orig->Saddr_v4, pEntry_orig->Daddr_v4, pEntry_orig->Sport, pEntry_orig->Dport);
		rtpqos_ipv4_link_stats_entry_by_tuple(pEntry_rep, pEntry_rep->Saddr_v4, pEntry_rep->Daddr_v4, pEntry_rep->Sport, pEntry_rep->Dport);
	}
	else
	{
		/* check if rtp stats entry is created for this conntrack, if found link the two object and mark the conntrack 's status field for RTP stats */
		rtpqos_ipv6_link_stats_entry_by_tuple(pEntry_orig, pEntry_orig->Saddr_v6, pEntry_orig->Daddr_v6, pEntry_orig->Sport, pEntry_orig->Dport);
		rtpqos_ipv6_link_stats_entry_by_tuple(pEntry_rep, pEntry_rep->Saddr_v6, pEntry_rep->Daddr_v6, pEntry_rep->Sport, pEntry_rep->Dport);
	}

	return NO_ERR;
}


void ct_remove(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep)
{
	IP_delete_CT_route(pEntry_orig);
	IP_delete_CT_route(pEntry_rep);

	L2_route_put(pEntry_orig->tnl_route);
	pEntry_orig->tnl_route = NULL;
	L2_route_put(pEntry_rep->tnl_route);
	pEntry_rep->tnl_route = NULL;
		
	slist_remove(&ct_cache[hash_rep], &pEntry_rep->list);

	slist_remove(&ct_cache[hash_orig], &pEntry_orig->list);

	CTData_Free(pEntry_orig);

#ifdef CFG_STATS
	STATISTICS_CTRL_DECREMENT(num_active_connections);
#endif
}


void ct_update(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep)
{
	return;
}

#endif

/**
 * IP_delete_CT_route()
 *
 *
 */
void IP_delete_CT_route(PCtEntry pEntry)
{
	PRouteEntry pRtEntry = pEntry->pRtEntry;
	U32 id;

	if (IS_NULL_ROUTE(pRtEntry))
		return;

	id = pRtEntry->id;

	L2_route_put(pRtEntry);

#if defined(COMCERTO_2000)
	pEntry->pRtEntry = NULL;
#else
	pEntry->route_id = id;
#endif
}


PRouteEntry IP_Check_Route(PCtEntry pCtEntry)
{
	PRouteEntry pRtEntry;

	pRtEntry = L2_route_get(pCtEntry->route_id);
	if (!pRtEntry)
		return NULL;

	pCtEntry->pRtEntry = pRtEntry;

	return pRtEntry;
}


/**
 * IP_expire()
 *
 *
 */
void IP_expire(PCtEntry pCtEntry)
{
	/* Can't send indication, try later from timeout routine */
	pCtEntry->status |= (CONNTRACK_FF_DISABLED | CONNTRACK_TIMED_OUT);

	IP_delete_CT_route(pCtEntry);
}


U32 IP_get_fwmark(PCtEntry pOrigEntry, PCtEntry pReplEntry)
{
	U32 fwmark;

	fwmark = pOrigEntry->fwmark;
	if (fwmark != pReplEntry->fwmark)
	{
		fwmark |= ((U32)pReplEntry->fwmark << 16) | 0x80000000;
	}
	return fwmark;
}


U32 Get_Ctentry_Hash(PVOID pblock)
{
	PCtEntry pctentry = pblock;
	U32 hash;
	U32 protocol;
	protocol = GET_PROTOCOL(pctentry);
	if (IS_IPV4(pblock))
	{
		hash = HASH_CT(pctentry->Saddr_v4, pctentry->Daddr_v4, pctentry->Sport, pctentry->Dport, protocol);
	}
	else
	{
		PCtEntryIPv6 pctentry6 = pblock;
		hash = HASH_CT6(pctentry6->Saddr_v6, pctentry6->Daddr_v6, pctentry6->Sport, pctentry6->Dport, protocol);
	}
	return hash;
}


int IPv4_delete_CTpair(PCtEntry ctEntry)
{
	PCtEntry twin_entry;
	struct _tCtCommand *message;
	HostMessage *pmsg;

	twin_entry = CT_TWIN(ctEntry);
	if ((twin_entry->status & CONNTRACK_ORIG) == CONNTRACK_ORIG)
	{
		ctEntry = twin_entry;
		twin_entry = CT_TWIN(ctEntry);
	}

	// Send indication message
	pmsg = msg_alloc();
	if (!pmsg)
		goto err;

	message = (struct _tCtCommand *)pmsg->data;

	// Prepare indication message
	message->action = (ctEntry->status & CONNTRACK_TCP_FIN) ? ACTION_TCP_FIN : ACTION_REMOVED;
	message->Saddr= ctEntry->Saddr_v4;
	message->Daddr= ctEntry->Daddr_v4;
	message->Sport= ctEntry->Sport;
	message->Dport= ctEntry->Dport ;
	message->SaddrReply= ctEntry->twin_Saddr;
	message->DaddrReply= ctEntry->twin_Daddr;
	message->SportReply= ctEntry->twin_Sport;
	message->DportReply= ctEntry->twin_Dport;
	message->protocol= GET_PROTOCOL(ctEntry);
	message->fwmark = 0;

	pmsg->code = CMD_IPV4_CONNTRACK_CHANGE;
	pmsg->length = sizeof(*message);

	if (msg_send(pmsg) < 0)
		goto err;

	//Remove conntrack from list
	ct_remove(ctEntry, twin_entry, Get_Ctentry_Hash(ctEntry), Get_Ctentry_Hash(twin_entry));

	return 0;

err:
	/* Can't send indication, try later from timeout routine */
	IP_expire(ctEntry);
	IP_expire(twin_entry);

	return 1;
}


/**
 * IP_ct_timer
 *
 */
static void IP_ct_timer(unsigned int start, unsigned int end)
{
	PCtEntry pCtEntry;
	PCtEntry twin_entry;
	U32 i;
	U32 timer;
	U32 twin_timer;
	U32 orig_timer;
	struct slist_entry *entry;

#if defined(COMCERTO_2000)
	hw_ct_delayed_remove();
#endif

	for (i = start; i < end; i++)
	{
restart_loop:
		slist_for_each(pCtEntry, entry, &ct_cache[i], list)
		{
			if ((pCtEntry->status & CONNTRACK_ORIG) == CONNTRACK_ORIG)  // only check orig ctEntries (not their twins)
			{
				BOOL bidir_flag;

				twin_entry = CT_TWIN(pCtEntry);

#if defined(COMCERTO_2000)
				if (ff_enable)
				{
					struct hw_ct *ct;

					if ((ct = pCtEntry->ct) && hw_ct_get_active(ct))
					{
						pCtEntry->last_ct_timer = ct_timer;
						hw_ct_set_active(ct, 0);
					}

					if ((ct = twin_entry->ct) && hw_ct_get_active(ct))
					{
						twin_entry->last_ct_timer = ct_timer;
						hw_ct_set_active(ct, 0);
					}
				}
#endif

				orig_timer = timer = ct_timer - pCtEntry->last_ct_timer;
				twin_timer = ct_timer - twin_entry->last_ct_timer;

				bidir_flag = twin_entry->last_ct_timer != UDP_REPLY_TIMER_INIT;

				if (bidir_flag)
				{
					if (twin_timer < timer)
						timer = twin_timer;
				}

				if (timer > GET_TIMEOUT_VALUE(pCtEntry , bidir_flag)
				|| (pCtEntry->status & CONNTRACK_TIMED_OUT))
				{
					if (IS_IPV4(pCtEntry))
					{
						if (!IPv4_delete_CTpair(pCtEntry))
							goto restart_loop;
					}
					else
					{
						if (!IPv6_delete_CTpair((PCtEntryIPv6)pCtEntry))
							goto restart_loop;
					}
				}

#if !defined(COMCERTO_2000)
				// Detach from route (and try to move it DDR) if no longer in use
				if (orig_timer >= CT_INACTIVE_TIME)
				{
					IP_delete_CT_route(pCtEntry);
				}
				if (bidir_flag && (twin_timer >= CT_INACTIVE_TIME))
				{
					IP_delete_CT_route(twin_entry);
				}

				// check for inactive connection
				if (timer >= CT_INACTIVE_TIME)
				{
					// if CT is in ARAM, move it to DDR
					if (Is_FastCT_block(pCtEntry))
					{
						// Move entry to DDR
						PVOID newblock;
						newblock = Heap_Alloc(CTENTRY_BIDIR_SIZE);
						if (newblock != NULL)
						{
							Fast_CTData_CopyBlock(newblock, pCtEntry);
							CTData_Free(pCtEntry);
							pCtEntry = (PCtEntry)(newblock);

							if(pCtEntry->status & CONNTRACK_RTP_STATS) {
								PCtEntryIPv6 pCT6Entry;

								if (IS_IPV4(pCtEntry)) {
									/* check if rtp stats entry is created for this conntrack, if found link the two object */
									rtpqos_ipv4_link_stats_entry_by_tuple(pCtEntry, pCtEntry->Saddr_v4, pCtEntry->Daddr_v4, pCtEntry->Sport, pCtEntry->Dport);
								} else {
									pCT6Entry = (PCtEntryIPv6)pCtEntry;
									rtpqos_ipv6_link_stats_entry_by_tuple((void *)pCT6Entry, pCT6Entry->Saddr_v6, pCT6Entry->Daddr_v6, pCT6Entry->Sport, pCT6Entry->Dport);
								}
							}
							twin_entry = CT_TWIN(pCtEntry);  // need to re-define after moving pCtEntry
							if(twin_entry->status & CONNTRACK_RTP_STATS) {
								PCtEntryIPv6 twin6_entry;

								if (IS_IPV4(twin_entry)) {
									/* check if rtp stats entry is created for this conntrack, if found link the two object */
									rtpqos_ipv4_link_stats_entry_by_tuple(twin_entry, twin_entry->Saddr_v4, twin_entry->Daddr_v4, twin_entry->Sport, twin_entry->Dport);
								} else {
									twin6_entry = (PCtEntryIPv6)twin_entry;
									rtpqos_ipv6_link_stats_entry_by_tuple((void *)twin6_entry, twin6_entry->Saddr_v6, twin6_entry->Daddr_v6, twin6_entry->Sport, twin6_entry->Dport);
								}
							}
						}
					}
				}
#endif /* !defined(COMCERTO_2000) */
			}
		}
	}
}


static void M_ip_timer(void)
{
	unsigned int start, end;

	ct_timer++;

	/* check active connection and send keep alive to CSP if needed */
	start = ct_bin_start;
	end = start + CT_TIMER_BINSIZE;

	if (end >= NUM_CT_ENTRIES)
	{
		end = NUM_CT_ENTRIES;
		ct_bin_start = 0;
	}
	else
		ct_bin_start = end;

	if (ff_enable)
		IP_ct_timer(start, end);
}


void IP_deleteCt_from_onif_index(U32 if_index)
{
	int i;
	int rc;
	PCtEntry pCtEntry;
	struct slist_entry *entry;

	for (i = 0; i < NUM_CT_ENTRIES; i++)
	{
restart_loop:
		slist_for_each_safe(pCtEntry, entry, &ct_cache[i], list)
		{
			/* Check the conntrack entry matching with the corresponding Rtentry */
			if(!(pCtEntry->status & CONNTRACK_TIMED_OUT) && !IS_NULL_ROUTE(pCtEntry->pRtEntry))
			{
				PRouteEntry pRtEntry = pCtEntry->pRtEntry;

				if (pRtEntry->itf->index == if_index)
				{
					if (IS_IPV4(pCtEntry))
						rc = IPv4_delete_CTpair(pCtEntry);
					else
						rc = IPv6_delete_CTpair((PCtEntryIPv6)pCtEntry);

					if (!rc)
						goto restart_loop;
				}
			}
		}
	}
}

static PCtEntry IPv4_find_ctentry(U32 hash, U32 saddr, U32 daddr, U16 sport, U16 dport)
{
	PCtEntry pEntry;
	struct slist_entry *entry;

	slist_for_each(pEntry, entry, &ct_cache[hash], list)
	{
		// Note: we don't need to check for protocol match, since if all of the other fields match then the
		//	protocol will always match.
		if (IS_IPV4(pEntry) && pEntry->Saddr_v4 == saddr && pEntry->Daddr_v4 == daddr && pEntry->Sport == sport && pEntry->Dport == dport)
			return pEntry;
	}

	return NULL;
}


PCtEntry IPv4_get_ctentry(U32 saddr, U32 daddr, U16 sport, U16 dport, U16 proto)
{
	U32 hash;
	hash = HASH_CT(saddr, daddr, sport, dport, proto);
	return IPv4_find_ctentry(hash, saddr, daddr, sport, dport);
}


int IPv4_HandleIP_CONNTRACK(U16 *p, U16 Length)
{
	PCtEntry pEntry_orig = NULL, pEntry_rep = NULL;
	CtExCommand Ctcmd;
	
	int i, reset_action = 0;
	U32 sum;
	U32 tmpU32;
	U16 hash_key_ct_orig, hash_key_ct_rep;
	PCtExCommand pCtCmd;
	U8 sa_flow_orig_2dir = 0, sa_flow_rep_2dir = 0;

	// Check length
	if ((Length != sizeof(CtCommand)) && (Length != sizeof(CtExCommand)))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&Ctcmd, (U8*)p,  Length);

	hash_key_ct_orig = HASH_CT(Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport, Ctcmd.protocol);
	hash_key_ct_rep = HASH_CT(Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply, Ctcmd.protocol);
		
	switch(Ctcmd.action)
	{
		case ACTION_DEREGISTER:

				pEntry_orig = IPv4_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport);
				pEntry_rep = IPv4_find_ctentry(hash_key_ct_rep, Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply);
				if (pEntry_orig == NULL || pEntry_rep == NULL ||
					CT_TWIN(pEntry_orig) != pEntry_rep || CT_TWIN(pEntry_rep) != pEntry_orig ||
					((pEntry_orig->status & CONNTRACK_ORIG) != CONNTRACK_ORIG))
					return ERR_CT_ENTRY_NOT_FOUND;

				ct_remove(pEntry_orig, pEntry_rep, hash_key_ct_orig, hash_key_ct_rep);
				break;

		case ACTION_REGISTER:

				/* We first check any possible errors case in the register request (already existing entries, route or arp not found...)
				then conntract entries allocations is performed */ 

				pEntry_orig = IPv4_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport);
				pEntry_rep = IPv4_find_ctentry(hash_key_ct_rep, Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply);

				if (pEntry_orig != NULL && pEntry_rep != NULL && ((pEntry_orig->status & CONNTRACK_ORIG) != CONNTRACK_ORIG))
					return ERR_CREATION_FAILED; // Reverse entry already exists

				if (pEntry_orig != NULL || pEntry_rep != NULL)
					return ERR_CT_ENTRY_ALREADY_REGISTERED; //trying to add exactly the same conntrack

				if (Ctcmd.format & CT_SECURE) {
					if (Ctcmd.SA_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;
					for (i=0;i<Ctcmd.SA_nr;i++) {
						if (M_ipsec_sa_cache_lookup_by_h(Ctcmd.SA_handle[i]) == NULL)
							return ERR_CT_ENTRY_INVALID_SA; 
						}						
					if ((Ctcmd.SA_dir & (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL)) == (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL))
						sa_flow_orig_2dir = 1;

					if (Ctcmd.SAReply_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;
					for (i=0;i<Ctcmd.SAReply_nr;i++) {
						if (M_ipsec_sa_cache_lookup_by_h(Ctcmd.SAReply_handle[i]) == NULL)
							return ERR_CT_ENTRY_INVALID_SA; 
					}	
					if ((Ctcmd.SAReply_dir & (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL)) == (FLOW_DIR_FWD_BITVAL | FLOW_DIR_OUT_BITVAL))
						sa_flow_rep_2dir = 1;
				}

				/* originator ------------------------------------*/
				if ((pEntry_orig = ct_alloc()) == NULL)
				{
					return ERR_NOT_ENOUGH_MEMORY;
				}

				pEntry_orig->Daddr_v4 = Ctcmd.Daddr;
	  			pEntry_orig->Saddr_v4 = Ctcmd.Saddr;
	  			pEntry_orig->Sport = Ctcmd.Sport;
	  			pEntry_orig->Dport = Ctcmd.Dport;
	  			pEntry_orig->twin_Daddr = Ctcmd.DaddrReply;
	  			pEntry_orig->twin_Saddr = Ctcmd.SaddrReply;
	  			pEntry_orig->twin_Sport = Ctcmd.SportReply;
	  			pEntry_orig->twin_Dport = Ctcmd.DportReply;
				pEntry_orig->last_ct_timer = ct_timer;
				pEntry_orig->fwmark = Ctcmd.fwmark & 0xFFFF;
				pEntry_orig->status = CONNTRACK_ORIG;

				if (Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED)
					pEntry_orig->status |= CONNTRACK_FF_DISABLED;

				pEntry_orig->route_id = Ctcmd.route_id;

				if (Ctcmd.format & CT_SECURE) {
					pEntry_orig->status |= CONNTRACK_SEC;
					for (i=0;i < SA_MAX_OP;i++) 
						pEntry_orig->hSAEntry[i] = 
						    (i<Ctcmd.SA_nr) ? Ctcmd.SA_handle[i] : 0;
					if (pEntry_orig->hSAEntry[0])
						pEntry_orig->status &= ~ CONNTRACK_SEC_noSA;
					else 
						pEntry_orig->status |= CONNTRACK_SEC_noSA;
					
					if (sa_flow_orig_2dir)
						pEntry_orig->status |= CONNTRACK_SEC_FLOW_BI_DIR;
						
	  			}

				/* Replier ----------------------------------------*/ 
				pEntry_rep = CT_TWIN(pEntry_orig);

	  			pEntry_rep->Daddr_v4 = Ctcmd.DaddrReply;
	  			pEntry_rep->Saddr_v4 = Ctcmd.SaddrReply;
	  			pEntry_rep->Sport = Ctcmd.SportReply;
	  			pEntry_rep->Dport = Ctcmd.DportReply;
	  			pEntry_rep->twin_Daddr = Ctcmd.Daddr;
	  			pEntry_rep->twin_Saddr = Ctcmd.Saddr;
	  			pEntry_rep->twin_Sport = Ctcmd.Sport;
	  			pEntry_rep->twin_Dport = Ctcmd.Dport;
				if (Ctcmd.fwmark & 0x80000000)
					pEntry_rep->fwmark = ((Ctcmd.fwmark >> 16) & 0x7FFF) | (Ctcmd.fwmark & 0x8000);
				else
					pEntry_rep->fwmark = pEntry_orig->fwmark;
				pEntry_rep->status = 0;
				SET_PROTOCOL(pEntry_orig, pEntry_rep, Ctcmd.protocol);

	  			if(Ctcmd.protocol == IPPROTOCOL_UDP)
					pEntry_rep->last_ct_timer = UDP_REPLY_TIMER_INIT;
				else
					pEntry_rep->last_ct_timer = ct_timer;

				if (Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED)
					pEntry_rep->status |= CONNTRACK_FF_DISABLED;

				pEntry_rep->route_id = Ctcmd.route_id_reply;

				if (Ctcmd.format & CT_SECURE) {
					pEntry_rep->status |= CONNTRACK_SEC;
					for (i=0; i < SA_MAX_OP;i++) 
						pEntry_rep->hSAEntry[i]= 
						    (i<Ctcmd.SAReply_nr) ? Ctcmd.SAReply_handle[i] : 0;
					if ( pEntry_rep->hSAEntry[0])
					       	pEntry_rep->status &= ~CONNTRACK_SEC_noSA;
					else 
						pEntry_rep->status |= CONNTRACK_SEC_noSA;

					if (sa_flow_rep_2dir)
						pEntry_rep->status |= CONNTRACK_SEC_FLOW_BI_DIR;
	  			}

				pEntry_orig->ip_chksm_corr = 0x0001;
				pEntry_rep->ip_chksm_corr = 0x0001;

				/* precompute forward processing (NAT or IP forward?) */
				if((pEntry_orig->Daddr_v4 != pEntry_rep->Saddr_v4)
				|| (pEntry_orig->Sport != pEntry_rep->Dport) 
				|| (pEntry_orig->Dport != pEntry_rep->Sport ) 
				|| (pEntry_orig->Saddr_v4 != pEntry_rep->Daddr_v4))
				{
					U32 Daddr_diff = 0;
					U32 Saddr_diff = 0;
					U32 Sport_diff = 0;
					U32 Dport_diff = 0;

					/* Check sum correction pre-computation RFC1624 */
					
					/* DNAT ? */
					if(pEntry_orig->Daddr_v4 != pEntry_rep->Saddr_v4) 
					{
						Daddr_diff = (pEntry_orig->Daddr_v4 & 0xffff)+
									 (pEntry_orig->Daddr_v4 >> 16) +
									((pEntry_rep->Saddr_v4 & 0xffff) ^ 0xffff) +
									((pEntry_rep->Saddr_v4 >>16)	^ 0xffff);
					}

					/* SNAT ? */
					if(pEntry_orig->Saddr_v4 != pEntry_rep->Daddr_v4)
					{
						Saddr_diff = (pEntry_orig->Saddr_v4 & 0xffff)+
									 (pEntry_orig->Saddr_v4 >> 16) +
									((pEntry_rep->Daddr_v4 & 0xffff) ^ 0xffff) +
									((pEntry_rep->Daddr_v4 >>16)	^ 0xffff);
					}

					/* PDNAT ? */
					if(pEntry_orig->Dport != pEntry_rep->Sport)
					{
						Dport_diff = (pEntry_orig->Dport) +
									((pEntry_rep->Sport) ^ 0xffff);
					}

					/* PSNAT ? */
					if(pEntry_orig->Sport != pEntry_rep->Dport)
					{
						Sport_diff = (pEntry_orig->Sport) +
									((pEntry_rep->Dport) ^ 0xffff);
					}

					/* IP Checksum */
					sum = Daddr_diff + Saddr_diff;
 
					 while (sum>>16)
						sum = (sum & 0xffff)+(sum >> 16);
					if (sum == 0xffff)
						sum = 0;

					tmpU32 = sum + 0x0001;
					if (tmpU32 + 1 >= 0x10000)	// add in carry, and convert 0xFFFF to 0x0000
						tmpU32++;
					pEntry_orig->ip_chksm_corr = tmpU32;

					/* Replier checksum */
					sum = sum == 0 ? 0 : sum ^ 0xffff;
					tmpU32 = sum + 0x0001;
					if (tmpU32 + 1 >= 0x10000)	// add in carry, and convert 0xFFFF to 0x0000
						tmpU32++;
					pEntry_rep->ip_chksm_corr = tmpU32;

					/* UDP/TCP checksum */
					sum = Daddr_diff + Saddr_diff + Dport_diff + Sport_diff;
 
					 while (sum>>16)
						sum = (sum & 0xffff)+(sum >> 16);
					if (sum == 0xffff)
						sum = 0;

					pEntry_orig->tcp_udp_chksm_corr = sum;

					/* Replier checksum  */
					pEntry_rep->tcp_udp_chksm_corr = sum == 0 ? 0 : sum ^ 0xffff;

					/* Set status */
					pEntry_orig->status |= CONNTRACK_NAT;
					pEntry_rep->status  |= CONNTRACK_NAT;
				}

				if ((Ctcmd.format & CT_ORIG_TUNNEL_4O6) && !(Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED))
				{
					pEntry_orig->tnl_route = L2_route_get(Ctcmd.tunnel_route_id);
					if (IS_NULL_ROUTE(pEntry_orig->tnl_route))
					{
						ct_free((PCtEntry)pEntry_orig);
						return ERR_RT_LINK_NOT_POSSIBLE;
					}
					pEntry_orig->status |= CONNTRACK_4O6;	
			  	}

				if ((Ctcmd.format & CT_REPL_TUNNEL_4O6) && !(Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED))
				{
					pEntry_rep->tnl_route = L2_route_get(Ctcmd.tunnel_route_id_reply);
					if (IS_NULL_ROUTE(pEntry_rep->tnl_route))
					{
						L2_route_put(pEntry_orig->tnl_route);
						ct_free((PCtEntry)pEntry_orig);
						return ERR_RT_LINK_NOT_POSSIBLE;
					}
					pEntry_rep->status |= CONNTRACK_4O6;	
			  	}



				/* Everything went Ok. We can safely put querier and replier entries in hash tables */

				/* add orig+replier to head of hash table list */
				return ct_add(pEntry_orig, pEntry_rep, hash_key_ct_orig, hash_key_ct_rep);

		case ACTION_UPDATE: 

				pEntry_orig = IPv4_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport);
				pEntry_rep = IPv4_find_ctentry(hash_key_ct_rep, Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply);
				if (pEntry_orig == NULL || pEntry_rep == NULL ||
					CT_TWIN(pEntry_orig) != pEntry_rep || CT_TWIN(pEntry_rep) != pEntry_orig ||
					((pEntry_orig->status & CONNTRACK_ORIG) != CONNTRACK_ORIG))
					return ERR_CT_ENTRY_NOT_FOUND;

				if ((Ctcmd.format & CT_ORIG_TUNNEL_4O6) && !(Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED))
				{
					PRouteEntry tnl_route;
					tnl_route = L2_route_get(Ctcmd.tunnel_route_id);  
					if (IS_NULL_ROUTE(tnl_route))
						return ERR_RT_LINK_NOT_POSSIBLE;
					L2_route_put(tnl_route);			 
			  	}
				if ((Ctcmd.format & CT_REPL_TUNNEL_4O6) && !(Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED))
				{
					PRouteEntry tnl_route;
					tnl_route = L2_route_get(Ctcmd.tunnel_route_id_reply);  
					if (IS_NULL_ROUTE(tnl_route))
						return ERR_RT_LINK_NOT_POSSIBLE;
					L2_route_put(tnl_route);			  
			  	}
	
				if (Ctcmd.format & CT_SECURE) {
					if (Ctcmd.SA_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;

					for (i = 0; i < Ctcmd.SA_nr; i++) {
						if (pEntry_orig->hSAEntry[i] != Ctcmd.SA_handle[i])
							if (M_ipsec_sa_cache_lookup_by_h( Ctcmd.SA_handle[i]) == NULL)
								return ERR_CT_ENTRY_INVALID_SA; 
					}

					if (Ctcmd.SAReply_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;

					if ((Ctcmd.SA_dir & (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL)) == (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL))
						sa_flow_orig_2dir = 1;

					for (i = 0; i < Ctcmd.SAReply_nr; i++) {
						if (pEntry_rep->hSAEntry[i] != Ctcmd.SAReply_handle[i])
							if (M_ipsec_sa_cache_lookup_by_h(Ctcmd.SAReply_handle[i]) == NULL)
								return ERR_CT_ENTRY_INVALID_SA;
					}
					if ((Ctcmd.SAReply_dir & (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL)) == (FLOW_DIR_FWD_BITVAL | FLOW_DIR_OUT_BITVAL))
						sa_flow_rep_2dir = 1;
				}

				pEntry_orig->fwmark = Ctcmd.fwmark & 0xFFFF;

				if (Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED) {
					pEntry_orig->status |= CONNTRACK_FF_DISABLED;
					IP_delete_CT_route(pEntry_orig);
				} else
					pEntry_orig->status &= ~CONNTRACK_FF_DISABLED;

				if (Ctcmd.format & CT_SECURE) {
					pEntry_orig->status |= CONNTRACK_SEC;

					for (i = 0;i < SA_MAX_OP; i++)
						pEntry_orig->hSAEntry[i] = 
						    (i<Ctcmd.SA_nr) ? Ctcmd.SA_handle[i] : 0;

					if (pEntry_orig->hSAEntry[0])
						pEntry_orig->status &= ~ CONNTRACK_SEC_noSA;
					else 
						pEntry_orig->status |= CONNTRACK_SEC_noSA;

					if (sa_flow_orig_2dir)
						pEntry_orig->status |= CONNTRACK_SEC_FLOW_BI_DIR;
					else
						pEntry_orig->status &= ~ CONNTRACK_SEC_FLOW_BI_DIR;
				} else
					pEntry_orig->status &= ~CONNTRACK_SEC;

				if (Ctcmd.fwmark & 0x80000000)
					pEntry_rep->fwmark = ((Ctcmd.fwmark >> 16) & 0x7FFF) | (Ctcmd.fwmark & 0x8000);
				else
					pEntry_rep->fwmark = pEntry_orig->fwmark;

				if (Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED) {
					pEntry_rep->status |= CONNTRACK_FF_DISABLED;
					IP_delete_CT_route(pEntry_rep);
				} else
					pEntry_rep->status &= ~CONNTRACK_FF_DISABLED;

				if (Ctcmd.format & CT_SECURE) {
					pEntry_rep->status |= CONNTRACK_SEC;

					for (i = 0; i < SA_MAX_OP; i++) 
						pEntry_rep->hSAEntry[i]= 
						    (i<Ctcmd.SAReply_nr) ? Ctcmd.SAReply_handle[i] : 0;

					if ( pEntry_rep->hSAEntry[0])
						pEntry_rep->status &= ~CONNTRACK_SEC_noSA;
					else 
						pEntry_rep->status |= CONNTRACK_SEC_noSA;

					if (sa_flow_rep_2dir)
						pEntry_rep->status |= CONNTRACK_SEC_FLOW_BI_DIR;
					else
						pEntry_rep->status &= ~ CONNTRACK_SEC_FLOW_BI_DIR;
				} else
					pEntry_rep->status &= ~CONNTRACK_SEC;

				/* Update route entries if needed */
				if (IS_NULL_ROUTE(pEntry_orig->pRtEntry))
				{
					pEntry_orig->route_id = Ctcmd.route_id;
				}
				else if (pEntry_orig->pRtEntry->id != Ctcmd.route_id)
				{
					IP_delete_CT_route(pEntry_orig);
					pEntry_orig->route_id = Ctcmd.route_id;
				}

				if (IS_NULL_ROUTE(pEntry_rep->pRtEntry))
				{
					pEntry_rep->route_id = Ctcmd.route_id_reply;
				}
				else if (pEntry_rep->pRtEntry->id != Ctcmd.route_id_reply)
				{
					IP_delete_CT_route(pEntry_rep);
					pEntry_rep->route_id = Ctcmd.route_id_reply;
				}

				if (pEntry_orig->tnl_route)
				{
					L2_route_put(pEntry_orig->tnl_route);
					pEntry_orig->tnl_route = NULL;
                                        pEntry_orig->status &= ~CONNTRACK_4O6;
				}
				if ((Ctcmd.format & CT_ORIG_TUNNEL_4O6) && !(Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED))
				{
					pEntry_orig->tnl_route = L2_route_get(Ctcmd.tunnel_route_id);
					pEntry_orig->status |= CONNTRACK_4O6;	
			  	}

				if (pEntry_rep->tnl_route)
				{
					L2_route_put(pEntry_rep->tnl_route);
					pEntry_rep->tnl_route = NULL;
					pEntry_rep->status &= ~CONNTRACK_4O6;

				}
				if ((Ctcmd.format & CT_REPL_TUNNEL_4O6) && !(Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED))
				{
					pEntry_rep->tnl_route = L2_route_get(Ctcmd.tunnel_route_id_reply);
					pEntry_rep->status |= CONNTRACK_4O6;	
			  	}


				ct_update(pEntry_orig, pEntry_rep, hash_key_ct_orig, hash_key_ct_rep);
				return NO_ERR;

		case ACTION_QUERY:
			reset_action = 1;

		case ACTION_QUERY_CONT:
		{
			int rc;

			pCtCmd = (PCtExCommand)p;
			rc = IPv4_Get_Next_Hash_CTEntry(pCtCmd, reset_action);

			return rc;
		}
		
		default :
                        return ERR_UNKNOWN_ACTION;
	  	
	} 

	return NO_ERR;
}


int IP_HandleIP_ROUTE_RESOLVE (U16 *p, U16 Length)
{
	PRouteEntry pRtEntry;
	RtCommand RtCmd;
	PRtCommand pRtCmd;
	int rc = NO_ERR, reset_action = 0;
	POnifDesc onif_desc;
	
	// Check length
	if (Length != sizeof(RtCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RtCmd, (U8*)p, sizeof(RtCommand));

	pRtEntry = L2_route_find(RtCmd.id);

	switch(RtCmd.action)
	{
		case ACTION_REGISTER:
			if (pRtEntry)
				return ERR_RT_ENTRY_ALREADY_REGISTERED; //trying to add exactly the same route

			onif_desc = get_onif_by_name(RtCmd.outputDevice);
			if (!onif_desc)
				return ERR_UNKNOWN_INTERFACE;

			/* This is no longer required for C2K but for C1K inorder to 
			   acoomodate most routes in the ARAM, the size of the route 
			   entry needs to be conditionally increased.*/
			#if !defined(COMCERTO_2000) 
			if (RtCmd.flags & (RTCMD_FLAGS_6o4 | RTCMD_FLAGS_4o6))
				pRtEntry = L2_route_add(RtCmd.id,16);
			else 
			#endif
				pRtEntry = L2_route_add(RtCmd.id, 0);

			if (!pRtEntry)
				return ERR_NOT_ENOUGH_MEMORY;

			pRtEntry->itf = onif_desc->itf;

			if (pRtEntry->itf->type & IF_TYPE_PPPOE)
				COPY_MACADDR(pRtEntry->dstmac, ((pPPPoE_Info)pRtEntry->itf)->DstMAC);
			else
				COPY_MACADDR(pRtEntry->dstmac, RtCmd.macAddr);

			rte_set_mtu(pRtEntry, RtCmd.mtu);

			if (RtCmd.flags & RTCMD_FLAGS_6o4)
				*((U32 *)ROUTE_EXTRA_INFO(pRtEntry)) = RtCmd.daddr[0];
			else if (RtCmd.flags & RTCMD_FLAGS_4o6)
				SFL_memcpy(ROUTE_EXTRA_INFO(pRtEntry), RtCmd.daddr, IPV6_ADDRESS_LENGTH);
			break;

		case ACTION_UPDATE:
			if (!pRtEntry)
				return ERR_RT_ENTRY_NOT_FOUND;

			onif_desc = get_onif_by_name(RtCmd.outputDevice);
			if (!onif_desc)
				return ERR_UNKNOWN_INTERFACE;

			pRtEntry->itf = onif_desc->itf;

			if (pRtEntry->itf->type & IF_TYPE_PPPOE)
				COPY_MACADDR(pRtEntry->dstmac, ((pPPPoE_Info)pRtEntry->itf)->DstMAC);
			else
				COPY_MACADDR(pRtEntry->dstmac, RtCmd.macAddr);
			rte_set_mtu(pRtEntry, RtCmd.mtu);

			break;

		case ACTION_DEREGISTER:
			rc = L2_route_remove(RtCmd.id);

			break;

		case ACTION_QUERY:
			reset_action = 1;

		/* fallthrough */
		case ACTION_QUERY_CONT:

			pRtCmd = (PRtCommand)p;
			rc = IPV4_Get_Next_Hash_RtEntry(pRtCmd, reset_action);

			break;
		default:
			rc =  ERR_UNKNOWN_ACTION;
			break;
	}  
 	
  	return rc;
}



static int IPv4_HandleIP_RESET (void)
{
	PRouteEntry pRtEntry = NULL;
	int i;
	int rc = NO_ERR;

	/* free Conntrack entries -- this handles both IPv4 and IPv6 */
	for(i = 0; i < NUM_CT_ENTRIES; i++)
	{
		PCtEntry pEntry_orig, pEntry_rep;
		struct slist_entry *entry;

		while ((entry = slist_first(&ct_cache[i])) != NULL)
		{
			pEntry_orig = CT_ORIG(container_of(entry, CtEntry, list));

			if (IS_IPV4(pEntry_orig))
				pEntry_rep = CT_TWIN(pEntry_orig);
			else
				pEntry_rep = (PCtEntry)CT6_TWIN(pEntry_orig);

			ct_remove(pEntry_orig, pEntry_rep, Get_Ctentry_Hash(pEntry_orig), Get_Ctentry_Hash(pEntry_rep));
		}
	}

	/* free IPv4 sockets entries */
	SOCKET4_free_entries();

	/* Do IPv6 reset */
	IPv6_handle_RESET();

	/* free all Route entries */
	for(i = 0; i < NUM_ROUTE_ENTRIES; i++)
	{
		struct slist_entry *entry;

		slist_for_each_safe(pRtEntry, entry, &rt_cache[i], list)
		{
			L2_route_remove(pRtEntry->id);
		}
	}

#if !defined(COMCERTO_2000_CONTROL)
	memset(sockid_cache, 0, sizeof(PVOID) * NUM_SOCK_ENTRIES);
#endif

	return rc;
}


static int IPv4_HandleIP_SET_TIMEOUT (U16 *p, U16 Length)
{
	TimeoutCommand TimeoutCmd;
	int rc = NO_ERR;

	// Check length
	if (Length != sizeof(TimeoutCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&TimeoutCmd, (U8*)p, sizeof(TimeoutCommand));

	// Check protocol and update timeout value
	switch(TimeoutCmd.protocol)
	{
		case IPPROTOCOL_TCP:
			if(TimeoutCmd.sam_4o6_timeout)
				tcp_4o6_timeout = TimeoutCmd.timeout_value1 * CT_TICKS_PER_SECOND;
			else
				tcp_timeout = TimeoutCmd.timeout_value1 * CT_TICKS_PER_SECOND;
			break;
		case IPPROTOCOL_UDP:
			if(TimeoutCmd.sam_4o6_timeout)
			{
				udp_4o6_bidir_timeout = TimeoutCmd.timeout_value1 * CT_TICKS_PER_SECOND;
				udp_4o6_unidir_timeout = (TimeoutCmd.timeout_value2 > 0 ? TimeoutCmd.timeout_value2 : TimeoutCmd.timeout_value1) * CT_TICKS_PER_SECOND;
			}
			else
			{
				udp_bidir_timeout = TimeoutCmd.timeout_value1 * CT_TICKS_PER_SECOND;
				udp_unidir_timeout = (TimeoutCmd.timeout_value2 > 0 ? TimeoutCmd.timeout_value2 : TimeoutCmd.timeout_value1) * CT_TICKS_PER_SECOND;
			}
			break;
		#define UNKNOWN_PROTO 0
		case UNKNOWN_PROTO:
			if(TimeoutCmd.sam_4o6_timeout)
				other_4o6_proto_timeout = TimeoutCmd.timeout_value1 * CT_TICKS_PER_SECOND;
			else
				other_proto_timeout = TimeoutCmd.timeout_value1 * CT_TICKS_PER_SECOND;
			break;
	       default:
			rc = ERR_UNKNOWN_ACTION;
			break;
	}
	return rc;
}


static int IPv4_HandleIP_Get_Timeout(U16 *p, U16 Length)
{
	int rc = NO_ERR;
	PTimeoutCommand TimeoutCmd;
	CtCommand 	Ctcmd;
	U32 			hash_key_ct_orig;
	PCtEntry		pEntry, twin_entry;
	// Check length
	if (Length != sizeof(CtCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&Ctcmd, (U8*)p,  Length);

	hash_key_ct_orig = HASH_CT(Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport, Ctcmd.protocol);

	if ((pEntry = IPv4_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport)) != NULL)
	{
		BOOL bidir_flag;
		int timeout_value;
		U32 current_timer, twin_timer;
		memset(p, 0, 256);
		TimeoutCmd = (PTimeoutCommand)(p+1);
		TimeoutCmd->protocol = GET_PROTOCOL(pEntry);
		twin_entry = CT_TWIN(pEntry);
		current_timer = ct_timer - pEntry->last_ct_timer;
		bidir_flag = twin_entry->last_ct_timer != UDP_REPLY_TIMER_INIT;
		if (bidir_flag)
		{
			twin_timer = ct_timer - twin_entry->last_ct_timer;
			if (twin_timer < current_timer)
				current_timer = twin_timer;
		}
		timeout_value = GET_TIMEOUT_VALUE(pEntry, bidir_flag) - current_timer;
		if (timeout_value < 0)
			timeout_value = 0;
		TimeoutCmd->timeout_value1 = (U32)timeout_value / CT_TICKS_PER_SECOND;
	}
	else
	{
		return CMD_ERR;
	}
	
	return rc;			
}


static int IPv4_HandleIP_FF_CONTROL (U16 *p, U16 Length)
{
	int rc = NO_ERR;
	FFControlCommand FFControlCmd;

	// Check length
	if (Length != sizeof(FFControlCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&FFControlCmd, (U8*)p, sizeof(FFControlCommand));

	if(FFControlCmd.enable == 1){
		PCtEntry ct;
		int i;

		ff_enable = 1;

		/* Reset all timeouts */
		for(i = 0; i < NUM_CT_ENTRIES; i++)
		{
			struct slist_entry *entry;

			slist_for_each(ct, entry, &ct_cache[i], list)
			{
				if(ct->last_ct_timer != UDP_REPLY_TIMER_INIT)
					ct->last_ct_timer = ct_timer;
			}
		}
	}
	else if (FFControlCmd.enable == 0){
		ff_enable = 0;
	}
	else
		return ERR_WRONG_COMMAND_PARAM;
	
	return rc;
}



static U16 M_ipv4_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 querySize = 0;
  	U16 action;

	switch (cmd_code)
	{
		case CMD_IPV4_CONNTRACK:			
			action = *pcmd;
			rc = IPv4_HandleIP_CONNTRACK(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				querySize = sizeof(CtExCommand);
			break;
			
		case CMD_IP_ROUTE:
			action = *pcmd;
			rc = IP_HandleIP_ROUTE_RESOLVE(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				querySize = sizeof(RtCommand);
			break;

		case CMD_IPV4_RESET:			
			rc = IPv4_HandleIP_RESET();
			break;

		case CMD_IPV4_SET_TIMEOUT:
			rc = IPv4_HandleIP_SET_TIMEOUT(pcmd, cmd_len);
			break;

		case CMD_IPV4_GET_TIMEOUT:
			rc = IPv4_HandleIP_Get_Timeout(pcmd, cmd_len);
			if (rc == NO_ERR)
				querySize = sizeof(TimeoutCommand);
			break;

		/* IPv4 module is used to handle alternate configuration API */
		case CMD_ALTCONF_SET:
			rc = ALTCONF_HandleCONF_SET(pcmd, cmd_len);
			break;
							
		case CMD_ALTCONF_RESET:
			rc = ALTCONF_HandleCONF_RESET_ALL(pcmd, cmd_len);
			break;

		case CMD_IPV4_FF_CONTROL:
			rc = IPv4_HandleIP_FF_CONTROL(pcmd, cmd_len);
			break;

		case CMD_IPV4_SOCK_OPEN:
			rc = SOCKET4_HandleIP_Socket_Open(pcmd, cmd_len);
			break;

		case CMD_IPV4_SOCK_CLOSE:
			rc = SOCKET4_HandleIP_Socket_Close(pcmd, cmd_len);
			break;

		case CMD_IPV4_SOCK_UPDATE:
			rc = SOCKET4_HandleIP_Socket_Update(pcmd, cmd_len);
			break;

		case CMD_IPV4_FRAGTIMEOUT:
		case CMD_IPV4_SAM_FRAGTIMEOUT:
			rc = IPv4_HandleIP_Set_FragTimeout(pcmd, cmd_len, (cmd_code == CMD_IPV4_SAM_FRAGTIMEOUT));
			break;

#ifdef CFG_DIAGS
		/* IPv4 module is used to handle fppDiag APIs */
		case CMD_FPPDIAG_ENABLE:
			rc = fppdiag_enable(pcmd, cmd_len);
			break;
			
		case CMD_FPPDIAG_DISABLE:
			rc = fppdiag_disable();
			break;
			
		case CMD_FPPDIAG_UPDATE:
			rc = fppdiag_update(pcmd, cmd_len);
			break;

		case CMD_FPPDIAG_DUMP_CTRS:
			rc = fppdiag_dump_counters();
			break;
#endif
		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;
	return 2 + querySize;
}


#if !defined(COMCERTO_2000)
static void IPv4_Init_Fast_CTData(void)
{
	int i;
	U8 *p;
	pFast_CTData_freelist = Fast_CTData;
	for (i = 0, p = Fast_CTData; i < NUM_FAST_CTDATA - 1; i++, p += FAST_CTDATA_ENTRY_SIZE)
		*(U8 **)p = p + FAST_CTDATA_ENTRY_SIZE;
	*(U8 **)p = NULL;
}
#endif


int ipv4_init(void)
{
#if !defined(COMCERTO_2000)
	/* Init Fast CTData pool */
	IPv4_Init_Fast_CTData();
	set_event_handler(EVENT_IPV4, M_ipv4_entry);
#else
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int i;

	dlist_head_init(&ct_removal_list);

	for (i = 0; i < NUM_CT_ENTRIES; i++)
	{
		struct hw_ct *ct = ctrl->hash_array_baseaddr + i * CLASS_ROUTE_SIZE;

		ct->dma_addr = cpu_to_be32(ctrl->hash_array_phys_baseaddr + i * CLASS_ROUTE_SIZE);
		dlist_head_init(&ct->list);
		hw_ct_set_next(ct, 0);
		hw_ct_set_flags(ct, 0);
	}
#endif

	set_cmd_handler(EVENT_IPV4, M_ipv4_cmdproc);

#if defined(COMCERTO_1000)
	timer_init(&ipsec_ratelimiter_timer, AltConfIpsec_RateLimit_token_generator);
#endif

	/* register ipv4 module to the timer service with 1 second granularity*/
	timer_init(&ip_timer, M_ip_timer);
	timer_add(&ip_timer, CT_TIMER_INTERVAL);

	/* Set default values to programmable L4 timeouts */
	udp_4o6_unidir_timeout =	udp_unidir_timeout = UDP_UNIDIR_TIMEOUT * CT_TICKS_PER_SECOND;
	udp_4o6_bidir_timeout  =	udp_bidir_timeout = UDP_BIDIR_TIMEOUT * CT_TICKS_PER_SECOND;
	tcp_4o6_timeout	       =	tcp_timeout = TCP_TIMEOUT * CT_TICKS_PER_SECOND;
	other_4o6_proto_timeout=	other_proto_timeout = OTHER_PROTO_TIMEOUT * CT_TICKS_PER_SECOND;

	return 0;
}


void ipv4_exit(void)
{
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct dlist_head *entry;
	struct hw_ct *ct;
#endif

	timer_del(&ip_timer);

	/* Just call IPv4_HandleIP_RESET */
	IPv4_HandleIP_RESET();

#if defined(COMCERTO_2000)
	/* class pe's must be stopped by now, remove all pending entries */
	dlist_for_each_safe(ct, entry, &ct_removal_list, rlist)
	{
		dlist_remove(&ct->rlist);

		if (!IS_HASH_ARRAY(ctrl, ct))
		{
			dma_pool_free(ctrl->dma_pool_128, ct, be32_to_cpu(ct->dma_addr));
		}
	}
#endif
}
