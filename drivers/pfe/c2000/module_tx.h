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


#ifndef _MODULE_TX_H_
#define _MODULE_TX_H_

#include "types.h"
#include "modules.h"
#include "layer2.h"

#if defined(COMCERTO_2000_CONTROL)

#define DEFAULT_NAME_0		eth0
#define DEFAULT_NAME_1		eth2
#define DEFAULT_NAME_2		eth3

typedef struct _tPortUpdateCommand {
	U16 portid;
	char ifname[IF_NAME_SIZE];
} PortUpdateCommand, *PPortUpdateCommand;


int tx_init(void);
void tx_exit(void);

#else
#include "tx.h"

void M_PKT_TX_process_packet(struct tMetadata *mtd);
BOOL M_tx_enable(U32 portid) __attribute__ ((noinline));
void send_to_tx_mcast(struct tMetadata *mtd, void *dmem_addr);
void M_PKT_TX_util_process_packet(struct tMetadata *mtd, u32 pkt_type);
void *dmem_header_writeback(struct tMetadata *mtd);
void send_to_util_direct(struct tMetadata *mtd, u32 pkt_type);
int send_fragment_to_ipsec(struct tMetadata *mtd);
u32 get_pkt_seqnum(u32 pbuf_i);
#ifdef CFG_PCAP
void M_PKT_TX_process_packet_from_util(PMetadata mtd, lmem_trailer_t *trailer);
#endif

#define	send_to_tx(pMetadata) SEND_TO_CHANNEL(M_PKT_TX_process_packet, pMetadata)

#define	send_to_tx_multiple(pBeg, pEnd) SEND_TO_CHANNEL_MULTIPLE(M_PKT_TX_process_packet, pBeg, pEnd)

#endif

#endif /* _MODULE_TX_H_ */

