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


#ifndef _MODULE_MC6_H_
#define _MODULE_MC6_H_

#include "types.h"
#include "modules.h"
#include "channels.h"
#include "multicast.h"
#include "fe.h"
#include "module_ipv6.h"

#define MC6_MAX_LISTENERS_PER_GROUP		MCx_MAX_LISTENERS_PER_GROUP

#define MC6_NUM_HASH_ENTRIES 16

typedef struct tMC6_context {

	unsigned char hoplimit_check_rule;
}MC6_context;

#if defined (COMCERTO_2000)

#if defined (COMCERTO_2000_CONTROL)

/** Control path MC6 HW entry */
typedef struct _thw_MC6Entry {
	U32 	flags;
	U32 	dma_addr;
	U32 next;
	U32 src_addr[4];
	U32 dst_addr[4];
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U8 rtpqos_slot;

	/* These fields are only used by host software, so keep them at the end of the structure */
	struct dlist_head 	list;
	struct _tMC6Entry	*sw_entry;	/**< pointer to the software flow entry */
	unsigned long		removal_time;
}hw_MC6Entry, *Phw_MC6Entry;

/** Control path MC6 SW entry */
typedef struct _tMC6Entry {
	struct slist_entry list;
	U32 src_addr[4];
	U32 dst_addr[4];
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U16 rtpqos_ref_count;
	U8 rtpqos_slot;

	hw_MC6Entry *hw_entry;
}MC6Entry, *PMC6Entry;


#else
/** Data path MC6 entry */
typedef struct _tMC6Entry {
	U32 	flags;
	U32 	dma_addr;
	U32 	next;
	U32 src_addr[4];
	U32 dst_addr[4];
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U8 rtpqos_slot;
}MC6Entry, *PMC6Entry;
#endif

#else /* COMCERTO 1000 */
typedef struct _tMC6Entry {
	struct slist_entry list;
	U32 src_addr[4];
	U32 dst_addr[4];
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U8 rtpqos_slot;
	U16 rtpqos_ref_count;
}MC6Entry, *PMC6Entry;
#endif



/***********************************
* MC4 API Command and Entry structures
*
***********************************/
typedef struct _tMC6Output {
	U32		timer;
	U8		output_device_str[11];
	U8 		shaper_mask;
	U8              uc_bit:1,
                        q_bit:1,
                        rsvd:6;
	U8              uc_mac[6];
	U8              queue;
	U8		new_output_device_str[11];
	U8		if_bit:1,
			unused:7;
}MC6Output, *PMC6Output;

typedef struct _tMC6Command {
	U16		action;
	U8 		mode : 1,
	     		queue : 5,
	     		rsvd : 2;
	U8		src_mask_len;
	U32		src_addr[4];
	U32		dst_addr[4];
	U32		num_output;
	MC6Output output_list[MCx_MAX_LISTENERS_IN_QUERY];
}MC6Command, *PMC6Command;
#define MC6_MIN_COMMAND_SIZE	64 /* with one listener entry using 1 interface name */


extern struct tMC6_context gMC6Ctx;
#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
extern struct slist_head mc6_table_memory[MC6_NUM_HASH_ENTRIES];
#else
extern PVOID mc6_table_memory[MC6_NUM_HASH_ENTRIES];
#endif

BOOL mc6_init(void);
void mc6_exit(void);
void M_MC6_process_packet(PMetadata mtd) __attribute__((section ("fast_path")));
#if !defined(COMCERTO_2000)
void M_mc6_entry(void) __attribute__((section ("fast_path")));
#endif

#if defined(COMCERTO_2000)
void MC6_entry_update(PMC6Entry this_entry);
#endif

void MC6_interface_purge(U32 if_index);

static inline u32 HASH_MC6(void *pdestaddr)  // pass in ptr to IPv6 dest addr
{
	u16 *p = (u16 *)pdestaddr;
	u32 hash;
	hash = ntohs(p[4]) + ntohs(p[5]) + ntohs(p[6]) + ntohs(p[7]);
	hash = hash ^ (hash >> 4) ^ (hash >> 8) ^ (hash >> 12);
	return hash & (MC6_NUM_HASH_ENTRIES - 1);
}

#if defined(COMCERTO_2000_CLASS)
/** Searches multicast forwarding table for a match based on a given IPv6 header.
 * @param[in] ipv6_hdr	pointer to IPv6 header to be used for the search.
 * @return pointer to matching entry, or NULL if no entry found.
 */
static __inline MC6Entry *MC6_rule_search_class(ipv6_hdr_t *ipv6_hdr)
{
	PMC6Entry dmem_entry;
	volatile PMC6Entry ddr_entry;
	U32 *srca = ipv6_hdr->SourceAddress;
	U32 *dsta = ipv6_hdr->DestinationAddress;
	U32 hash_key;

	hash_key = HASH_MC6(dsta);

	ddr_entry = mc6_table_memory[hash_key];
	dmem_entry = (PVOID)(CLASS_ROUTE0_BASE_ADDR);
	while (ddr_entry) {
		efet_memcpy(dmem_entry, ddr_entry, sizeof(MC6Entry));
		while (dmem_entry->flags & HASH_ENTRY_UPDATING)
		{
			while (ddr_entry->flags & HASH_ENTRY_UPDATING) ;
			efet_memcpy(dmem_entry, ddr_entry, sizeof(MC6Entry));
		}

		if (!IPV6_CMP(dsta, dmem_entry->dst_addr))
		{
			if (ipv6_cmp_mask(srca, (U32*)dmem_entry->src_addr, dmem_entry->mcdest.src_mask_len)) {
				return dmem_entry;
			}
		}
		ddr_entry = (PMC6Entry) dmem_entry->next;
	}
  	return NULL;
}
#else
/** Searches multicast forwarding table for match based on source and destination IPv6 addresses.
 * @param[in] srca	IPv6 source address
 * @param[in] dsta	IPv6 destination address
 * @return pointer to matching entry, or NULL if no entry found.
 */
static __inline MC6Entry *MC6_rule_search(U32 *srca, U32 *dsta)
{
	PMC6Entry this_entry;
	struct slist_entry *entry;
	U32 hash_key;
#define _l_src	((PACKED_U32 *)(srca))
#define _l_dst	((PACKED_U32 *)(dsta))

	hash_key = HASH_MC6(_l_dst);
	slist_for_each(this_entry, entry, &mc6_table_memory[hash_key], list)
	{
	  if (!IPV6_CMP(_l_dst,this_entry->dst_addr))
	    {
	      if (ipv6_cmp_mask((U32*) _l_src, (U32*)this_entry->src_addr, this_entry->mcdest.src_mask_len)) {
		return this_entry;
	      }
	    }
	}
#undef _l_src
#undef _l_dst
  	return NULL;
}
#endif

#endif /* _MODULE_MC6_H_ */
