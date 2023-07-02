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


#ifndef _UTIL_MTD_H_
#define _UTIL_MTD_H_

#include "types.h"
#include "hal.h"


#define NUM_MTDS		64
#define NUM_FRAG_MTDS		(NUM_FRAG4_Q*2*2)


typedef struct tMetadata {
	struct tMetadata *next;
	u16	length;
	u16	offset;		/**< data offset, both in DDR (relative to mtd->rx_next) and DMEM (relative to mtd->data) */
	u8	*data;		/**< Points to packet data in DMEM */

	void 	*rx_next;	/** Points to packet data in DDR, _not_ 2K aligned */
	void	*rx_dmem_end;	/** Points to end of packet data in DMEM */

	u16	flags;
	u8	input_port;
	u8	base_offset;	/**< DMEM base offset where packet is fetched to */

	union {
		// FIXME -- is this right????  problem with c2k???
	  	struct  { U32 pad_double; U64 dword;} __attribute__((packed));
		U32	word[MTD_PRIV];
		struct socket_info {
			U16 id;
			U32 l4cksum;
			U32 l4offset;
		} socket;

		struct frag_info {
			U16 offset;
			U16 l4offset; /* Set by class */
			U16 end;
			U16 l3offset; /* Set by class */
		} frag;

		struct ipsec_info {
			U8  l2len[SA_MAX_OP];
			U8  output_port;
			U8  queue;
                        U8  sa_outitf;
                        U8  l4offset;
                        U8  l4proto;
                        S8  sa_op;
                        U16 sa_handle[SA_MAX_OP];
                } ipsec;

		struct rtpqos_info {
		 	U32 client;
			BOOL udp_check;
		} rtpqos;
	} priv;

	//SAEntry	dmem_sa;
} Metadata, *PMetadata;

extern Metadata util_mtd_rtp;
extern Metadata g_util_mtd_table[];
extern Metadata g_util_frag_mtd_table[];
extern PMetadata gmtd_head;
extern PMetadata g_frag_mtd_head;

/* Returns the DDR buffer offset corresponding to the DMEM address */
#define ddr_offset(mtd, dmem_addr)	((void *)(dmem_addr) - (void*)((mtd)->data + BaseOffset(mtd)))

/* Returns the DDR buffer address corresponding to the DMEM address */
#define ddr_addr(mtd, dmem_addr)	((mtd)->rx_next + ddr_offset(mtd, dmem_addr))

static __inline void mtd_init()
{
        int i;
        gmtd_head = NULL;
        for (i = 0; i < NUM_MTDS; i++)
        {
                g_util_mtd_table[i].next = gmtd_head;
                gmtd_head = &g_util_mtd_table[i];
        }
	/*TODO Is it a good idea to move to FPART model?? */
	g_frag_mtd_head = NULL;
	for(i = 0; i < NUM_FRAG_MTDS; i++)
	{
		g_util_frag_mtd_table[i].next = g_frag_mtd_head;
		g_frag_mtd_head = &g_util_frag_mtd_table[i];
	}
}

static __inline PMetadata mtd_alloc()
{
	PMetadata mtd = NULL;

	if (gmtd_head)
	{
		mtd = gmtd_head;
		gmtd_head = gmtd_head->next;
		//mtd->next =NULL;
	}

	return (mtd);
}

static __inline PMetadata frag_mtd_alloc()
{
	PMetadata mtd = NULL;

	if (g_frag_mtd_head)
	{
		mtd = g_frag_mtd_head;
		g_frag_mtd_head = g_frag_mtd_head->next;
		//mtd->next =NULL;
	}

	return (mtd);
}

static __inline void frag_mtd_free(PMetadata pMetaData)
{
	pMetaData->next = g_frag_mtd_head;
	g_frag_mtd_head = pMetaData;
}

static __inline void mtd_free(PMetadata pMetaData)
{
	if (pMetaData == &util_mtd_rtp)
		return;
	if(pMetaData >= g_util_frag_mtd_table) {
		frag_mtd_free(pMetaData);
	}else {
		pMetaData->next = gmtd_head;
		gmtd_head = pMetaData;
	}

	return;
}

#endif /* UTIL_MTD_H_ */
