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

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/timer.h>

#include <net/ip.h>
#include <net/sock.h>
#include "pfe_mod.h"
#include "pfe_tso.h"


/** pfe_tso_skb_unmap
 */

void pfe_tx_skb_unmap( struct sk_buff *skb)
{
	struct hif_frag_info_s *fi = (struct hif_frag_info_s *)skb->cb;
	int ii;
	//printk("frag count : %d, map :%p\n", fi->frag_count, fi->map);

	for (ii = 0; ii < fi->frag_count; ii++) {
		struct hif_frag_dma_map_s *dma = fi->map + ii;

		//	printk("frag addr : %x len : %d\n",  dma->data, dma->len);
		dma_unmap_single(pfe->hif.dev, dma->data, dma->len, DMA_TO_DEVICE);
	}
}

/** pfe_tso_to_desc
 */
static unsigned int pfe_tso_to_desc(struct sk_buff *skb)
{
	struct skb_shared_info *sh = skb_shinfo(skb);
	unsigned int sh_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
	skb_frag_t *f; /* current fragment */
	int len;
	int f_size;  /* size of the current fragment */
	unsigned int s_size; /* segment size */
	unsigned int n_desc = 0;
	int i;

	/* linear part */
	len = skb_headlen(skb) - sh_len;
	s_size = sh->gso_size;

	while (len >= s_size) {
#if 0
		if (printk_ratelimit())
			printk(KERN_INFO "%s: remainig leanier len : %d\n", __func__, len);
#endif

		len -= s_size;
		n_desc++;
	}

	if (len) {
#if 0
		if (printk_ratelimit())
			printk(KERN_INFO "%s: remainig leanier len : %d\n", __func__, len);
#endif

		n_desc++;
		s_size -= len;
	}

	/* fragment part */
	for (i = 0; i < sh->nr_frags; i++) {
		f = &sh->frags[i];
		f_size = skb_frag_size(f);

		while (f_size >= s_size) {
			f_size -= s_size;
			n_desc++;
			s_size = sh->gso_size;
		}

		if (f_size) {
			n_desc++;
			s_size -= f_size;
		}
	}

	return n_desc;

}

/** pfe_tso_get_req_desc
 *  Compute number of required HIF descriptors and number of packets on wire
 *  n_desc : number of HIF descriptors
 *  n_segs : number of segments on wire
 */
void pfe_tx_get_req_desc(struct sk_buff *skb, unsigned int *n_desc, unsigned int *n_segs)
{
	struct skb_shared_info *sh = skb_shinfo(skb);
	/**
	* TSO case
	* Number of descriptors required = payload scatter +
	*                                  sh->gso_segs (pre segment header) +
	*                                  sh->gso_segs (hif/tso segment header)
	* sh->gso_segs : number of packets on wire
	*/
	if (skb_is_gso(skb)) {
		*n_desc = pfe_tso_to_desc(skb) + 2 * sh->gso_segs;
		*n_segs = sh->gso_segs;
	}
	// Scattered data
	else if (sh->nr_frags) {
		*n_desc = sh->nr_frags + 1;
		*n_segs = 1;
	}
	// Regular case
	else {
		*n_desc = 1;
		*n_segs = 1;
	}
	return;
}

#define inc_txq_idx(idxname) idxname = (idxname+1) & (qdepth-1)
/** pfe_tso
 */
int pfe_tso( struct sk_buff *skb, struct hif_client_s *client, struct tso_cb_s *tso, int qno, u32 init_ctrl)
{
	struct skb_shared_info *sh = skb_shinfo(skb);
	skb_frag_t *f;
	struct tcphdr *th;
	unsigned int tcp_off = skb_transport_offset(skb);
	unsigned int ip_off = skb_network_offset(skb);
	unsigned int sh_len = tcp_off + tcp_hdrlen(skb); /* segment header length (link + network + transport) */
	unsigned int data_len = skb->len - sh_len;
	unsigned int f_id;	/* id of the current fragment */
	unsigned int f_len;	/* size of the current fragment */
	unsigned int s_len;	/* size of the current segment */
	unsigned int ip_tcp_hdr_len;
	unsigned int l_len;	/* skb linear size */
	unsigned int len;
	int segment;
	unsigned int off, f_off;
	u32 ctrl;
	unsigned int id;
	unsigned int seq;
	int wrIndx;
	struct hif_frag_info_s *f_info;
	struct hif_frag_dma_map_s *f_dma, *q_dma_base, *f_dma_first;
	unsigned int tx_bytes = 0;
	int qdepth = client->tx_qsize;

	// ctrl = (skb->ip_summed == CHECKSUM_PARTIAL) ? HIF_CTRL_TX_CHECKSUM : 0;
	ctrl = (HIF_CTRL_TX_CHECKSUM | init_ctrl);

	if (skb->protocol == htons(ETH_P_IP)) {
		struct iphdr *ih = ip_hdr(skb);

		id = ntohs(ih->id);

		ctrl |= HIF_CTRL_TX_TSO;

	} else if (skb->protocol == htons(ETH_P_IPV6)) {
		id = 0;
		ctrl |= HIF_CTRL_TX_TSO6;
	}
	else {
		kfree_skb(skb);
		return 0;
	}

#ifdef PFE_TSO_STATS
	tso->len_counters[((u32)skb->len >> 11) & 0x1f]++;
#endif

	th = tcp_hdr(skb);
	ip_tcp_hdr_len = sh_len - ip_off;

	seq = ntohl(th->seq);

	/* linear part */
	l_len = skb_headlen(skb) - sh_len;
	off = sh_len;

	/* fragment part */
	f_id = 0;
	f_len = 0;
	f = NULL;
	f_off = 0;

	f_info = (struct hif_frag_info_s *)skb->cb;

	/* In case of TSO segmentation, we might need to transmit single skb->fragment
	 * in multiple segments/scatters. So hif will perform cache flush, for each scatter
         * of same fragment separatly. Since cahche flush is expensive operatrion we will 
         * perform dma mapping for full fragment here and provide physical addresses to HIF.
	 *
	 * Preserve dmamapping information corresponding to skb fragments in array, this 
         * information is used while freeing SKB.
	 */
	wrIndx = hif_lib_get_tx_wrIndex(client, qno);
	q_dma_base = (struct hif_frag_dma_map_s *)&tso->dma_map_array[qno][0];
	f_dma = f_info->map = q_dma_base + wrIndx;
	f_dma_first = f_dma;
	f_dma->data = dma_map_single(pfe->hif.dev, skb->data, skb_headlen(skb), DMA_TO_DEVICE);
	f_dma->len = skb_headlen(skb);
	f_info->frag_count = 1;
	//printk("data : %x, len :%d base : %p\n",f_dma->data, f_dma->len, f_info->map);


//	printk("%s: orig_seq: %x, orig_id: %x, seg_size: %d, seg_n: %d, data_len: %d, hdr_len: %d skb_fras: %d\n", __func__, seq, id, s_len, sh->gso_segs, data_len, sh_len, sh->nr_frags);

	for (segment = 0; segment < sh->gso_segs; segment++) {
		unsigned int hif_flags = HIF_TSO;

		if (segment == (sh->gso_segs - 1)) {
			ctrl |= HIF_CTRL_TX_TSO_END;
		}

		/* The last segment may be less than gso_size. */
		if (data_len < sh->gso_size)
			s_len = data_len;
		else
			s_len = sh->gso_size;

		data_len -= s_len;

//		printk("%s: seq: %x, id: %x, ctrl: %x\n", __func__, seq, id, ctrl);

		/* hif/tso header */
		__hif_lib_xmit_tso_hdr(client, qno, ctrl, ip_off, id, ip_tcp_hdr_len + s_len, tcp_off, seq);

		id++;
		seq += s_len;

		/* ethernet/ip/tcp header */
		__hif_lib_xmit_pkt(client, qno, (void *)f_dma_first->data, sh_len, 0, HIF_DONT_DMA_MAP | hif_flags, skb);
		tx_bytes += sh_len;

		while (l_len && s_len)
		{
			len = s_len;
			if (len > l_len)
				len = l_len;

			l_len -= len;
			s_len -= len;

			if (!s_len)
				hif_flags |= HIF_LAST_BUFFER;

#if 0
			if (printk_ratelimit())
				printk(KERN_INFO "%s: Extra lienear data addr: %p, len: %d, flags: %x\n", __func__,
							skb->data + off, len, hif_flags);
#endif

			__hif_lib_xmit_pkt(client, qno,  (void *)((u32)f_dma->data + off),
									len, 0, hif_flags | HIF_DONT_DMA_MAP, skb);
			off += len;
			tx_bytes += len;
		}

		while (s_len) {
			hif_flags = HIF_TSO | HIF_DONT_DMA_MAP;

			/* Advance as needed. */
			if (!f_len) {
				f = &sh->frags[f_id];
				f_len = skb_frag_size(f);
				f_off = 0;
				f_id++;

				/* In case of TSO segmentation, we might need to transmit single skb->fragment
				 * in multiple segments/scatters. So hif will perform cache flush, for each scatter
			         * of same fragment separatly. Since cahche flush is expensive operatrion we will
			         * perform dma mapping for full fragment here and provide physical addresses to HIF.
				 *
				 * Preserve dmamapping information corresponding to skb fragments in array, this
			         * information is used while freeing SKB.
				 */
			        inc_txq_idx(wrIndx);
				f_dma = q_dma_base + wrIndx;
				f_dma->data = dma_map_single(pfe->hif.dev, skb_frag_address(f), f_len, DMA_TO_DEVICE);
				f_dma->len =  f_len;
				f_info->frag_count++;
				//printk("data : %x, len :%d\n",f_dma->data, f_dma->len);
				//printk("%s: frag addr: %p, len: %d\n", __func__, f, f_len);
			}

			/* Use bytes from the current fragment. */
			len = s_len;
			if (len > f_len)
				len = f_len;

			f_len -= len;
			s_len -= len;

			if (!s_len) {
				hif_flags |= HIF_LAST_BUFFER;

				if (segment == (sh->gso_segs - 1))
					hif_flags |= HIF_DATA_VALID;
			}

			//printk("%s: scatter addr: %p, len: %d, flags: %x\n", __func__,
			//		skb_frag_address(f) + f_off, len, hif_flags);

			__hif_lib_xmit_pkt(client, qno, (void *)((u32)f_dma->data + f_off), len, 0, hif_flags, skb);

			f_off += len;
			tx_bytes += len;
		}
	}

	hif_tx_dma_start();

	return tx_bytes;

}
