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
#ifndef _MODULE_LRO_H_
#define _MODULE_LRO_H_

#ifdef CFG_LRO

#if !defined(COMCERTO_2000_CONTROL)
#include "modules.h"
#include "common_hdrs.h"
#include "fe.h"

#define	IPH_LEN_WO_OPTIONS	5
#define TCPH_LEN_WO_OPTIONS	5
#define TCPH_LEN_W_TIMESTAMP	8

#define MAX_LRO_SESSION		2
#define IFG_SIZE		12
#define PREAMBLE		8
#define CRC_SIZE		4
#define ETHERNET_MAX_SIZE	1514
#define FRAME_MAX_SIZE_BITS	((IFG_SIZE + PREAMBLE + ETHERNET_MAX_SIZE + CRC_SIZE) * 8)
#define LINE_BITS_PER_SEC	1000000000 /* assume 1Gbps link */
#define US_PER_FRAME		((FRAME_MAX_SIZE_BITS * 10000)/(LINE_BITS_PER_SEC/100))
#define LRO_TIMEOUT_US		(1 * US_PER_FRAME)
#define LRO_TIMEOUT_CYCLES	(10 * LRO_TIMEOUT_US * TIMER_CYCLES_PER_USEC)
#define LRO_LCOUNT_TIMEOUT	(50 * TIMER_CYCLES_PER_MSEC)
#define MAX_SG_NUM		4
#define MAX_SG_SIZE		(16 * 1024 - 32)

/* Data structure to represent a LRO session */
struct lro_session {
	ipv4_hdr_t *iph;
	tcp_hdr_t *tcph;
	u32 src_addr;
	u32 dst_addr;
	u32 tcp_next_seq;
	u32 tcp_ack;
	u16 total_len;
	u16 src_port;
	u16 total_ip_len;
	u16 dst_port;
	u16 window;
	u16 tcp_flags;
	u16 mss;
	u16 tcp_data_offset;
	u16 count;			/* current aggregation packet count */
//	u16 vlan_tag;
//	u32 cur_tsval;
//	u32 cur_tsecr;
	u32 last_cycles;
	u32 lcount;			/* long term session packet count */
	void *ddr_data[MAX_SG_NUM];	/* points to the start of the session data in DDR */
	u16 ddr_len[MAX_SG_NUM];
	void *ddr_start;		/* points to start of current BMU2 buffer */
	u16 ddr_offset;			/* offset in current BMU2 buffer */
	u16 ddr_left;			/* remaining space in current BMU2 buffer */
	u16 flags;			/* flags from the first session mtd */
//	u8 saw_ts;
	u8 in_use;
	u8 sg_num;
	u8 input_port;			/* input_port from the first session mtd */
};

void lro_init(void);

void M_LRO_process_packet(PMetadata mtd);
void M_LRO_process_2nd_stage(PMetadata mtd);

void lro_timer_cb(void);

#if defined(COMCERTO_2000_CLASS)
extern u8 lro_enable;
#else
#define lro_enable	0 /*TODO change it to shared variable */
#endif
#endif /* COMCERTO_2000_CONTROL */

#define LRO_ENABLE_ALL		1
#define LRO_ENABLE_SELECTED	2

#endif /* CFG_LRO */
#endif /* _MODULE_LRO_H_ */
