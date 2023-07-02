
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


#ifndef _UTIL_RX_H_
#define _UTIL_RX_H_

#include "types.h"
#include "system.h"
#include "util_mtd.h"
#include "util_tx.h"


#define MAX_UTIL_RX_FETCH_CNT	4

/* Maxmium number of packets to be handled per event */
#define MAX_PKTS_TO_DEQUEUE	8

void util_init(void);
int M_Util_RX_process_packets(void);

#define data_buf_offset(mtd) ((unsigned long)mtd->rx_next & 0x7)
/**
 * Returns the number of packets pending in the Util PE INQ FIFO.
 * @return number of pending packets in UtilPE INQ queue.
 */
static int inline util_pending_inq_packets(void)
{
	return readl(INQ_FIFO_CNT);
}

static inline void __fetch_dmem_payload(PMetadata mtd, unsigned int dmem_offset, u32 len)
{
	void *ddr_addr = mtd->rx_next;

	mtd->data = TRANSMIT_BUFFER;
	mtd->offset = mtd->base_offset = ((unsigned long)ddr_addr & 0x7) + dmem_offset;

	if (len)
		efet_memcpy(mtd->data + mtd->offset, ddr_addr, len);

	mtd->rx_dmem_end = mtd->data + mtd->offset + len;
}

/* This function fetches dmem considering the offset of the data in the packet */
static inline u32 _fetch_dmem_payload(PMetadata mtd, unsigned int dmem_offset, u32 pkt_len, u32 buf_len)
{
	u32 dmem_len  = min (pkt_len , (buf_len - data_buf_offset(mtd)));

	__fetch_dmem_payload(mtd, dmem_offset, dmem_len);

	return dmem_len;
}
/**
 * Fetches packet payload to local DMEM buffer conserving relative data offset
 * and keeping track of amount of data copied to DMEM.
 * The caller specifies a minimum amount of data to fetch, but the function may fetch less if above the maximum buffer size.
 * On UTIL transmit all the data in DMEM is written back to BMU1 and/or BMU2 buffer.
 *
 */
static inline void fetch_dmem_payload(PMetadata mtd, u32 len)
{
	void *ddr_addr = mtd->rx_next;
	u32 dmem_len, offset;

	/* Copy DDR to local DMEM */
	/* On exit from UTIL to CLASS, mtd->data will be copied to start of BMU1 */
	/* Make sure that the offset of data in DMEM/BMU1 matches the one in DDR (relative to 2K + DDR_HDR_SIZE - LMEM_BUF_SIZE) */
	/* This way when CLASS does writeback of data to DDR, on transmit, the data aligns correctly on DDR */
	offset = ((unsigned long)ddr_addr & 0x7);

	/* never more than the maximum buffer size */
	dmem_len = min(len, LMEM_BUF_SIZE - LMEM_HDR_SIZE - offset);

	/* never less than the minimum buffer size */
	dmem_len = max(dmem_len, LMEM_MAX_PKTDATA_LEN - offset);


	/* unless the packet is smaller */
	dmem_len = min(dmem_len, mtd->length);

	__fetch_dmem_payload(mtd, LMEM_HDR_SIZE, dmem_len);
}

#endif /* _UTIL_RX_H_ */

