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


#ifndef _FE_H_
#define _FE_H_

#include "hal.h"
#include "module_timer.h"
#include "module_ethernet.h"
#include "fpp_globals.h"



/* Actions */
#define ACTION_REGISTER		0
#define ACTION_DEREGISTER	1
#define ACTION_REMOVED		3
#define ACTION_UPDATE		4
#define ACTION_QUERY            6
#define ACTION_QUERY_CONT       7
/* 8 is locally used by CMM */
#define ACTION_TCP_FIN		9

/* Actions allowed for multicast entry managment*/
#define MC_ACTION_ADD		0
#define MC_ACTION_REMOVE	1
#define MC_ACTION_REFRESH	2


#define 	OUTPUT_ACP		1
#define 	OUTPUT_GEM		0



/* Error codes */
enum return_code {
	NO_ERR = 0,
	ERR_UNKNOWN_COMMAND = 1,
	ERR_WRONG_COMMAND_SIZE = 2,
	ERR_WRONG_COMMAND_PARAM = 3,

	ERR_UNKNOWN_ACTION = 4,
	ERR_UNKNOWN_INTERFACE = 5,
	ERR_NOT_ENOUGH_MEMORY = 6,
	ERR_CREATION_FAILED = 7,
	
	ERR_BRIDGE_ENTRY_NOT_FOUND = 50,
	ERR_BRIDGE_ENTRY_ALREADY_EXISTS = 51,
	ERR_BRIDGE_WRONG_MODE = 52,

#if !defined(COMCERTO_100)
	ERR_MACVLAN_ALREADY_REGISTERED = 60,
	ERR_MACVLAN_ENTRY_NOT_FOUND = 61,
	ERR_MACVLAN_ENTRY_INVALID = 62,
#endif

	ERR_CT_ENTRY_ALREADY_REGISTERED = 100,
	ERR_CT_ENTRY_NOT_FOUND = 101,
	ERR_CT_ENTRY_INVALID_SA = 102,
	ERR_CT_ENTRY_TOO_MANY_SA_OP = 103,



	ERR_RT_ENTRY_ALREADY_REGISTERED = 200,
	ERR_RT_ENTRY_NOT_FOUND = 201,
	ERR_RT_ENTRY_LINKED = 202,
	ERR_RT_LINK_NOT_POSSIBLE = 203,


	ERR_QM_QUEUE_RATE_LIMITED = 500,
	ERR_QM_RATE_LIMIT_NOT_APPLIED_TO_OFF = 501,
	ERR_QM_QUEUE_OUT_OF_RANGE = 502,
	ERR_QM_NUM_DSCP_OUT_OF_RANGE = 503,
	ERR_QM_DSCP_OUT_OF_RANGE = 504,
	ERR_QM_NO_FREE_SHAPER = 505,
	ERR_QM_NO_QUEUE_SPECIFIED = 506,


	ERR_VLAN_ENTRY_ALREADY_REGISTERED = 600,
	ERR_VLAN_ENTRY_NOT_FOUND = 601,

	ERR_MC_ENTRY_NOT_FOUND = 700,
	ERR_MC_MAX_LISTENERS = 701,
	ERR_MC_DUP_LISTENER = 702,
	ERR_MC_ENTRY_OVERLAP = 703,
	ERR_MC_INVALID_ADDR = 704,
	ERR_MC_INTERFACE_NOT_ALLOWED = 705,

	ERR_PPPOE_ENTRY_ALREADY_REGISTERED = 800,
	ERR_PPPOE_ENTRY_NOT_FOUND = 801,

	ERR_SA_DUPLICATED = 904,
	ERR_SA_DUPLICATED_HANDLE = 905,
	ERR_SA_UNKNOWN = 906,
	ERR_SA_INVALID_CIPHER_KEY = 907,
	ERR_SA_INVALID_DIGEST_KEY = 908,
	ERR_SA_ENTRY_NOT_FOUND = 909,
	ERR_SA_SOCK_ENTRY_NOT_FOUND = 910,
	ERR_SA_INVALID_MODE = 911,

	ERR_TNL_MAX_ENTRIES = 1000,
	ERR_TNL_ENTRY_NOT_FOUND = 1001,
	ERR_TNL_NOT_SUPPORTED = 1002,
	ERR_TNL_NO_FREE_ENTRY = 1003,
	ERR_TNL_ALREADY_CREATED = 1004,

	ERR_STAT_FEATURE_NOT_ENABLED = 1100,

	ERR_EXPT_QUEUE_OUT_OF_RANGE = 1101,
	ERR_EXPT_NUM_DSCP_OUT_OF_RANGE = 1102,
	ERR_EXPT_DSCP_OUT_OF_RANGE = 1103,

	ERR_SOCK_ALREADY_OPEN	= 1200,
	ERR_SOCKID_ALREADY_USED	= 1201,
	ERR_SOCK_ALREADY_OPENED_WITH_OTHER_ID	= 1202,
	ERR_TOO_MANY_SOCKET_OPEN = 1203,
	ERR_SOCKID_UNKNOWN = 1204,
	ERR_SOCK_ALREADY_IN_USE	= 1206,
	ERR_RTP_CALLID_IN_USE	= 1207,
	ERR_RTP_UNKNOWN_CALL = 1208,
	ERR_WRONG_SOCKID = 1209,
	ERR_RTP_SPECIAL_PKT_LEN = 1210,
	ERR_RTP_CALL_TABLE_FULL = 1211,
	ERR_WRONG_SOCK_FAMILY = 1212,
	ERR_WRONG_SOCK_PROTO = 1213,
	ERR_WRONG_SOCK_TYPE = 1214,
	ERR_MSP_NOT_READY = 1215,
	ERR_WRONG_SOCK_MODE = 1216,

	ERR_NATPT_UNKNOWN_CONNECTION = 1220,

	ERR_RTP_STATS_MAX_ENTRIES = 1230,
	ERR_RTP_STATS_STREAMID_ALREADY_USED = 1231,
	ERR_RTP_STATS_STREAMID_UNKNOWN = 1232,
	ERR_RTP_STATS_DUPLICATED = 1233,
	ERR_RTP_STATS_WRONG_DTMF_PT = 1234,
	ERR_RTP_STATS_WRONG_TYPE = 1235,
	ERR_RTP_STATS_NOT_AVAILABLE = 1236,

	ERR_VOICE_BUFFER_UNKNOWN = 1240,
	ERR_VOICE_BUFFER_USED = 1241,
	ERR_VOICE_BUFFER_PT = 1242,
	ERR_VOICE_BUFFER_FRAME_SIZE = 1243,
	ERR_VOICE_BUFFER_ENTRIES = 1244,
	ERR_VOICE_BUFFER_SIZE = 1245,
	ERR_VOICE_BUFFER_STARTED = 1246,

	ERR_ALTCONF_OPTION_NOT_SUPPORTED = 1300,
	ERR_ALTCONF_MODE_NOT_SUPPORTED = 1301,	
	ERR_ALTCONF_WRONG_NUM_PARAMS = 1302,
#ifdef CFG_WIFI_OFFLOAD
	ERR_WLAN_DUPLICATE_OPERATION = 2001,
#endif
#ifdef CFG_PCAP
	ERR_PKTCAP_ALREADY_ENABLED = 1400,
	ERR_PKTCAP_NOT_ENABLED	= 1401,
	ERR_PKTCAP_FLF_RESET	= 1402,
#endif
	ERR_ICC_TOO_MANY_ENTRIES = 1500,
	ERR_ICC_ENTRY_ALREADY_EXISTS = 1501,
	ERR_ICC_ENTRY_NOT_FOUND = 1502,
	ERR_ICC_THRESHOLD_OUT_OF_RANGE = 1503,
	ERR_ICC_INVALID_MASKLEN = 1504,
};




/******************************
* Forward Engine Common Definitions
*
******************************/

/* Conntrack status */
#define CONNTRACK_SEC_FLOW_BI_DIR	0x8000		/* if the flow has both encryption and decryption sa */
#define CONNTRACK_4O6			0x4000
#define	CONNTRACK_RTP_STATS		0x2000
#define CONNTRACK_SEC			0x1000
#define CONNTRACK_SEC_noSA     		0x800
#define CONNTRACK_TCP_FIN		0x400
#define CONNTRACK_TIMED_OUT		0x200
#define CONNTRACK_FF_DISABLED		0x100
#define CONNTRACK_NAT			0x20
#define CONNTRACK_SNAT			CONNTRACK_NAT
#define CONNTRACK_DNAT			0x10
#define CONNTRACK_IPv6			0x08
#define CONNTRACK_ORIG			0x04
#if defined(COMCERTO_2000)
#define CONNTRACK_IPv6_PORTNAT		0x01
#else
#define CONNTRACK_IPIP			0x02
#define CONNTRACK_UDP			0x01
#endif


#define IPPROTOCOL_ICMP 	1
#define IPPROTOCOL_IGMP 	2
#define IPPROTOCOL_TCP 		6
#define IPPROTOCOL_UDP		17
#define IPPROTOCOL_IPIP		4
#define IPPROTOCOL_IPV6		41
#define IPPROTOCOL_ESP 		 50            /* Encapsulation Security Payload protocol */
#define IPPROTOCOL_AH 		 51             /* Authentication Header protocol       */
#define IPPROTOCOL_ICMPV6 	58
#define IPPROTOCOL_ETHERIP	97



/* Nb buckets in cache tables */
#define NUM_ROUTE_ENTRIES	256 

#if defined(COMCERTO_2000_CONTROL)
#define NUM_CT_ENTRIES		(1 << ROUTE_TABLE_HASH_BITS)
#define	CT_TABLE_HASH_MASK	(NUM_CT_ENTRIES - 1)
#elif defined(COMCERTO_2000)
#define	CT_TABLE_HASH_MASK	class_route_table_hash_mask
#else
#define NUM_CT_ENTRIES 		8192
#define	CT_TABLE_HASH_MASK	(NUM_CT_ENTRIES - 1)
#endif

#define NUM_VLAN_ENTRIES	16

#define NUM_BT_ENTRIES 		256
#define NUM_BT_L3_ENTRIES 	1024

#define NUM_SA_ENTRIES 	16
#if defined(COMCERTO_2000)
#define NUM_SOCK_ENTRIES 	64
#else
#define NUM_SOCK_ENTRIES 	128
#endif
#define NUM_IPSEC_SOCK_ENTRIES 	16
#define NUM_L2TP_ENTRIES 		16
#if !defined(COMCERTO_2000)
/* Use status bits to decide on connection protocol */
#define MAX_CONNTRACK_PROTO MAX_L4_PROTO 
#define CONNTRACK_PROTO_MASK 0x03
extern U8   ct_proto[MAX_CONNTRACK_PROTO];
#endif


// connection timer settings
#if defined(COMCERTO_2000_CONTROL) || !defined(COMCERTO_2000)

#define TCP_TIMEOUT			432000		/*5 days*/
#define UDP_UNIDIR_TIMEOUT		30		/*30s*/
#define UDP_BIDIR_TIMEOUT		180		/*180s*/
#define OTHER_PROTO_TIMEOUT		600		/*10 minutes*/

#if defined(COMCERTO_2000_CONTROL)
#define CT_TIMER_INTERVAL_MS	100
#define CT_SCAN_TIME_MS		2000
#else
#define CT_TIMER_INTERVAL_MS	10
#define CT_SCAN_TIME_MS		5120
#endif


#define CT_TIMER_INTERVAL	((CT_TIMER_INTERVAL_MS * TIMER_TICKS_PER_SEC) / 1000)
#define CT_TICKS_PER_SECOND	(1000 / CT_TIMER_INTERVAL_MS)
#define	CT_TIMER_BINSIZE	((NUM_CT_ENTRIES * 1000) / (CT_SCAN_TIME_MS * CT_TICKS_PER_SECOND))
#define CT_INACTIVE_TIME	(1 * CT_TICKS_PER_SECOND)

#if ((CT_TICKS_PER_SECOND * CT_TIMER_INTERVAL) != TIMER_TICKS_PER_SEC)
#	error "TIMER_TICKS_PER_SEC has to be multiple of CT_TIMER_INTERVAL_MS"
#endif

#if !defined(COMCERTO_2000_CONTROL)
#if (((NUM_CT_ENTRIES/CT_TIMER_BINSIZE)*CT_TIMER_BINSIZE) != NUM_CT_ENTRIES)
#	error "NUM_CT_ENTRIES has to be multiple of CT_TIMER_BINSIZE"
#endif
#endif

#endif /* defined(COMCERTO_2000_CONTROL) || !defined(COMCERTO_2000) */

/******************************
* IPv4 API Command and Entry strutures
*
******************************/

#define CTCMD_FLAGS_ORIG_DISABLED	(1 << 0)
#define CTCMD_FLAGS_REP_DISABLED	(1 << 1)

typedef struct _tCtCommand {
	U16		action;
	U16		rsvd1;
	U32		Saddr;
	U32		Daddr;
	U16		Sport;
	U16		Dport;
	U32		SaddrReply;
	U32		DaddrReply;
	U16		SportReply;
	U16		DportReply;
	U16		protocol;
	U16		flags;
	U32		fwmark;
	U32		route_id;
	U32		route_id_reply;
}CtCommand, *PCtCommand;


/*Structure representing the command sent to add or remove a Conntrack when extentions (IPsec SA) is available*/
typedef struct _tCtExCommand {
	U16 		action;			/*Action to perform*/
	U16 		format;			/* bit 0 : indicates if SA info are present in command */ 		
							/* bit 1 : indicates if orig Route info is present in command  */ 		
							/* bit 2 : indicates if repl Route info is present in command  */ 		
	U32 		Saddr;			/*Source IP address*/
	U32 		Daddr;			/*Destination IP address*/
	U16 		Sport;			/*Source Port*/
	U16 		Dport;			/*Destination Port*/
	U32 		SaddrReply;
	U32 		DaddrReply;
	U16 		SportReply;
	U16 		DportReply;
	U16 		protocol;		/*TCP, UDP ...*/
	U16 		flags;
	U32 		fwmark;
	U32		route_id;
	U32		route_id_reply;
	// optional security parameters
	U8 		SA_dir; /* flow direction */
	U8 		SA_nr;
	U16 		SA_handle[4];
	U8 		SAReply_dir;
	U8 		SAReply_nr;
	U16 		SAReply_handle[4];
	U32 		tunnel_route_id;
	U32		tunnel_route_id_reply;
}CtExCommand, *PCtExCommand;

#define RTCMD_FLAGS_6o4		(1<<0)	/* A IPv4 tunnel destination address is present */
#define RTCMD_FLAGS_4o6		(1<<1)	/* A IPv6 tunnel destination address is present */

typedef struct _tRtCommand {
	U16		action;
	U16		mtu;
	U8		macAddr[ETHER_ADDR_LEN];
	U8		outputDevice[12];
	U32		id;
	U32		flags;
	/* Optional parameters */
	U32		daddr[4];
}RtCommand, *PRtCommand;

typedef struct _tTimeoutCommand {
	U16 	protocol;
	U16	sam_4o6_timeout;
	U32		timeout_value1;
	U32		timeout_value2;
}TimeoutCommand , *PTimeoutCommand;


typedef struct _tFFControlCommand {
	U16 enable;
	U16 reserved;
} FFControlCommand, *PFFControlCommand;		 	


typedef struct _tSockOpenCommand {
	U16		SockID;
	U8		SockType;
	U8		mode;		// 0 : not connected -> use 3 tuples.  1 : connected -> use 5 tuples
	U32		Saddr;
	U32		Daddr;
	U16		Sport;
	U16		Dport;
	U8		proto;
	U8		queue;
	U16		dscp;
	U32		route_id;
#ifdef COMCERTO_2000
	U16		secure;
	U16 		SA_nr_rx;
	U16 		SA_handle_rx[4];
	U16 		SA_nr_tx;
	U16 		SA_handle_tx[4];
	U16 		pad;
#endif
}SockOpenCommand, *PSockOpenCommand;

typedef struct _tSockCloseCommand {
	U16		SockID;
	U16		rsvd1;
}SockCloseCommand, *PSockCloseCommand;


typedef struct _tSockUpdateCommand {
	U16 SockID;
	U16 rsvd1;
	U32 Saddr;
	U16 Sport;
	U8   rsvd2;
	U8 	queue;
	U16 dscp;
	U16 pad;
	U32 route_id;
#ifdef COMCERTO_2000
	U16		secure;
	U16 		SA_nr_rx;
	U16 		SA_handle_rx[4];
	U16 		SA_nr_tx;
	U16 		SA_handle_tx[4];
	U16 		pad2;
#endif
}SockUpdateCommand, *PSockUpdateCommand;


/******************************
* IPv6 API Command and Entry strutures
*
******************************/
typedef struct _tCtCommandIPv6 {
	U16		action;
	U16		rsvd1;
	U32		Saddr[4];
	U32		Daddr[4];
	U16		Sport;
	U16		Dport;
	U32		SaddrReply[4];
	U32		DaddrReply[4];
	U16		SportReply;
	U16		DportReply;
	U16		protocol;
	U16		flags;
	U32		fwmark;
	U32		route_id;
	U32		route_id_reply;
}CtCommandIPv6, *PCtCommandIPv6;

typedef struct _tCtExCommandIPv6_old {
	U16		action;
	U16		format;	/* indicates if SA or tunnel info is present in command */ 
	U32		Saddr[4];
	U32		Daddr[4];
	U16		Sport;
	U16		Dport;
	U32		SaddrReply[4];
	U32		DaddrReply[4];
	U16		SportReply;
	U16		DportReply;
	U16		protocol;
	U16		flags;
	U32		fwmark;
	U32		route_id;
	U32		route_id_reply;
	U16 		SA_nr;
	U16 		SA_handle[4];
	U16 		SAReply_nr;
	U16 		SAReply_handle[4];
}CtExCommandIPv6_old, *PCtExCommandIPv6_old;

typedef struct _tCtExCommandIPv6 {
	U16		action;
	U16		format;	/* indicates if SA or tunnel info is present in command */ 
	U32		Saddr[4];
	U32		Daddr[4];
	U16		Sport;
	U16		Dport;
	U32		SaddrReply[4];
	U32		DaddrReply[4];
	U16		SportReply;
	U16		DportReply;
	U16		protocol;
	U16		flags;
	U32		fwmark;
	U32		route_id;
	U32		route_id_reply;
	U8 		SA_dir;
	U8 		SA_nr;
	U16 		SA_handle[4];
	U8 		SAReply_dir;
	U8 		SAReply_nr;
	U16 		SAReply_handle[4];
	U32 		tunnel_route_id;
	U32		tunnel_route_id_reply;
}CtExCommandIPv6, *PCtExCommandIPv6;

/* CtExCommand	FORMAT bitfield DEFINES*/ 
#define	CT_SECURE		(1 << 0)
#define	CT_ORIG_TUNNEL_SIT	(1 << 1)
#define	CT_REPL_TUNNEL_SIT	(1 << 2)
#define CT_ORIG_TUNNEL_4O6     CT_ORIG_TUNNEL_SIT
#define CT_REPL_TUNNEL_4O6     CT_REPL_TUNNEL_SIT

typedef struct _tSock6OpenCommand {
	U16		SockID;
	U8		SockType;
	U8		mode;		// 0 : not connected -> use 3 tuples.  1 : connected -> use 5 tuples
	U32		Saddr[4];
	U32		Daddr[4];
	U16		Sport;
	U16		Dport;
	U8		proto;
	U8		queue;
	U16		dscp;
	U32		route_id;
#ifdef COMCERTO_2000
	U16		secure;
	U16 		SA_nr_rx;
	U16 		SA_handle_rx[4];
	U16 		SA_nr_tx;
	U16 		SA_handle_tx[4];
	U16		pad;
#endif
}Sock6OpenCommand, *PSock6OpenCommand;

typedef struct _tSock6CloseCommand {
	U16		SockID;
	U16		rsvd1;
}Sock6CloseCommand, *PSock6CloseCommand;


typedef struct _tFragTimeoutCommand {
	U16		timeout;
	U16		mode;
}FragTimeoutCommand, *PFragTimeoutCommand;


typedef struct _tSock6UpdateCommand {
	U16 SockID;
	U16 rsvd1;
	U32 Saddr[4];
	U16 Sport;
	U8   rsvd2;
	U8 	queue;
	U16 dscp;
	U16 pad;
	U32	route_id;
#ifdef COMCERTO_2000
	U16		secure;
	U16 		SA_nr_rx;
	U16 		SA_handle_rx[4];
	U16 		SA_nr_tx;
	U16 		SA_handle_tx[4];
	U16 		pad2;
#endif
}Sock6UpdateCommand, *PSock6UpdateCommand;

/******************************
* TCP definitions
*
******************************/
typedef struct TCP_HDR_STRUCT
{
	unsigned short 	SourcePort;
	unsigned short 	DestinationPort;
	unsigned int	SequenceNumber;
	unsigned int	AckNumber;
	unsigned short	TcpFlags;
#ifdef ENDIAN_LITTLE
#define TCPFLAGS_FIN	htons(0x0001)
#define TCPFLAGS_SYN	htons(0x0002)
#define TCPFLAGS_RST	htons(0x0004)
#define TCPFLAGS_PSH	htons(0x0008)
#else
#define TCPFLAGS_FIN	0x0001
#define TCPFLAGS_SYN	0x0002
#define TCPFLAGS_RST	0x0004
#define TCPFLAGS_PSH	0x0008
#define TCPFLAGS_ACK	0x0010
#define TCPFLAGS_URG	0x0020
#define TCPFLAGS_ECE	0x0040
#define TCPFLAGS_CWR	0x0080
#define TCPFLAGS_DOFF(flags)	((flags & 0xF000) >> 12)
#endif
	unsigned short 	Window;
	unsigned short	Checksum;
	unsigned short 	UrgentPtr;
} tcp_hdr_t;

/******************************
* UDP definitions
*
******************************/
typedef struct UDP_HDR_STRUCT
{
	unsigned short SourcePort;
	unsigned short DestinationPort;
	unsigned short Length;
	unsigned short Chksum;
} udp_hdr_t;

/******************************
* Macros
*
******************************/

#define IS_IPV4(pEntry) ((((PCtEntry)pEntry)->status & CONNTRACK_IPv6) == 0)
#define IS_IPV6(pEntry) ((((PCtEntry)pEntry)->status & CONNTRACK_IPv6) != 0)

static __inline U32 HASH_RT(U32 id)
{
	U16 sum;
	U32 tmp32;
	tmp32 = ((id << 7) | (id >> 25));
	sum = (tmp32 >> 16) + (tmp32 & 0xffff);
	return (sum ^ (sum >> 8)) & (NUM_ROUTE_ENTRIES - 1);
}

#define IP6_LO_ADDR	3


#if defined(COMCERTO_2000)
static __inline void COPY_MACHEADER(void *pheader, void *pdstmacaddr, void *psrcmacaddr, U16 typeid)
{
	U16 *pto_U16;
	U16 *pdst_U16;
	U16 *psrc_U16;
	pto_U16 = (U16 *)pheader;
	pdst_U16 = (U16 *)pdstmacaddr;
	psrc_U16 = (U16 *)psrcmacaddr;

	pto_U16[0] = pdst_U16[0];
	pto_U16[1] = pdst_U16[1];
	pto_U16[2] = pdst_U16[2];
	pto_U16[3] = psrc_U16[0];
	pto_U16[4] = psrc_U16[1];
	pto_U16[5] = psrc_U16[2];
	pto_U16[6] = typeid;
}

static __inline void COPY_MACADDR(void *ptomacaddr, void *pfrommacaddr)
{
	((U16 *)ptomacaddr)[0] = ((U16 *)pfrommacaddr)[0];
	((U16 *)ptomacaddr)[1] = ((U16 *)pfrommacaddr)[1];
	((U16 *)ptomacaddr)[2] = ((U16 *)pfrommacaddr)[2];
}

static __inline int TESTEQ_MACADDR(void *pmacaddr1, void *pmacaddr2)
{
	return ((U16 *)pmacaddr1)[0] == ((U16 *)pmacaddr2)[0] &&
					((U16 *)pmacaddr1)[1] == ((U16 *)pmacaddr2)[1] &&
					((U16 *)pmacaddr1)[2] == ((U16 *)pmacaddr2)[2];
}

static __inline int TESTEQ_NULL_MACADDR(void *pmacaddr1)
{
	return ((U16 *)pmacaddr1)[0] == 0 && ((U16 *)pmacaddr1)[1] == 0 && ((U16 *)pmacaddr1)[2] == 0;
}

static __inline void COPY_MACADDR2(void *ptomacaddr, void *pfrommacaddr)
{
	U16 *pto;
	U16 *pfrom;
	pto = (U16 *)ptomacaddr;
	pfrom = (U16 *)pfrommacaddr;
	pto[0] = pfrom[0];
	pto[1] = pfrom[1];
	pto[2] = pfrom[2];
	pto[3] = pfrom[3];
	pto[4] = pfrom[4];
	pto[5] = pfrom[5];
}

static __inline int TESTEQ_MACADDR2(void *pmacaddr1, void *pmacaddr2)
{
	U16 *p1;
	U16 *p2;
	p1 = (U16 *)pmacaddr1;
	p2 = (U16 *)pmacaddr2;
	return p1[0] == p2[0] && p1[1] == p2[1] && p1[2] == p2[2] &&
					p1[3] == p2[3] && p1[4] == p2[4] && p1[5] == p2[5];
}

#else	// !defined(COMCERTO_2000)

static __inline void COPY_MACHEADER(void *pheader, void *pdstmacaddr, void *psrcmacaddr, U16 typeid)
{
#if ((BaseOffset & 0x3) == 0)
	U32 *pto_U32;
	U16 *pto_U16;
	U32 *pdst_U32;
	U16 *pdst_U16;
	U16 *psrc_U16;
	pto_U32 = (U32 *)pheader;
	pto_U16 = (U16 *)pheader;
	pdst_U32 = (U32 *)pdstmacaddr;
	pdst_U16 = (U16 *)pdstmacaddr;
	psrc_U16 = (U16 *)psrcmacaddr;
	WRITE_UNALIGNED_INT(pto_U32[0], READ_UNALIGNED_INT(pdst_U32[0]));
	WRITE_UNALIGNED_INT(pto_U32[1], (U32)pdst_U16[2] | ((U32)psrc_U16[0] << 16));
	WRITE_UNALIGNED_INT(pto_U32[2], (U32)psrc_U16[1] | ((U32)psrc_U16[2] << 16));
	pto_U16[6] = typeid;
#else
	U32 *pto_U32;
	U16 *pto_U16;
	U32 *pdst_U32;
	U32 *psrc_U32;
	U32 dmac0, dmac1;
	U32 smac0, smac1;
	pto_U16 = (U16 *)pheader;
	pto_U32 = (U32 *)(&pto_U16[1]);
	pdst_U32 = (U32 *)pdstmacaddr;
	psrc_U32 = (U32 *)psrcmacaddr;
	dmac0 = READ_UNALIGNED_INT(pdst_U32[0]);
	dmac1 = READ_UNALIGNED_INT(pdst_U32[1]);
	smac0 = READ_UNALIGNED_INT(psrc_U32[0]);
	smac1 = READ_UNALIGNED_INT(psrc_U32[1]);
	pto_U16[0] = (U16)(dmac0 & 0xFFFF);
	WRITE_UNALIGNED_INT(pto_U32[0], (dmac0 >> 16) | (dmac1 << 16));
	WRITE_UNALIGNED_INT(pto_U32[1], smac0);
	WRITE_UNALIGNED_INT(pto_U32[2], (smac1 & 0xFFFF) | ((U32)typeid << 16));
#endif
}

static __inline void COPY_MACADDR(void *ptomacaddr, void *pfrommacaddr)
{
	WRITE_UNALIGNED_INT(((U32 *)ptomacaddr)[0], READ_UNALIGNED_INT(((U32 *)pfrommacaddr)[0]));
	((U16 *)ptomacaddr)[2] = ((U16 *)pfrommacaddr)[2];
}

static __inline int TESTEQ_MACADDR(void *pmacaddr1, void *pmacaddr2)
{
	return READ_UNALIGNED_INT(((U32 *)pmacaddr1)[0]) == READ_UNALIGNED_INT(((U32 *)pmacaddr2)[0]) &&
					((U16 *)pmacaddr1)[2] == ((U16 *)pmacaddr2)[2];
}

static __inline int TESTEQ_NULL_MACADDR(void *pmacaddr1)
{
	return READ_UNALIGNED_INT(((U32 *)pmacaddr1)[0]) == 0 && ((U16 *)pmacaddr1)[2] == 0;
}

static __inline void COPY_MACADDR2(void *ptomacaddr, void *pfrommacaddr)
{
	PEthernetFrame pto;
	PEthernetFrame pfrom;
	pto = (PEthernetFrame)ptomacaddr;
	pfrom = (PEthernetFrame)pfrommacaddr;
	WRITE_UNALIGNED_INT(pto->dst_src_x[0], READ_UNALIGNED_INT(pfrom->dst_src_x[0]));
	WRITE_UNALIGNED_INT(pto->dst_src_x[1], READ_UNALIGNED_INT(pfrom->dst_src_x[1]));
	WRITE_UNALIGNED_INT(pto->dst_src_x[2], READ_UNALIGNED_INT(pfrom->dst_src_x[2]));
}

static __inline int TESTEQ_MACADDR2(void *pmacaddr1, void *pmacaddr2)
{
	PEthernetFrame p1;
	PEthernetFrame p2;
	p1 = (PEthernetFrame)pmacaddr1;
	p2 = (PEthernetFrame)pmacaddr2;
	return READ_UNALIGNED_INT(p1->dst_src_x[0]) == READ_UNALIGNED_INT(p2->dst_src_x[0]) &&
				READ_UNALIGNED_INT(p1->dst_src_x[1]) == READ_UNALIGNED_INT(p2->dst_src_x[1]) &&
				READ_UNALIGNED_INT(p1->dst_src_x[2]) == READ_UNALIGNED_INT(p2->dst_src_x[2]);
}
#endif


#if defined(COMCERTO_2000_CONTROL)

#define GET_TIMEOUT_VALUE(CtEntry,bidir_flag) get_timeout_value(CtEntry->proto,CtEntry->status & CONNTRACK_4O6,bidir_flag)
static __inline U32 get_timeout_value(U32 Proto,int sam_flag, int bidir_flag)
{
	switch (Proto)
	{
		case IPPROTO_UDP:
			if(sam_flag)
				return bidir_flag ? udp_4o6_bidir_timeout : udp_4o6_unidir_timeout;
			return bidir_flag ? udp_bidir_timeout : udp_unidir_timeout;
		break;

		case IPPROTO_TCP:
			return sam_flag ? tcp_4o6_timeout: tcp_timeout;
		break;

		case IPPROTO_IPIP:
		default:
			return sam_flag ? other_4o6_proto_timeout : other_proto_timeout;
		break;
	}
}
#elif (!defined(COMCERTO_2000))
#define GET_TIMEOUT_VALUE(CtEntry,bidir_flag) get_timeout_value(CtEntry->status & CONNTRACK_PROTO_MASK , CtEntry->status & CONNTRACK_4O6 ,bidir_flag)

static __inline U32 get_timeout_value(U32 proto_flag, int sam_flag,int bidir_flag)
{
	if(!SAM_4o6flag)
	{
		if((proto_flag & CONNTRACK_UDP) && bidir_flag) 
		{
			return udp_bidir_timeout;
		}
		else
			return ct_proto_timeout[proto_flag];
	}
	else
	{
                if ((proto_flag & CONNTRACK_UDP) && bidir_flag)
                {
                        return udp_4o6_bidir_timeout;
                }
                return sam_4o6_proto_timeout[proto_flag];
	}
}
#endif


static __inline int HASH_IPSEC_SOCK(U32 Saddr, U32 Daddr, U16 Sport, U16 Dport) 
{                                                                               
        U32 tmp32;                                                              
        U16 sum;                                                                
        U8 hash;                                                                
        tmp32 = Saddr + ((Daddr << 7) | (Daddr >> 25));                         
        tmp32 += Sport + ((Dport << 11) | (Dport >> 5)); /* FIXME Dport is only 16bit */
        sum = (tmp32 >> 16) + (tmp32 & 0xffff) ;                                
        hash = (sum >> 8) ^ (sum & 0xFF);                                       
        return (hash ^ (hash >> 4)) & (NUM_IPSEC_SOCK_ENTRIES - 1);             
}                                                                              

#endif /* _FE_H_ */
