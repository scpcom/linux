/*
 *
 *  Copyright (C) 2007 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PFE_TSO_H_
#define _PFE_TSO_H_

#define  PFE_TSO_STATS

struct tso_cb_s
{
	unsigned int len_counters[32];
	/* This array is used to store the dma mapping for skb fragments */
	struct hif_frag_info_s 	dma_map_array[EMAC_TXQ_CNT][EMAC_TXQ_DEPTH];
};

int pfe_tso( struct sk_buff *skb, struct hif_client_s *client, struct tso_cb_s *tso, int qno, u32 ctrl);
void pfe_tx_skb_unmap( struct sk_buff *skb);
void pfe_tx_get_req_desc(struct sk_buff *skb, unsigned int *n_desc, unsigned int *n_segs);
#endif /* _PFE_TSO_H_ */
