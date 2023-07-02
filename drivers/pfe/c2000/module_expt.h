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


#ifndef _MODULE_EXPT_H_
#define _MODULE_EXPT_H_

#include "types.h"
#include "channels.h"
#include "modules.h"
#include "module_Rx.h"
#include "module_qm.h"

#define SMI_EXPT0_BASE		0x0A000420
#define SMI_EXPT1_BASE		0x0A000440

#define FPP_SMI_CTRL		0x00
#define FPP_SMI_RXBASE		0x04
#define FPP_SMI_TXBASE		0x08
#define FPP_SMI_TXEXTBASE	0x0C

#define EXPT_BUF_HEADROOM	64

#define EXPT_DEFAULT_TXRATE	(100000 / 1000)
#define EXPT_TX_QMAX		64

#define EXPT_Q0 0
#define EXPT_MAX_QUEUE  3
#define EXPT_ARP_QUEUE	4

#define DSCP_MAX_VALUE 63

#define EXPT_WEIGTH 8
#define EXPT_BUDGET 16

#define EXPT_RX_EXT_DESC_MASK	 (\
	RX_STA_L4_CKSUM |\
	RX_STA_L4_GOOD |\
	RX_STA_L3_CKSUM |\
	RX_STA_L3_GOOD |\
	RX_STA_TCP	 |\
	RX_STA_UDP	 |\
	RX_STA_IPV6	 |\
	RX_STA_IPV4	 |\
	RX_STA_PPPOE	 |\
	0)


typedef struct _tExptQueueDSCPCommand {
	unsigned short queue ;
	unsigned short num_dscp;
	unsigned char dscp[64];
}ExptQueueDSCPCommand, *PExptQueueDSCPCommand;

typedef struct _tExptQueueCTRLCommand {
	unsigned short queue;
	unsigned short reserved;
}ExptQueueCTRLCommand, *PExptQueueCTRLCommand;

//extern volatile U8 Expt_tx_go[MAX_PORTS];

void M_EXPT_process_packet(struct tMetadata *mtd);
void M_EXPT_rx_process_packet(struct tMetadata *mtd);
U16 M_expt_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd);
int M_expt_updateL2(struct tMetadata *mtd, U16 family, U8* itf_flags);
int M_expt_queue_reset(void);
void pfe_expt_queue_init(void);

void M_expt_tx_enable(U8 portid);
void M_expt_tx_disable(U8 portid);
void M_expt_rx_enable(U8 portid);
void M_expt_rx_disable(U8 portid);

#if defined(COMCERTO_2000_CLASS)
void M_EXPT_process_from_util(struct tMetadata *mtd, lmem_trailer_t *trailer);
#endif

#endif /* _MODULE_EXPT_H_ */
