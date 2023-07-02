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



#ifndef _MODULE_IPV6FRAG_H_
#define _MODULE_IPV6FRAG_H_

#include "types.h"
#include "fpart.h"
#include "channels.h"
#include "module_timer.h"
#include "module_ipv6.h"

#define NUM_FRAG_ENTRIES	128

#define FRAG_DROP	(1 << 2)
#define FRAG_FIRST_IN	(1 << 1)
#define FRAG_LAST_IN	(1 << 0)

#if defined (COMCERTO_2000)
#define SKB_SHARED_INFO_SIZE	256
#define FPP_MAX_IP_FRAG	(DDR_BUF_SIZE - (DDR_HDR_SIZE/2) - SKB_SHARED_INFO_SIZE) //2k DDR buffer - 128 head room
#else
#define FPP_MAX_IP_FRAG	1628 // 4Kb buffer - Linux skb_shared (164) - max stagger offset (2240)  - Baseoffset (64)
#endif
#define FPP_MAX_FRAG_NUM	2

#define IP_FRAG_TIMER_INTERVAL		(1 * TIMER_TICKS_PER_SEC)
#define FRAG_TIMEOUT			(10 * TIMER_TICKS_PER_SEC)

typedef struct _tFragIP6
{
	
	struct _tFragIP6	*next;
#if defined(COMCERTO_2000)
	struct _tFragIP6	*ddr_addr;
#endif
	PMetadata	mtd_frags;   /* linked list of packets */
	U32		id;					/* fragment id		*/
	U32 		sAddr[4];
	U32 		dAddr[4];
	
	U32		refcnt;		/* number of fragments */
	int		timer;      	/* when will this queue expire */
	U32		cumul_len;   /* cumulative len of received fragments */
	U8		last_in;    	/* first/last segment arrived? */
	U8		protocol;	/* layer 4 protocol */	
	U16		end;		
	U32		hash;
	U32		seq_num;
} FragIP6 , *PFragIP6;

void ipv6_frag_init(void);
void M_ipv6_frag_timer(void);
void M_ipv6_frag_entry(void);

extern FASTPART frag6Part;
extern FragIP6 Frag6Storage[];

#if defined(COMCERTO_2000_CLASS)
void M_FRAG6_process_packet(PMetadata mtd);
int M_ipv6_frag_reassemble(PMetadata mtd, ipv6_hdr_t *ipv6_hdr, frag_info *frag);
#endif

static __inline U32 HASH_FRAG6(U32 Saddr, U32 Daddr, U32 id)
{
	U32 sum;

	sum = id + Saddr + ((Daddr << 7) | (Daddr >> 25));
	sum += (sum >> 16);

	return (sum ^ (sum >> 8)) & (NUM_FRAG_ENTRIES - 1);
}
#if defined (COMCERTO_2000) && defined (COMCERTO_2000_UTIL)
#include "util_dmem_storage.h"

static __inline void *frag_alloc(PFASTPART part)
{
	PFragIP6 pFrag_ddr, pFrag_dmem;

	pFrag_ddr = SFL_alloc_part(part);
	if (pFrag_ddr) {
		pFrag_dmem = FRAG_MTD_CACHE;
		pFrag_dmem->ddr_addr = pFrag_ddr;
		pFrag_dmem->next = NULL;
		return pFrag_dmem;
	}
	return NULL;
}

#define frag_put(frag)	\
	efet_memcpy(frag->ddr_addr, frag, sizeof(*frag))

#define frag_get(frag_ddr)		\
	(efet_memcpy(FRAG_MTD_CACHE, frag_ddr, sizeof(*frag_ddr)), FRAG_MTD_CACHE)

#define frag_ddr_addr(frag)	((frag)->ddr_addr)


#define frag_free(part, frag) SFL_free_part((part), (frag)->ddr_addr)
#else

#define frag_get(frag)			(frag)
#define frag_ddr_addr(frag)		(frag)

static __inline void *frag_alloc(PFASTPART part)
{
	return SFL_alloc_part(part);
}

#define frag_free(part, frag) SFL_free_part(&(part), (frag))
#endif

#if defined(COMCERTO_2000_CLASS)
void M_FRAG6_process_packet(PMetadata mtd);
#endif

#if defined(COMCERTO_2000_CONTROL) || !defined(COMCERTO_2000)
int IPv6_HandleIP_Set_FragTimeout(U16 *p, U16 Length);
#endif

struct ip_frag_ctrl {
	U16 timeout;
	U16 expire_drop;
	U16 sam_timeout;
	U16 unused;
};
#endif /* _MODULE_IPV6FRAG_H_ */

