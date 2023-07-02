
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

#ifndef _UTIL_TX_H_
#define _UTIL_TX_H_

#include "types.h"
#include "hal.h"
#include "mtd.h"
#include "util_dmem_storage.h"
#include "tx.h"

#define LMEM_MAX_PKTDATA_LEN		(LMEM_BUF_SIZE - LMEM_HDR_SIZE - UTIL_TX_TRAILER_SIZE) /**< Round it down to nearest 32-bit boundary so trailer is aligned. */

#define TRANSMIT_BUFFER		((void *)&util_buffers.transmit_buffer[0])
#define PACKET_BUFFER		(TRANSMIT_BUFFER + LMEM_HDR_SIZE)


/**
 * Prepares UTIL tx trailer to send packet to CLASS. If data in DMEM overlaps with the trailer
 * do writeback of the overlaping range to DDR now, otherwise writeback will be done later to LMEM only.
 *
 */
lmem_trailer_t *util_tx_trailer_prepare(PMetadata mtd, u32 packet_type);
void util_send_to_tx_direct(u8 port, u8 queue, PMetadata mtd, u16 hdr_len, u8 free);
void util_send_to_classpe(PMetadata mtd,u8 free, u8 pe_id);
void util_send_to_expt_direct(PMetadata mtd);


/**
 * Prepares UTIL tx trailer to send packet to CLASS.
 *
 */
static inline lmem_trailer_t *__util_tx_trailer_prepare(PMetadata mtd, u32 packet_type)
{
	lmem_trailer_t *trailer = UTIL_TX_TRAILER(mtd);

	trailer->packet_type = packet_type;
	trailer->offset = mtd->offset & 0x7;
	trailer->ddr_offset = (((unsigned long)mtd->rx_next & 0x7f) - LMEM_HDR_SIZE) >> 3;
	trailer->mtd_flags = mtd->flags;

	return trailer;
}
#endif /* _UTIL_TX_H_ */
