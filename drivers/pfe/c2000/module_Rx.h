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


#ifndef _MODULE_RX_H_
#define _MODULE_RX_H_

#include "types.h"
#include "channels.h"



#if defined(COMCERTO_2000_CONTROL)
int rx_init(void);
void rx_exit(void);

BOOL M_rx_enable(U8 portid);

#elif defined(COMCERTO_2000_CLASS)
#define QMOD_NONE 0
#define QMOD_DSCP 1
/*
 * Commands
 */
/* Ingress congestion control management */
typedef struct _tEthIccCommand {
	unsigned short portIdx;	/* enable and disable commands */
	unsigned short acc;	/* optional , enable only*/ 
	unsigned short onThr;	/* optional , enable only*/
	unsigned short offThr;	/* optional , enable only*/
} EthIccCommand, *PEthIccCommand;


__attribute__((always_inline))
static int inline Rx_packet_available(u32 pbuf_i)
{
	U32 qb_buf_status;

	/* read the hardware bitmask value for classifier buffers and check for new packets */

	/* read QB_BUF_STATUS into qbBufStatus */
	/* FIXME can this be read only when the bit is not set? */
	qb_buf_status = readl(PERG_QB_BUF_STATUS);

	return (qb_buf_status & regMask[pbuf_i]) != 0;
}

void class_init(void);
void M_RX_process_packet(u32 pbuf_i);
void fetch_trailer_payload(PMetadata mtd);

#endif /* defined(COMCERTO_2000_CLASS) */


#endif /* _MODULE_RX_H_ */
