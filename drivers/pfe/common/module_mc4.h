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


#ifndef _MODULE_MC4_H_
#define _MODULE_MC4_H_

#include "types.h"
#include "modules.h"
#include "channels.h"
#include "multicast.h"

#define MC4_MAX_LISTENERS_PER_GROUP MCx_MAX_LISTENERS_PER_GROUP 

#define MC4_NUM_HASH_ENTRIES 16

#define INVALID_IDX (-1)
typedef struct tMC4_context {

  	unsigned char ttl_check_rule;
}MC4_context;


#if defined (COMCERTO_2000)

#if defined (COMCERTO_2000_CONTROL)

/** Control path MC4 HW entry */
typedef struct _thw_MC4Entry {
	U32 	flags;
	U32 	dma_addr;
	U32 next;
	U32 src_addr;
	U32 dst_addr;
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U8 rtpqos_slot;

	/* These fields are only used by host software, so keep them at the end of the structure */
	struct dlist_head 	list;
	struct _tMC4Entry	*sw_entry;	/**< pointer to the software flow entry */
	unsigned long		removal_time;
}hw_MC4Entry, *Phw_MC4Entry;

/** Control path MC4 SW entry */
typedef struct _tMC4Entry {
	struct slist_entry list;
	U32 src_addr;
	U32 dst_addr;
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U16 rtpqos_ref_count;
	U8 rtpqos_slot;

	hw_MC4Entry *hw_entry;
}MC4Entry, *PMC4Entry;


#else
/** Data path MC4 entry */
typedef struct _tMC4Entry {
	U32 	flags;
	U32 	dma_addr;
	U32 	next;
	U32 src_addr;
	U32 dst_addr;
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U8 rtpqos_slot;
}MC4Entry, *PMC4Entry;
#endif

#else /* COMCERTO 1000 */
typedef struct _tMC4Entry {
	struct slist_entry list;
	U32 src_addr;
	U32 dst_addr;
	/* common portion */
	MCxEntry mcdest;
	U16 status;
	U8 rtpqos_slot;
	U16 rtpqos_ref_count;
}MC4Entry, *PMC4Entry;
#endif



/***********************************
* MC4 API Command and Entry structures
*
************************************/
typedef struct _tMC4Output {
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
}MC4Output, *PMC4Output;

typedef struct _tMC4Command {
	U16		action;
	U8		src_addr_mask;
	U8 		mode : 1,
	     		queue : 5,
	     		rsvd : 2;
	U32		src_addr;
	U32		dst_addr;
	U32		num_output;
	MC4Output output_list[MCx_MAX_LISTENERS_IN_QUERY];
}MC4Command, *PMC4Command;
#define MC4_MIN_COMMAND_SIZE	32 /* with one listener entry using 1 interface name */


extern struct tMC4_context gMC4Ctx;
#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
extern struct slist_head mc4_table_memory[MC4_NUM_HASH_ENTRIES];
#else
extern PVOID mc4_table_memory[MC4_NUM_HASH_ENTRIES];
#endif

BOOL mc4_init(void);
void mc4_exit(void);
void M_MC4_process_packet(PMetadata mtd) __attribute__((section ("fast_path")));
#if !defined(COMCERTO_2000)
void M_mc4_entry(void) __attribute__((section ("fast_path")));
#endif

void MC4_interface_purge(U32 if_index);

int MC4_handle_MULTICAST(U16 *p, U16 Length);
int MC4_handle_RESET (void);
int MC4_check_entry(struct tMetadata *mtd);

#if defined(COMCERTO_2000)
void MC4_entry_update(PMC4Entry this_entry);
#endif

static inline u32 HASH_MC4(u32 destaddr)  // pass in IPv4 dest addr
{
	u32 hash;
	destaddr = ntohl(destaddr);
	hash = destaddr + (destaddr >> 16);
	hash = hash ^ (hash >> 4) ^ (hash >> 8) ^ (hash >> 12);
	return hash & (MC4_NUM_HASH_ENTRIES - 1);
}

#ifdef COMCERTO_2000_CLASS
/** Searches multicast forwarding table for match based on source and destination IPv4 addresses.
 * @param[in] srca	IPv4 source address
 * @param[in] dsta	IPv4 destination address
 * @return pointer to matching entry, or NULL if no entry found.
 */
static __inline MC4Entry *MC4_rule_search_class(ipv4_hdr_t *ipv4_hdr)
{
	PMC4Entry dmem_entry;
	volatile PMC4Entry ddr_entry;
	U32 srca = READ_UNALIGNED_INT(ipv4_hdr->SourceAddress);
	U32 dsta = READ_UNALIGNED_INT(ipv4_hdr->DestinationAddress);
	U32 hash_key = HASH_MC4(dsta);

	ddr_entry = mc4_table_memory[hash_key];
	dmem_entry = (PVOID)(CLASS_ROUTE0_BASE_ADDR);
	while (ddr_entry) {
		efet_memcpy(dmem_entry, ddr_entry, sizeof(MC4Entry));
		while (dmem_entry->flags & HASH_ENTRY_UPDATING)
		{
			while (ddr_entry->flags & HASH_ENTRY_UPDATING) ;
			efet_memcpy(dmem_entry, ddr_entry, sizeof(MC4Entry));
		}

		if (dsta == dmem_entry->dst_addr)
		{
			// We have to check for a null mask because the shift instruction is limited to 31 bits on esiRISC
			if ((dmem_entry->mcdest.src_mask_len == 0) || !(ntohl(srca ^  dmem_entry->src_addr)>>(32 - dmem_entry->mcdest.src_mask_len))) {
				return dmem_entry;
			}
		}
		ddr_entry = (PMC4Entry) dmem_entry->next;
	}

	return NULL;
}
#else
/** Searches multicast forwarding table for match based on source and destination IPv4 addresses.
 * @param[in] srca	IPv4 source address
 * @param[in] dsta	IPv4 destination address
 * @return pointer to matching entry, or NULL if no entry found.
 */
static __inline MC4Entry *MC4_rule_search(U32 srca, U32 dsta)
{
	PMC4Entry this_entry;
	struct slist_entry *entry;
	U32 hash_key;

	hash_key = HASH_MC4(dsta);
	slist_for_each(this_entry, entry, &mc4_table_memory[hash_key], list)
	{
	  	if (dsta == this_entry->dst_addr)
	    	{
	      		if (!(ntohl(srca ^  this_entry->src_addr)>>(32 - this_entry->mcdest.src_mask_len))) {
				return this_entry;
	      		}
	    	}
	}

  	return NULL;
}
#endif

#endif /* _MODULE_MC4_H_ */
