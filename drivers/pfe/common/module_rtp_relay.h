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

#ifndef _MODULE_RTP_RELAY_H_
#define _MODULE_RTP_RELAY_H_

#include "module_timer.h"
#include "fe.h"
#include "mtd.h"

#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
#include "module_ipv4.h"
#include "module_mc4.h"
#include "module_mc6.h"
#endif


#if defined(COMCERTO_2000)
#define NUM_RTPFLOW_ENTRIES 	64
#else
#define NUM_RTPFLOW_ENTRIES 	256
#endif

#define MAX_RTP_STATS_ENTRY 64

#define RTP_SPECIAL_PAYLOAD_LEN	160

#define RTP_MIN_SEQUENTIAL	2
#define RTP_MAX_DROPOUT 	3000
#define RTP_LATE_THRESH 	0xFF00
#define RTP_SEQ_MOD			1 << 16

#define RTP_MAX_SEQNUM	0xFFFF

enum {
	RCVD_OK,
	RCVD_ERR,
	RCVD_FIRST_PACKET,				// 1st received packet
	RCVD_IN_ORDER_PACKET,			// sequential packet
	RCVD_OUT_OF_ORDER_PACKET,		// out-of-order packet (could be late or duplicated)
	RCVD_FIRST_UNEXPECTED_PACKET,	// large jump of seq.
	RCVD_SECOND_SEQUENTIAL_PACKET,	// 2nd of the new seq.
	RCVD_LATE_PACKET,
	RCVD_DUPLICATED_PACKET,
	RCVD_NEW_SSRC_DECTECTED,
	INVALID
};


typedef enum {
	RTCP_SR		= 200,
	RTCP_RR		= 201,
	RTCP_SDES	= 202,
	RTCP_BYE	= 203,
	RTCP_APP	= 204,
	RTCP_XR	= 207
}rtcp_type_t;


typedef enum {
	RTP_PT_G711U	= 0,
	RTP_PT_G711A	= 8,
} rtp_pt_t;


#ifdef ENDIAN_LITTLE
typedef struct RTP_HDR_STRUCT
{
	U8	cc:4;		// CSRC count
	U8	x:1;		// header extension flag
	U8	p:1;		// padding flag
	U8	version:2;	// protocol version
	U8	pt:7;		// payload type
	U8	m:1;		// marker bit
	U16 seq;		// sequence number
	U32 ts;			// timestamp
	U32 ssrc;		// synchronization source
} __attribute__((packed)) rtp_hdr_t;
#else
typedef struct RTP_HDR_STRUCT
{
	U8	version:2;	// protocol version
	U8	p:1;		// padding flag
	U8	x:1;		// header extension flag
	U8	cc:4;		// CSRC count
	U8	m:1;		// marker bit
	U8	pt:7;		// payload type
	U16 seq;		// sequence number
	U32 ts;			// timestamp
	U32 ssrc;		// synchronization source
} __attribute__((packed)) rtp_hdr_t;
#endif


#define  RTP_HDR_SIZE	12

#define RTP_DISCARD	0x0
#define RTP_RELAY		0x1

#if defined(COMCERTO_2000)
#define FLOW_VALID		(1 << 0)
#define FLOW_USED		(1 << 1)
#define FLOW_UPDATING	(1 << 2)
#endif



typedef struct _tRTPinfo {
	BOOL  	first_packet;
	BOOL	probation;
	U16	   	last_Seq;
	STIME	last_rx_time;
	U32	   	last_TS;
	U32	   	last_SSRC;
	U32		last_transit;
	U16		seq_base;
	U16		mode;
	U32		cycles;
}RTPinfo, *PRTPinfo;



#if defined (COMCERTO_2000)

#if defined (COMCERTO_2000_CONTROL)

/* control path HW flow entry */
typedef struct _thw_rtpflow {
	U32 	flags;
	U32 	dma_addr;
	U32		next;

	struct _tRTPinfo	rtp_info;

	U16	   	ingress_socketID;
	U16	   	egress_socketID;
	U8	   	state; 
	BOOL  	takeover_resync;
	U8		SSRC_takeover;
	U8		Seq_takeover;
	U8		TS_takeover;
	U8		call_update_seen;
	U16 	hash;
	U32	   	SSRC;
	U32	   	TimestampBase;
	U32	   	TimeStampIncr;
	U16	   	Seq;
	U8		takeover_mode;
	U8		Special_tx_active;
	U8		Special_tx_type;

	U8      pad[7]; /* padding to get first part of the flow structure multiple of 64bits */

	/* following part 2*160 bytes is fetched (efet) only if special packet feature is active */
	U8		Special_payload1[RTP_SPECIAL_PAYLOAD_LEN];
	U8		Special_payload2[RTP_SPECIAL_PAYLOAD_LEN];
	
	/* These fields are only used by host software (not fetched by data path), so keep them at the end of the structure */
	struct dlist_head 	list;
	struct _tRTPflow	*sw_flow;	/** pointer to the software flow entry */
	unsigned long		removal_time;
}hw_rtpflow, *Phw_rtpflow;

/* control path SW flow entry */
typedef struct _tRTPflow {
	struct slist_entry	list;
	struct _tRTPflow *next;
	struct _tRTPcall *RTPcall;

	struct _tRTPinfo	rtp_info;

	U16	   	ingress_socketID;
	U16	   	egress_socketID;
	U8	   	state; 			// 0 discard, 1 relay
	BOOL  	takeover_resync;
	U8		SSRC_takeover;	// 0 transparent / 1 mangle
	U8		Seq_takeover;   // 0 transparent / 1 mangle
	U8		TS_takeover;     // 0 transparent / 1 mangle	
	U8		takeover_mode;
	U16 	hash;
	U32	   	SSRC;
	U32	   	TimestampBase;
	U32	   	TimeStampIncr;
	U16	   	Seq;

	struct _thw_rtpflow *hw_flow;
}RTPflow, *PRTPflow;

#else

/* data path rtp flow entry for util-pe (MUST have a size multiple of 64bits) */
typedef struct _tRTPflow {
	U32 	flags;
	U32 	dma_addr;
	U32		next;

	struct _tRTPinfo	rtp_info;

	U16	   	ingress_socketID;
	U16	   	egress_socketID;
	U8	   	state;
	BOOL  	takeover_resync;
	U8		SSRC_takeover;
	U8		Seq_takeover;
	U8		TS_takeover;
	U8		call_update_seen;
	U16 	hash;
	U32	   	SSRC;
	U32	   	TimestampBase;
	U32	   	TimeStampIncr;
	U16	   	Seq;
	U8		takeover_mode;
	U8		Special_tx_active;
	U8		Special_tx_type;

	U8      pad[7]; /* padding to get first part of the flow structure multiple of 64bits */

	/* following part 2*160 bytes is fetched (efet) only if special packet feature is active */
	U8		Special_payload1[RTP_SPECIAL_PAYLOAD_LEN];
	U8		Special_payload2[RTP_SPECIAL_PAYLOAD_LEN];
}RTPflow, *PRTPflow;

#endif

#else /* COMCERTO _1000 */

typedef struct _tRTPflow {
	struct slist_entry	list;
	struct _tRTPflow *next;
	struct _tRTPcall *RTPcall;

	struct _tRTPinfo	rtp_info;

	U16	   	ingress_socketID;
	U16	   	egress_socketID;
	U8	   	state; 			// 0 discard, 1 relay

	BOOL  	takeover_resync;
	U8		SSRC_takeover;	// 0 transparent / 1 mangle
	U8		Seq_takeover;   // 0 transparent / 1 mangle
	U8		TS_takeover;     // 0 transparent / 1 mangle	
	U8		takeover_mode;
	U32	   	SSRC;
	U32	   	TimestampBase;
	U32	   	TimeStampIncr;
	U32		cycles;
	U8		call_update_seen;
}RTPflow, *PRTPflow;
#endif


#if !defined(COMCERTO_2000) || defined (COMCERTO_2000_CONTROL)
typedef struct _tRTPcall {
	struct slist_entry	list;
	U16		  call_id;	// unique id
	U16		  valid;
	struct _tRTPflow	*AtoB_flow;
	struct _tRTPflow	*BtoA_flow;	
	U8		Special_payload1[RTP_SPECIAL_PAYLOAD_LEN];
	U8		Special_payload2[RTP_SPECIAL_PAYLOAD_LEN];
	U8		Next_Special_payload1[RTP_SPECIAL_PAYLOAD_LEN];
	U8		Next_Special_payload2[RTP_SPECIAL_PAYLOAD_LEN];
	U8		Special_tx_active;
	U8		Special_tx_type;
}RTPCall, *PRTPCall;
#else
typedef struct _tRTPcall {
	U8		Special_payload1[RTP_SPECIAL_PAYLOAD_LEN];
	U8		Special_payload2[RTP_SPECIAL_PAYLOAD_LEN];
}RTPCall, *PRTPCall;
#endif

typedef struct _tRTCPStats {
	U64		average_reception_period;
	U32		prev_reception_period;
	U32		last_reception_period;
	U32	   	num_tx_pkts;
	U32	   	num_rx_pkts;
	U32	   	num_rx_pkts_in_seq;
	U32	   	last_TimeStamp;
	U32		packets_duplicated;
	U32		num_rx_since_RTCP;
	U32		num_tx_bytes;
	U32		min_jitter;
	U32		max_jitter;
	U32		mean_jitter;
	U32		num_rx_lost_pkts;
	U32		min_reception_period;
	U32		max_reception_period;
	U32		num_expected_pkts;
	U32		num_malformed_pkts;
	U32		cycles;
	U32		num_late_pkts;
	STIME	last_tx_time;
	U32		num_big_jumps;
	U32		num_previous_rx_lost_pkts;
	U16		sport;
	U16		dport;
	U16		last_rx_Seq;
	U16		seq_base;
	U16		state;
	U8		first_received_RTP_header[RTP_HDR_SIZE];
	U32		ssrc_overwrite_value; /* make sure to get multiple of 64bits */
}RTCPStats, *PRTCPStats;

typedef struct _tRTPOpenCommand {
	U16		CallID;
	U16		SocketA;
	U16		SocketB;
	U16		rsvd1;
}RTPOpenCommand, *PRTPOpenCommand;

typedef struct _tRTPCloseCommand {
	U16		CallID;
	U16		rsvd1;
}RTPCloseCommand, *PRTPCloseCommand;

typedef struct _tRTPTakeoverCommand {
	U16		CallID;
	U16		Socket;
	U16 		mode;
	U16		SeqNumberBase;
	U32		SSRC;
	U32 		TimeStampBase;
	U32		TimeStampIncr;
}RTPTakeoverCommand, *PRTPTakeoverCommand;


/* bit field definition for takeover modes */
#define RTP_TAKEOVER_MODE_TSINCR_FREQ	1
#define RTP_TAKEOVER_MODE_SSRC			2

#define RTP_TAKEOVER_SSRC_TRANSPARENT	0
#define RTP_TAKEOVER_SSRC_MANGLE			1
#define RTP_TAKEOVER_SSRC_AUTO			2

typedef struct _tRTPControlCommand {
	U16		CallID;
	U16		ControlDir;
}RTPControlCommand, *PRTPControlCommand;

#define RTP_SPEC_TX_START	0
#define RTP_SPEC_TX_RESPONSE	1
#define RTP_SPEC_TX_STOP	2
#define RTP_SPEC_TX_START_ONE_SHOT	3


typedef struct _tRTPSpecTxCtrlCommand {
	U16		CallID;
	U16		Type;	
}RTPSpecTxCtrlCommand, *PRTPSpecTxCtrlCommand;

typedef struct _tRTPSpecTxPayloadCommand {
	U16		CallID;
	U16		payloadID;	
	U16		payloadLength;	
	U16		payload[RTP_SPECIAL_PAYLOAD_LEN/2];
}RTPSpecTxPayloadCommand, *PRTPSpecTxPayloadCommand;


typedef struct _tRTCPQueryCommand {
	U16		SocketID;
	U16		flags;
}RTCPQueryCommand, *PRTCPQueryCommand;

typedef struct _tRTCPQueryResponse {
	U32		prev_reception_period;
	U32		last_reception_period;
	U32	   	num_tx_pkts;
	U32	   	num_rx_pkts;
	U32	   	last_rx_Seq;
	U32	   	last_TimeStamp;
	U8		RTP_header[RTP_HDR_SIZE];
	U32		num_rx_dup;
	U32		num_rx_since_RTCP;
	U32		num_tx_bytes;
	U32		min_jitter;
	U32		max_jitter;
	U32		mean_jitter;
	U32		num_rx_lost_pkts;
	U32		min_reception_period;
	U32		max_reception_period;
	U32		average_reception_period;
	U32		num_malformed_pkts;
	U32		num_expected_pkts;
	U32		num_late_pkts;
	U16		sport;
	U16		dport;
	U32		num_cumulative_rx_lost_pkts;
	U32		ssrc_overwrite_value;
}RTCPQueryResponse, *PRTCPQueryResponse;

/**************** RTP Statistics for FF connections *******************/

#define RTP_STATS_FREE 0xFFFF

#define RTP_STATS_FULL_RESET 	1
#define RTP_STATS_PARTIAL_RESET 2
#define RTP_STATS_RX_RESET      3
#define RTP_STATS_FIRST_PACKET  4

#define IP4	0
#define IP6	1
#define MC4	2
#define MC6	3
#define RLY	4
#define RLY6	5



#if defined(COMCERTO_2000)
#define RTPQOS_VALID		(1 << 0)
#define RTPQOS_USED			(1 << 1)
#define RTPQOS_UPDATING		(1 << 2)
#endif


#if defined (COMCERTO_2000)

#if defined (COMCERTO_2000_CONTROL)

/* control path HW rtp qos stats entry */
typedef struct _thw_rtpqos_entry
{
	U32 	flags;
	U32 	dma_addr;
	U32 	next;
	U16		stream_id;
	U16		stream_type;
	U32		saddr[4];
	U32		daddr[4];
	U16		sport;
	U16		dport;
	U16		proto;
	U16		slot;
	struct _tRTPinfo	rtp_info;
	struct _tRTCPStats	stats;

	/* HOST only , these fields are only used by host software, so keep them at the end of the structure */
	struct dlist_head	list;
	struct _trtpqos_entry *sw_rtpqos;
	unsigned long		removal_time;
} hw_RTPQOS_ENTRY, *hw_PRTPQOS_ENTRY;


/* control path SW rtp qos stats entry */
typedef struct _trtpqos_entry
{
	struct slist_entry 	list;
	U16		stream_id;
	U16		stream_type;
	U32		saddr[4];
	U32		daddr[4];
	U16		sport;
	U16		dport;
	U16		proto;
	U16		slot;
	struct _tRTPinfo	rtp_info;
	struct _tRTCPStats	stats;
	struct _thw_rtpqos_entry   *hw_rtpqos;
} RTPQOS_ENTRY, *PRTPQOS_ENTRY;

#else

/* data path rtp qos stats entry for util-pe (MUST have a size multiple of 64bits) */
typedef struct _trtpqos_entry
{
	U32 	flags;
	U32 	dma_addr;
	U32 	next;
	U16		stream_id;
	U16		stream_type;
	U32		saddr[4];
	U32		daddr[4];
	U16		sport;
	U16		dport;
	U16		proto;
	U16		slot;
	struct _tRTPinfo	rtp_info;
	struct _tRTCPStats	stats;
} RTPQOS_ENTRY, *PRTPQOS_ENTRY;

#endif

#else

/* C1000 */
typedef struct _trtpqos_entry
{
	struct slist_entry 	list;
	U32 	flags;
	U16		stream_id;
	U16		stream_type;
	U32		saddr[4];
	U32		daddr[4];
	U16		sport;
	U16		dport;
	U16		proto;
	U16		slot;
	struct _tRTPinfo	rtp_info;
	struct _tRTCPStats	stats;
} RTPQOS_ENTRY, *PRTPQOS_ENTRY;

#endif

typedef struct _tRTP_enable_stats_command {
	U16 stream_id;
	U16 stream_type;
	U32 saddr[4];
	U32 daddr[4];
	U16 sport;
	U16 dport;
	U16 proto;
	U16 mode;
}RTP_ENABLE_STATS_COMMAND, *PRTP_ENABLE_STATS_COMMAND;

typedef struct _tRTP_disable_stats_command {
	U16 stream_id;
}RTP_DISABLE_STATS_COMMAND, *PRTP_DISABLE_STATS_COMMAND;

typedef struct _tRTP_query_stats_command {
	U16 stream_id;
	U16 flags;
}RTP_QUERY_STATS_COMMAND, *PRTP_QUERY_STATS_COMMAND;

typedef struct _tRTP_dmtf_pt_command {
	U16 pt;
}RTP_DTMF_PT_COMMAND, *PRTP_DTMF_PT_COMMAND;

#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
extern struct _trtpqos_entry rtpqos_cache[MAX_RTP_STATS_ENTRY];
#else
extern PVOID rtpqos_cache[MAX_RTP_STATS_ENTRY];
#endif

extern U8 gDTMF_PT[2];

#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
extern struct slist_head rtpflow_cache[];
extern struct slist_head rtpcall_cache;
#else
extern PVOID rtpflow_cache[];
#endif

void RTP_compute_stats(PMetadata mtd, udp_hdr_t *pUdpHdr, rtp_hdr_t *pRtpHeader, U8 slot);
void RTP_clear_stats(PRTCPStats pStats, U8 type);

#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
int rtpqos_ipv4_link_stats_entry_by_tuple(PCtEntry pClient, U32 saddr, U32 daddr, U16 sport, U16 dport);
int rtpqos_ipv6_link_stats_entry_by_tuple(PCtEntryIPv6 pClient, U32 *saddr, U32 *daddr, U16 sport, U16 dport);
int rtpqos_mc4_link_stats_entry_by_tuple(PMC4Entry pClient, U32 saddr, U32 daddr);
int rtpqos_mc6_link_stats_entry_by_tuple(PMC6Entry pClient, U32 *saddr, U32 *daddr);
int rtpqos_relay_link_stats_entry_by_tuple(PSockEntry pSocket, U32 saddr, U32 daddr, U16 sport, U16 dport);
int rtpqos_relay6_link_stats_entry_by_tuple(PSock6Entry pSocket, U32 *saddr, U32 *daddr, U16 sport, U16 dport);
#endif

BOOL rtp_relay_init(void);
void rtp_relay_exit(void);

#if defined(COMCERTO_2000_UTIL)
#include "module_socket.h"
#include "util_dmem_storage.h"
void M_rtp_receive(PMetadata mtd);
void M_rtp_rx_handler(PMetadata mtd);
void M_rtp_qos_stats_handler(PMetadata mtd);
void RTP_tx_special_packet(PSockEntry pSocket, U8* rtpheader, U8* payload);
#else
void M_rtp_receive(PMetadata mtd) __attribute__((section ("fast_path")));
void M_rtp_entry(void) __attribute__((section ("fast_path")));
#endif

PRTPflow RTP_find_flow(U16 in_socket);


static __inline U32 HASH_RTP(U16 socketID)
{
	return (((socketID & 0xff) ^ (socketID >> 8)) & (NUM_RTPFLOW_ENTRIES - 1));
}

static inline U32 x1000(U32 x)
{
    U32 x125;
    x125 = x + (x << 7) - (x << 2);  // x + 128*x - 4*x => 125*x
    return x125 << 3;                // 8*(125*x) => 1000*x
}



#endif /* _MODULE_RTP_RELAY_H_ */
