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

#ifdef CFG_STATS

#ifndef _MODULE_STAT_H_
#define _MODULE_STAT_H_

#include "types.h"
#include "module_qm.h"


#define FPP_STAT_RESET  		0x0001
#define FPP_STAT_QUERY 			0x0002
#define FPP_STAT_QUERY_RESET   (FPP_STAT_RESET|FPP_STAT_QUERY)

#define FPP_STAT_ENABLE		0x0001
#define FPP_STAT_DISABLE		0x0000

#define STAT_PPPOE_QUERY_NOT_READY	0
#define STAT_PPPOE_QUERY_READY	1
#define STAT_PPPOE_QUERY_RESET      2

#define STAT_BRIDGE_QUERY_NOT_READY	0
#define STAT_BRIDGE_QUERY_READY	1
#define STAT_BRIDGE_QUERY_RESET    2

#define STAT_IPSEC_QUERY_NOT_READY	0
#define STAT_IPSEC_QUERY_READY	1
#define STAT_IPSEC_QUERY_RESET    2
#define STAT_VLAN_QUERY_NOT_READY	0
#define STAT_VLAN_QUERY_READY			1
#define STAT_VLAN_QUERY_RESET      		2

#define STAT_TUNNEL_QUERY_NOT_READY	0
#define STAT_TUNNEL_QUERY_READY		1
#define STAT_TUNNEL_QUERY_RESET      	2


/* Definitions of Bit Masks for the features */
#define STAT_QUEUE_BITMASK 		0x00000001
#define STAT_INTERFACE_BITMASK 		0x00000002
#define STAT_PPPOE_BITMASK 		0x00000008
#define STAT_BRIDGE_BITMASK 		0x00000010
#define STAT_IPSEC_BITMASK 		0x00000020
#define STAT_VLAN_BITMASK 		0x00000040
#define STAT_TUNNEL_BITMASK 		0x00000080

#if defined(COMCERTO_2000)
#if defined(COMCERTO_2000_CONTROL) || defined (COMCERTO_2000_CLASS)
struct class_stats {
	U64 total_bytes_received[MAX_PHY_PORTS];
	U32 total_pkts_received[MAX_PHY_PORTS];
};

#endif /* defined(COMCERTO_2000_CONTROL) || defined (COMCERTO_2000_CLASS) */

#if defined(COMCERTO_2000_CONTROL) || defined (COMCERTO_2000_TMU)
struct tmu_stats {
	U64 total_bytes_transmitted;
	U64 emitted_pkts[NUM_QUEUES];
	U32 total_pkts_transmitted;
	U32 peak_queue_occ[NUM_QUEUES];
};

#endif /* defined(COMCERTO_2000_CONTROL) || defined (COMCERTO_2000_TMU) */

#if !defined(COMCERTO_2000_CONTROL)

#if defined (COMCERTO_2000_CLASS)
//extern struct class_stats stats;
#define STATISTICS_INCREMENT(counter) 			((counter) += 1)
#define STATISTICS_DECREMENT(counter) 			((counter) -= 1)
#define STATISTICS_ADD(counter, add_amount) 		((counter) += (add_amount))
#elif defined (COMCERTO_2000_TMU)
extern struct tmu_stats stats;
#define STATISTICS_INCREMENT(counter) 			((stats.counter) += 1)
#define STATISTICS_DECREMENT(counter) 			((stats.counter) -= 1)
#define STATISTICS_ADD(counter, add_amount) 		((stats.counter) += (add_amount))
#endif

#endif /* !defined(COMCERTO_2000_CONTROL) */

#else /* !defined(COMCERTO_2000) */

typedef struct _tFpStatistics{
	U64 FS_total_bytes_received[GEM_PORTS];
	U64 FS_total_bytes_transmitted[GEM_PORTS];
	U32 FS_total_pkts_received[GEM_PORTS];
	U32 FS_total_pkts_transmitted[GEM_PORTS];
	U64 FS_emitted_pkts[GEM_PORTS][NUMBER_QUEUES];
	U64 FS_dropped_pkts[GEM_PORTS][NUMBER_QUEUES];
	U32 FS_peak_queue_occ[GEM_PORTS][NUMBER_QUEUES];
} __attribute__ ((aligned(32))) tFpStatistics;


extern tFpStatistics gFpStatistics;

#define STATISTICS_INCREMENT(counter) 			((gFpStatistics.FS_##counter) += 1)
#define STATISTICS_DECREMENT(counter) 			((gFpStatistics.FS_##counter) -= 1)
#define STATISTICS_ADD(counter, add_amount) 		((gFpStatistics.FS_##counter) += add_amount)

#endif /* !defined(COMCERTO_2000) */

typedef struct _tFpCtrlStatistics {
	U32 FS_max_active_connections;
	U32 FS_num_active_connections;
} tFpCtrlStatistics;

typedef struct _tStatEnableCmd {
	U16 action; /* 1 - Enable, 0 - Disable */
	U32 bitmask; /* Specifies the feature to be enabled or disabled */ 
}StatEnableCmd, *PStatEnableCmd;

typedef struct _tStatQueueCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
	unsigned short interface;
	unsigned short queue;
}StatQueueCmd, *PStatQueueCmd;

typedef struct _tStatInterfaceCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
	unsigned short interface;
}StatInterfaceCmd, *PStatInterfaceCmd;

typedef struct _tStatConnectionCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
}StatConnectionCmd, *PStatConnectionCmd;

typedef struct _tStatPPPoEStatusCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
}StatPPPoEStatusCmd, *PStatPPPoEStatusCmd;

typedef struct _tStatBridgeStatusCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
}StatBridgeStatusCmd, *PStatBridgeStatusCmd;

typedef struct _tStatIpsecStatusCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
}StatIpsecStatusCmd, *PStatIpsecStatusCmd;

typedef struct _tStatVlanStatusCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
}StatVlanStatusCmd, *PStatVlanStatusCmd;

typedef struct _tStatTunnelStatusCmd {
	unsigned short action; /* Reset, Query, Query & Reset */
	unsigned short pad;
	char ifname[IF_NAME_SIZE];
}StatTunnelStatusCmd, *PStatTunnelStatusCmd;

typedef struct _tStatQueueResponse {
	U16 ackstatus;
	U16 rsvd1;
	unsigned int peak_queue_occ; 
	unsigned int emitted_pkts; 
	unsigned int dropped_pkts; 
	
}StatQueueResponse, *PStatQueueResponse;

typedef struct _tStatInterfacePktResponse {
	U16 ackstatus;
	U16 rsvd1;
	U32 total_pkts_transmitted;
	U32 total_pkts_received;
	U32 total_bytes_transmitted[2]; /* 64 bit counter stored as 2*32 bit counters */ 
	U32 total_bytes_received[2]; /* 64 bit counter stored as 2*32 bit counters */

}StatInterfacePktResponse, *PStatInterfacePktResponse;

typedef struct _tStatConnResponse {
	U16 ackstatus;
	U16 rsvd1;
	U32 max_active_connections;
	U32 num_active_connections;
}StatConnResponse, *PStatConnResponse;

typedef struct _tStatPPPoEEntryResponse {
	U16 ackstatus;
	U16 eof;
	U16 sessionID;
	U16 interface_no; /* physical output port id */
	U32 total_packets_received;  
	U32 total_packets_transmitted; 
}StatPPPoEEntryResponse, *PStatPPPoEEntryResponse;

typedef struct _tStatBridgeEntryResponse {
	U16 ackstatus;
	U16 eof;
	U16 input_interface;
	U16 input_svlan; 
	U16 input_cvlan; 
	U8 dst_mac[6];
	U8 src_mac[6];
	U16 etherType;
	U16 output_interface;
	U16 output_svlan; 
	U16 output_cvlan; 
	U16 session_id;
	U32 total_packets_transmitted; 
	U8 input_name[16];
	U8 output_name[16];
}StatBridgeEntryResponse, *PStatBridgeEntryResponse;

typedef struct _tStatIpsecEntryResponse {
	U16 ackstatus;
	U16 eof;
	U16 family;
	U16 proto;
	U32 spi;
	U32 dstIP[4];
	U32 total_pkts_processed;
	U32 total_bytes_processed[2];
	U16 sagd;
}StatIpsecEntryResponse, *PStatIpsecEntryResponse;

typedef struct _tStatVlanEntryResponse {
	U16 ackstatus;
	U16 eof;
	U16 vlanID;
	U16 rsvd; 
	U32 total_packets_received;
	U32 total_packets_transmitted;
	U32 total_bytes_received[2];
	U32 total_bytes_transmitted[2];
	U8 vlanifname[IF_NAME_SIZE];
	U8 phyifname[IF_NAME_SIZE];
}StatVlanEntryResponse, *PStatVlanEntryResponse;

typedef struct _tStatTunnelEntryResponse {
	U16 ackstatus;
	U16 eof;
	U32 rsvd;
	U32 total_packets_received;
	U32 total_packets_transmitted;
	U32 total_bytes_received[2];
	U32 total_bytes_transmitted[2];
	U8 ifname[IF_NAME_SIZE];
}StatTunnelEntryResponse, *PStatTunnelEntryResponse;

int statistics_init(void);
void statistics_exit(void);

#if defined(COMCERTO_2000)
extern U32 gFpStatFeatureBitMask;
#else
#define gFpStatFeatureBitMask  gFppGlobals.FG_statFeatureBitMask
#endif

extern int gStatBridgeQueryStatus;
extern int gStatPPPoEQueryStatus;
extern int gStatIpsecQueryStatus;
extern int gStatVlanQueryStatus;
extern int gStatTunnelQueryStatus;
extern tFpCtrlStatistics gFpCtrlStatistics;

#if defined(COMCERTO_2000)
void class_statistics_get(void *addr, void *val, U8 size, U32 do_reset);
void class_statistics_get_ddr(void *addr, void *val, U8 size, U32 do_reset);
void tmu_statistics_get(U8 interface, void *addr, void *val, U8 size, U32 do_reset);

#define STATISTICS_CLASS_GET_DMEM(counter, variable, do_reset) 	class_statistics_get(&counter, &variable, sizeof(counter), do_reset)

/* Note: Queue Manager statistics are always handled per interface, IOW per TMU as we have one dedicated TMU per port.
That's why for TMU macros the interface number is used to retrieve statistics from a single TMU */
#define STATISTICS_TMU_GET(interface, counter, variable, do_reset) 	tmu_statistics_get(interface, &tmu_stats.counter, &variable, sizeof(tmu_stats.counter), do_reset)

#endif

#define STATISTICS_SET(counter, value) 			((gFpStatistics.FS_##counter) = value)
#define STATISTICS_GET(counter, variable) 		(variable = (gFpStatistics.FS_##counter))
#define STATISTICS_GET_LSB(counter, variable, type)	(variable = (type)((gFpStatistics.FS_##counter) & 0xffffffff))
#define STATISTICS_GET_MSB(counter, variable, type)	(variable = (type)(((gFpStatistics.FS_##counter) >> 32) & 0xffffffff))

#define GET_PEAK_QUEUE_OCC(interface,queue) 		(gFpStatistics.FS_peak_queue_occ[interface][queue])
#define GET_LSB(counter, variable, type) 		(variable = (type)((counter) & 0xffffffff))
#define GET_MSB(counter, variable, type) 		(variable = (type)(((counter) >> 32) & 0xffffffff))

#define STATISTICS_CTRL_INCREMENT(counter) 		((gFpCtrlStatistics.FS_##counter) += 1)
#define STATISTICS_CTRL_DECREMENT(counter) 		((gFpCtrlStatistics.FS_##counter) -= 1)
#define STATISTICS_CTRL_SET(counter, value) 		((gFpCtrlStatistics.FS_##counter) = value)
#define STATISTICS_CTRL_GET(counter, variable)		(variable = (gFpCtrlStatistics.FS_##counter))


#endif /* _MODULE_STAT_H_ */
#endif /* CFG_STATS */
