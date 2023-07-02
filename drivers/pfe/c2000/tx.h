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

#ifndef _TX_H_
#define _TX_H_

#include "system.h"
#include "module_qm.h"
#include "module_capture.h"
#include "mtd.h"

#if defined(COMCERTO_2000_CLASS)
#define IS_QOS_ENABLED(port)		(phy_port[port].flags & QOS_ENABLED)
#elif defined(COMCERTO_2000_UTIL)
#define IS_QOS_ENABLED(port)		(1) 	/* FIXME we should have a qos enable flag in UTIL too */
#endif
#define IS_TX_ENABLED(port)		(phy_port[port].flags & TX_ENABLED)

u32 __hif_tx_prepare(void *dmem_addr, u32 output_port, u32 *queue, u32 ctrl, u32 flags);
#if defined(COMCERTO_2000_CLASS)
u32 hif_tx_prepare(struct tMetadata *mtd);
#endif
void send_to_tx_direct(u32 port, u32 queue, void *dmem_addr, void *ddr_addr, u32 hdr_len, u16 len, u32 ctrl, u32 flags);
void send_to_expt_direct(void *dmem_addr, void *ddr_addr, u32 len, u32 input_port, u32 ctrl, u32 hdr_len, u32 flags);
void send_to_tmu(u32 port, u32 queue, void *ddr_addr, u32 len);
void __send_to_tx_direct(u32 port, u32 queue, void *ddr_addr, u32 len);


/** Posts a packet descriptor directly to TMU
 *
 * @param port		TMU port
 * @param queue		TMU queue
 * @param addr 		DDR class_tx_hdr + packet data address
 * @param len		DDR packet len
 */
static inline void __send_to_tmu(u32 phy, u32 queue, void *ddr_addr, u32 len)
{
	tmu_tx_hdr_t tmu __attribute__ ((aligned (8)));

	tmu.pkt_ptr = (u32)ddr_addr;
	tmu.len = len;
	tmu.queueno = queue;
	tmu.phyno = phy & 0xf;

	/* write buffer pointer to TMU INQ */
	efet_memcpy((void *)TMU_PHY_INQ_PKTPTR, &tmu, 0x8);

	PESTATUS_INCR_TX();
}

/** Retrieves hif queue
 *
 * @param queue		priority queue
 * @param lro		lro flag
 */
static inline u32 get_hif_queue(u32 queue, u32 flags)
{
#ifdef CFG_LRO
	if (flags & MTD_LRO)
		return 2;
	else
#endif
		return queue > 0;
}

static inline u32 hif_hdr_add(void *dmem_addr, u32 client_id, u32 client_queue, u32 ctrl_le)
{
	u32 hdr_len = sizeof(struct hif_pkt_hdr);
	struct hif_pkt_hdr *hif_hdr = dmem_addr - hdr_len;

	hif_hdr->client_id = client_id;
	hif_hdr->qNo = client_queue;

	hif_hdr->client_ctrl_le_lsw = ctrl_le >> 16;
	hif_hdr->client_ctrl_le_msw = ctrl_le & 0xffff;

	return hdr_len;
}

/*GPI may corrupt the packet if txhdr+pktlen is < GPI_TX_MIN_LENGTH */
#define GPI_TX_MIN_LENGTH	73

/*make sure the minimal packet length passed to GEM is 16bytes to prevent any lockup */
#define GEM_TX_MIN_PKT_LEN 16

static inline u32 tx_hdr_add(void *dmem_addr, void *ddr_addr, u16 len, u32 phy, u32 queue, u32 action)
{
	class_tx_hdr_t *tx_hdr;
	u32 hdr_len;

	tx_hdr = dmem_addr - sizeof(class_tx_hdr_t);

	if (phy < TX_PHY_TMU3)
	{
		if (len < GEM_TX_MIN_PKT_LEN)
			len = GEM_TX_MIN_PKT_LEN;

		if (sizeof(class_tx_hdr_t) + len < GPI_TX_MIN_LENGTH)
			tx_hdr = dmem_addr - (GPI_TX_MIN_LENGTH - len);
	}

	tx_hdr = (class_tx_hdr_t *)ALIGN64(tx_hdr);
	hdr_len = (void *)dmem_addr - (void *)tx_hdr;

	tx_hdr->start_data_off = dmem_addr - (void *)tx_hdr;
	tx_hdr->start_buf_off = ((u32)ddr_addr - hdr_len) & 0xff;
	tx_hdr->pkt_length = len;

	tx_hdr->act_phyno = (action & 0xf0) | (phy & 0xf);
	tx_hdr->queueno = queue;

	return hdr_len;
}

/* HGPI may not work properly for the packets size < HGPI_TX_MIN_PAYLOAD
 * Following WA pads the tx packet length */
#define HGPI_TX_MIN_PAYLOAD 60
static inline u32 tx_hgpi_wa(u32 phy, void *dmem_addr, u16 *len)
{
	u32 pad_len = 0;

	if (*len < HGPI_TX_MIN_PAYLOAD)
	{
		u32 offset = ((u32)dmem_addr & (CLASS_PBUF_SIZE - 1)) + *len;

		if (offset < CLASS_PBUF_SIZE)
		{
			pad_len = min(CLASS_PBUF_SIZE - offset, HGPI_TX_MIN_PAYLOAD - *len);

			memset(dmem_addr + *len, 0, pad_len);
		}

		*len = HGPI_TX_MIN_PAYLOAD;
	}

	return pad_len;
}

#define TX_HGPI_WA(phy, mtd) do {							\
		mtd->rx_dmem_end += tx_hgpi_wa(phy, mtd->data + mtd->offset, &mtd->length);	\
} while (0)

static inline u32 __tx_prepare(u32 port, u32 *queue, u32 phy, void *dmem_addr, void *ddr_addr, u16 *len, u32 ctrl, u32 flags)
{
	u32 hdr_len = 0;
	u32 action = 0;

	if (port < GEM_PORTS)
	{
		if (!IS_QOS_ENABLED(port))
			*queue = 0;

		if (flags & MTD_TX_CHECKSUM)
			action = ACT_TCPCHKSUM_REPLACE;

	}
	else
	{
		if (IS_HIF_PORT(port)) {
			hdr_len = __hif_tx_prepare(dmem_addr, port, queue, ctrl, flags);
			dmem_addr -= hdr_len;
			ddr_addr -= hdr_len;
			*len += hdr_len;

#ifdef CFG_LRO
			if (flags & MTD_LRO)
				*queue = TMU_QUEUE_LRO;
#endif
		}

		if (port != UTIL_PORT)
			tx_hgpi_wa(phy, dmem_addr, len);
	}

	hdr_len += tx_hdr_add(dmem_addr, ddr_addr, *len, phy, *queue, action);

	return hdr_len;
}

#if defined(COMCERTO_2000_UTIL)
#define UTIL_MAX_TX_HDR_LEN 32 /* must be 64bit aligned and big enough for all tx headers class + hif/(hif+class)/(class+hif+fraghdr)) */
#endif
#if defined(COMCERTO_2000_CLASS)
#define CLASS_MAX_TX_HDR_LEN 32 /* must be 64bit aligned and big enough for all tx headers class + hif/(hif+ipsec)/(hif+lro)/util) */
static inline u32 tx_prepare(struct tMetadata *mtd, void *dmem_addr, void *ddr_addr)
{
	u32 hdr_len = 0;
	u32 phy = get_tx_phy(mtd->output_port);
	u32 action = 0;

	if (mtd->output_port < GEM_PORTS)
	{
#if defined(CFG_PCAP)
		if (M_pktcap_chk_enable(mtd->output_port))
			M_pktcap_process(mtd, mtd->output_port);
#endif

		if (!IS_QOS_ENABLED(mtd->output_port))
			mtd->queue = 0;

		if (mtd->flags & MTD_TX_CHECKSUM)
			action = ACT_TCPCHKSUM_REPLACE;
	}
	else
	{
		if (IS_HIF_PORT(mtd->output_port)) {
			hdr_len += hif_tx_prepare(mtd);
			mtd->offset -= hdr_len;
			mtd->length += hdr_len;

#ifdef CFG_LRO
			if (mtd->flags & MTD_LRO)
				mtd->queue = TMU_QUEUE_LRO;
#endif
		}

		if (mtd->output_port != UTIL_PORT)
			TX_HGPI_WA(phy, mtd);
	}

	hdr_len += tx_hdr_add(dmem_addr - hdr_len, ddr_addr - hdr_len, mtd->length, phy, mtd->queue, action);

	return hdr_len;
}
#endif

static inline void send_to_class(PMetadata mtd, u32 pkt_length, class_rx_hdr_t *lmem_hdr, u32 lmem_len, u8 target_pe_id)
{
	void *lmem_ptr;

	//Initialize class_rx_hdr
	lmem_hdr->length = pkt_length;
	lmem_hdr->next_ptr = (u32)mtd->rx_next & ~(DDR_BUF_SIZE - 1);
	lmem_hdr->phyno = get_rx_phy(mtd->input_port) | (mtd->input_port << RX_PHY_SW_INPUT_PORT_OFFSET);
#if defined(COMCERTO_2000_CLASS)
	lmem_hdr->phyno |= RX_PHY_CLASS;
#else
	lmem_hdr->phyno |= RX_PHY_UTIL;
#endif
	lmem_hdr->status = (STATUS_IP_CHECKSUM_CORRECT | STATUS_UDP_CHECKSUM_CORRECT |
			STATUS_TCP_CHECKSUM_CORRECT | STATUS_UNICAST_HASH_MATCH | STATUS_CUMULATIVE_ARC_HIT);
	if (target_pe_id != PE_ID_ANY)
		lmem_hdr->status |= STATUS_DIR_PROC_ID | STATUS_PE2PROC_ID(target_pe_id);
	lmem_hdr->status2 = 0;

	//dmem writeback to BMU1
	lmem_ptr = bmu1_alloc();
	efet_memcpy(lmem_ptr, lmem_hdr, lmem_len);

	// post BMU1 to class
	efet_writel(lmem_ptr, CLASS_INQ_PKTPTR);
	PESTATUS_INCR_TX();
}

#endif /* _TX_H_ */
