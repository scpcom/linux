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


#ifndef _MODULE_BRIDGE_H_
#define _MODULE_BRIDGE_H_

#include "types.h"
#include "channels.h"
#include "fe.h"
#include "ipv6.h"


/* Modes */
#define L2_BRIDGE_MODE_MANUAL	0	
#define L2_BRIDGE_MODE_AUTO		1

/* Timer */
#define L2_BRIDGE_DEFAULT_TIMEOUT			30
#define L2_BRIDGE_TIMER_INTERVAL			10
#define L2_BRIDGE_TICKS_PER_SECOND		(TIMER_TICKS_PER_SEC / L2_BRIDGE_TIMER_INTERVAL)
#define L2_BRIDGE_TIMER_BINSIZE			16
#define L2_BRIDGE_TIMER_NUMBINS			(NUM_BT_ENTRIES/L2_BRIDGE_TIMER_BINSIZE)

#define L2_BRIDGE_L3_TIMER_BINSIZE			16
#define L2_BRIDGE_L3_TIMER_NUMBINS		(NUM_BT_L3_ENTRIES/L2_BRIDGE_TIMER_BINSIZE)

/* Status */
#define L2_BRIDGE_TIMED_OUT				0x1

#define L2FLOW_UPDATING				(1 << 0)

/*
 * L2 Bridge Table structure
 */
#if !defined(COMCERTO_2000)	

typedef struct _tL2Bridge_entry {
	struct tL2Bridge_entry *next;
	U8 da[6];
	U8 sa[6];
	U16 ethertype;
	U16 input_vlan;
	U16 output_vlan;
    U16 session_id;
	struct itf *output_itf;
        struct {
            U32 pkt_priority : 8;
            U32 vlan_priority : 3;
            U32 vlan_priority_mark : 1;
            U32 sa_wc : 1;
            U32 qmod : 1;
            U32 unused : 2;
            U32 input_interface : 8;
            U32 unused1 : 8;
        } __attribute__((packed));
#ifdef CFG_STATS
	U32 total_packets_transmitted;
#endif
//	U8 input_name[16];
//	U8 output_name[16];
} L2Bridge_entry, *PL2Bridge_entry;

#else	// defined(COMCERTO_2000)

struct _tbridge_enable {
	U8 used;
	U8 enabled;
	U16 onif_index;
};

#if defined (COMCERTO_2000_CONTROL)

/* control path SW bridge entry */
typedef struct _tL2Bridge_entry {
	struct slist_entry	list;
	U32	valid;
	U8 	pad1[6];
	U16	hash;
	U8 da[6];
	U8 sa[6];
	U16 ethertype;
	U16 input_svlan;
	U16 input_cvlan;
	U16 output_svlan;
	U16 output_cvlan;
    U16 session_id;
	struct itf *output_itf;
    struct {
            U32 pkt_priority : 8;
            U32 svlan_priority : 3;
            U32 svlan_priority_mark : 1;
            U32 cvlan_priority : 3;
            U32 cvlan_priority_mark : 1;
            U32 sa_wc : 1;
            U32 qmod : 1;
            U32 unused : 6;
            U32 input_interface : 8;
        } __attribute__((packed));
#ifdef CFG_STATS
	U32 total_packets_transmitted;
#endif
	U8 pad2[16];

	struct _thw_L2Bridge_entry *hw_l2bridge;
} L2Bridge_entry, *PL2Bridge_entry;

/* control plane HW bridge entry */
typedef struct _thw_L2Bridge_entry {
	U32 	flags;
	U32 	dma_addr;
	U32		next;
	
	U32	valid;
	U16	pad1;
	U16	hash;
	U8 da[6];
	U8 sa[6];
	U16 ethertype;
	U16 input_svlan;
	U16 input_cvlan;
	U16 output_svlan;
	U16 output_cvlan;
	U16 session_id;
	struct itf *output_itf;
	struct {
		U32 pkt_priority : 8;
		U32 svlan_priority : 3;
		U32 svlan_priority_mark : 1;
		U32 cvlan_priority : 3;
		U32 cvlan_priority_mark : 1;
		U32 sa_wc : 1;
		U32 qmod : 1;
		U32 unused : 6;
		U32 unused1 : 8;
	} __attribute__((packed));
	#ifdef CFG_STATS
	U32 total_packets_transmitted[NUM_PE_CLASS];
	#endif
	//U32 pad3[3];

	/* These fields are only used by host software (not fetched by data path), so keep them at the end of the structure */
	struct dlist_head 			list;
	struct _tL2Bridge_entry	*sw_l2bridge;	/** pointer to the software bridge entry */
	unsigned long				removal_time;
} hw_L2Bridge_entry, *Phw_L2Bridge_entry;

#else	// !defined (COMCERTO_2000_CONTROL)
/* data path HW bridge entry */
typedef struct _tL2Bridge_entry {
	U32 	flags;
	U32 	dma_addr;
	U32		next;

	U32	valid;
	U16	pad1;
	U16	hash;
	U8 da[6];
	U8 sa[6];
	U16 ethertype;
	U16 input_svlan;
	U16 input_cvlan;
	U16 output_svlan;
	U16 output_cvlan;
    U16 session_id;
	struct itf *output_itf;
	struct {
		U32 pkt_priority : 8;
		U32 unused : 2;
		U32 qmod : 1;
		U32 sa_wc : 1;
		U32 svlan_priority_mark : 1;
		U32 svlan_priority : 3;
		U32 cvlan_priority_mark : 1;
		U32 cvlan_priority : 3;
		U32 unused1 : 4;
		U32 unused2 : 8;
        } __attribute__((packed));
#ifdef CFG_STATS
	U32 total_packets_transmitted[NUM_PE_CLASS];
#endif
	//U32 pad3[3];
} L2Bridge_entry, *PL2Bridge_entry;
#endif	// !defined (COMCERTO_2000_CONTROL)

#endif	// defined(COMCERTO_2000)

/*
 * L2/L3 Bridge structures
 */

struct L2Flow{
	U8 da[6];
	U8 sa[6];
	U16 ethertype;
	U16 session_id;
	U16 svlan_tag; /* TCI */
	U16 cvlan_tag; /* TCI */
};

struct L2Flow_tmp{
	U8 *da_sa;
	U16 ethertype;
	U16 session_id;
	U16 input_ifindex;
	U16 svlan_tag; /* TCI */
	U16 cvlan_tag; /* TCI */
};

struct L3Flow{
	U32		saddr[4];
	U32		daddr[4];
	U16		sport;
	U16		dport;
	U8		proto;
};

struct l2_qos_mark{
#ifdef  ENDIAN_LITTLE
	U16 queue : 5;
	U16 vlan_pbits : 3;
	U16 qmod_flag : 1;
	U16 vlan_prio_inh_flag : 1;
	U16 queue_from_vlan_flag : 1;
	U16 unused : 5;
#else
	U16 unused : 5;
	U16 queue_from_vlan_flag : 1;
	U16 vlan_prio_inh_flag : 1;
	U16 qmod_flag : 1;
	U16 vlan_pbits : 3;
	U16 queue : 5;
#endif
} __attribute__((packed));


#if !defined(COMCERTO_2000)
typedef struct _tL2Flow_entry {
	struct slist_entry 	list;
	struct L2Flow	l2flow;
	U32 last_l2flow_timer;
       U8 input_if;
       U8 output_if;	
	U16 status;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
	U16	nb_l3_ref;
	void  *output_if_h;
	U8 output_if_type;
} L2Flow_entry, *PL2Flow_entry;

typedef struct _tL3Flow_entry {
	struct slist_entry 	list;
	struct L3Flow	l3flow;
	PL2Flow_entry l2flow_entry;
	U32 last_l3flow_timer;
	U16 status;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
} L3Flow_entry, *PL3Flow_entry;
#endif

#if defined (COMCERTO_2000_CONTROL)

/* control path SW L2 flow entry */
typedef struct _tL2Flow_entry {
	struct slist_entry 	list;
	struct L2Flow	l2flow;
	U32 last_l2flow_timer;
	struct itf *output_if;
	U16 input_ifindex;
	U16 status;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
	U16	nb_l3_ref;
	struct _thw_L2Flow_entry   *hw_l2flow;	/** pointer to the hardware flow entry */
} L2Flow_entry, *PL2Flow_entry;

/* control path SW L3 flow entry */
typedef struct _tL3Flow_entry {
	struct slist_entry 	list;	
	struct L3Flow	l3flow;
	PL2Flow_entry l2flow_entry;
	U32 last_l3flow_timer;
	U16 status;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
	struct _thw_L3Flow_entry   *hw_l3flow;	/** pointer to the hardware flow entry */
} L3Flow_entry, *PL3Flow_entry;

/* control path HW L2 flow entry */
typedef struct _thw_L2Flow_entry {
	U32 	flags;
	U32 	dma_addr;
	U32		next;
	U32 	active;
	struct L2Flow	l2flow;
	struct itf *output_if;
	U16 input_ifindex;
	U16 status;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
	U16	nb_l3_ref;

	/* These fields are only used by host software (not fetched by data path), so keep them at the end of the structure */
	struct dlist_head 		list;
	struct _tL2Flow_entry	*sw_l2flow;	/** pointer to the software L2 flow entry */
	unsigned long			removal_time;
} hw_L2Flow_entry, *Phw_L2Flow_entry;

/* control path HW L3 flow entry */
typedef struct _thw_L3Flow_entry {
	U32 	flags;
	U32 	dma_addr;
	U32		next;
	U32 	active;
	struct L3Flow	l3flow;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
	
	/* These fields are only used by host software (not fetched by data path), so keep them at the end of the structure */
	struct dlist_head 		list;
	struct _tL3Flow_entry	*sw_l3flow;	/** pointer to the software L3 flow entry */
	unsigned long			removal_time;
} hw_L3Flow_entry, *Phw_L3Flow_entry;


void Bridge_output_interface_reset(U32 if_index);

#else	// !defined (COMCERTO_2000_CONTROL)

/* data path HW L2 flow entry */
typedef struct _tL2Flow_entry {
	U32 	flags;
	U32 	dma_addr;
	U32		next;
	U32 	active;
	struct L2Flow	l2flow;
	struct itf *output_if;
	U16 input_ifindex;
	U16 status;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
	U16	nb_l3_ref;
} L2Flow_entry, *PL2Flow_entry;

/* data path HW L3 flow entry */
typedef struct _tL3Flow_entry {
	U32 	flags;
	U32 	dma_addr;
	U32		next;
	U32		active;
	struct L3Flow	l3flow;
	union {
		U16 mark;
		struct l2_qos_mark qos_mark;
	};
} L3Flow_entry, *PL3Flow_entry;
#endif	// !defined (COMCERTO_2000_CONTROL)

#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
extern struct slist_head bridge_cache[];
extern struct slist_head bridge_l3_cache[];
#else
extern PVOID bridge_cache[];
extern PVOID bridge_l3_cache[];
#endif

/*
 * Commands
 */

/* L2 Bridging Enable command */
typedef struct _tL2BridgeEnableCommand {
	U16 interface;
	U16 enable_flag;
	U8 input_name[16];
}L2BridgeEnableCommand, *PL2BridgeEnableCommand;

/* L2 Bridging Add Entry command */
typedef struct _tL2BridgeAddEntryCommand {
	U16 input_interface;
	U16 input_svlan;
	U16 input_cvlan;
	U8 destaddr[6];
	U8 srcaddr[6];
	U16 ethertype;
	U16 output_interface;
	U16 output_svlan;
	U16 output_cvlan;
	U16 pkt_priority;
	U16 svlan_priority;
	U16 cvlan_priority;
	U8 input_name[16];
	U8 output_name[16];
	U16 qmod;
	U16 session_id;
}L2BridgeAddEntryCommand, *PL2BridgeAddEntryCommand;

/* L2 Bridging Remove Entry command */
typedef struct _tL2BridgeRemoveEntryCommand {
	U16 input_interface;
	U16 input_svlan;
	U16 input_cvlan;
	U8 destaddr[6];
	U8 srcaddr[6];
	U16 ethertype;
	U16 session_id;
	U16 reserved;
	U8 input_name[16];
}L2BridgeRemoveEntryCommand, *PL2BridgeRemoveEntryCommand;

/* L2 Bridging Query Status response */
typedef struct _tL2BridgeQueryStatusResponse {
	U16 ackstatus;
	U16 status;
	U8  ifname[16];
	U32 eof;
}L2BridgeQueryStatusResponse, *PL2BridgeQueryStatusResponse;

#define BRIDGE_QUERY_NOT_READY	0
#define BRIDGE_QUERY_READY	1

/* L2 Bridging Query Entry response */
typedef struct _tL2BridgeQueryEntryResponse {
	U16 ackstatus;
	U16 eof;
	U16 input_interface;
	U16 input_svlan;
	U16 input_cvlan;
	U8 destaddr[6];
	U8 srcaddr[6];
	U16 ethertype;
	U16 output_interface;
	U16 output_svlan;
	U16 output_cvlan;
	U16 pkt_priority;
	U16 svlan_priority;
	U16 cvlan_priority;
	U8 input_name[16];
	U8 output_name[16];
	U16 qmod;
	U16 session_id;
}L2BridgeQueryEntryResponse, *PL2BridgeQueryEntryResponse;

/* L2 Bridging  Flow entry command */
typedef struct _tL2BridgeL2FlowEntryCommand {
	U16		action;				/*Action to perform*/
	U16		ethertype;			/* If VLAN Tag !=0, ethertype of next header */
	U8		destaddr[6];			/* Dst MAC addr */
	U8		srcaddr[6];			/* Src MAC addr */
	U16		svlan_tag; 			/* S TCI */
	U16		cvlan_tag; 			/* C TCI */
	U16		session_id;			/* Meaningful only if ethertype PPPoE */
	U16		pad1;				
	U8		input_name[16];		/* Input itf name */
	U8		output_name[16];	/* Output itf name */
	/* L3-4 optional information*/
	U32		saddr[4];
	U32		daddr[4];
	U16		sport;
	U16		dport;
	U8		proto;
	U8		pad;
	U16		mark;
	U32		timeout;
} L2BridgeL2FlowEntryCommand, *PL2BridgeL2FlowEntryCommand;

/* L2 Bridging Control command */
typedef struct _tL2BridgeControlCommand {
	U16 mode_timeout;		/* Either set bridge mode or set timeout for flow entries */
}L2BridgeControlCommand, *PL2BridgeControlCommand;

/* Function proto */
int bridge_init(void);
void bridge_exit(void);

void M_BRIDGE_process_packet(PMetadata mtd) __attribute__((section ("fast_path")));
void M_bridge_entry(void) __attribute__((section ("fast_path")));
void M_bridge_enqueue(struct tMetadata *mtd);
int M_bridge_interface_deregister( U16 index );
int M_bridge_interface_register( U8 *name, U16 index );

PL2Flow_entry M_bridge_find_l2_flow_entry(U32 hash, struct L2Flow_tmp * l2flow);
PL3Flow_entry M_bridge_find_l3_flow_entry(U32 hash, struct L3Flow * l3flow);
U32 l2bridge_get_mode(void);

#if defined (COMCERTO_2000)

/* For C2000 unaligned accesses on layer-2 fields are managed in software */
typedef struct
{
	U32 x[3];
} PKT_L2_MAC_ARRAY;

#define READ_L2_FIELD_INT(var) __READ_UNALIGNED_INT(var)

#else

/* For C1000  __packed attribut is used to handle unaligned accesses on layer-2 fields  */
typedef struct
{
	U32 x[3];
} __attribute__((packed)) PKT_L2_MAC_ARRAY;


#define READ_L2_FIELD_INT(var) (*(var))

#endif
static __inline U32 HASH_L2FLOW(struct L2Flow_tmp *l2flow)
{
	PKT_L2_MAC_ARRAY *lp;
	U32 sum;
	U32 hash;
	lp = (PKT_L2_MAC_ARRAY *)l2flow->da_sa;
	sum = ntohl(READ_L2_FIELD_INT(&lp->x[0])) + ntohl(READ_L2_FIELD_INT(&lp->x[1])) + ntohl(READ_L2_FIELD_INT(&lp->x[2])) +  ntohs(l2flow->svlan_tag) + ntohs(l2flow->cvlan_tag);
	sum += (sum >> 16) + ntohs(l2flow->ethertype) +  ntohs(l2flow->session_id) + ntohs(l2flow->input_ifindex);
	hash = (sum >> 8) ^ (sum & 0xFF);
	return hash & (NUM_BT_ENTRIES - 1);
}

static __inline U32 HASH_L3FLOW(struct L3Flow *l3flow)
{
	U32 sum;
	sum = ntohl(l3flow->saddr[0]) + ((ntohl(l3flow->daddr[0]) << 7) | (ntohl(l3flow->daddr[0]) >> 25));
	sum ^= ntohl(l3flow->saddr[IP6_LO_ADDR]) + ((ntohl(l3flow->daddr[IP6_LO_ADDR]) << 7) | (ntohl(l3flow->daddr[IP6_LO_ADDR]) >> 25));
	sum ^= ntohs(l3flow->sport) + ((ntohs(l3flow->dport) << 11) | (ntohs(l3flow->dport) >> 21));
	sum ^= (sum >> 16);
	sum ^= (sum >> 8);
	return (sum ^ l3flow->proto) & (NUM_BT_L3_ENTRIES - 1);
}

static __inline U32 HASH_L2BRIDGE(void *pmacaddr, U16 ethertype, U16 interface)
{
	PKT_L2_MAC_ARRAY *lp;
	U32 sum;
	U32 hash;

	ethertype = ntohs(ethertype);

	lp = (PKT_L2_MAC_ARRAY *)pmacaddr;
	sum = ntohl(READ_L2_FIELD_INT(&lp->x[0])) + ntohl(READ_L2_FIELD_INT(&lp->x[1])) + ntohl(READ_L2_FIELD_INT(&lp->x[2]));
	sum += (sum >> 16) + ethertype + interface;
	hash = (sum >> 8) ^ (sum & 0xFF);

	return hash & (NUM_BT_ENTRIES - 1);
}

static __inline U32 HASH_L2BRIDGE_WC(void *pmacaddr, U16 interface)
{
	U16 *lp;
	U32 sum;
	U32 hash;

	interface = ntohs(interface);

	lp = (U16 *)pmacaddr;
	sum = lp[0] + lp[1] + lp[2];
	sum += interface;
	hash = (sum >> 8) ^ (sum & 0xFF);
	return hash & (NUM_BT_ENTRIES - 1);
}

static __inline int TESTEQ_L2FLOW(struct L2Flow_tmp *l2flow_a, struct L2Flow *l2flow_b)
{
	return TESTEQ_MACADDR2(l2flow_a->da_sa, l2flow_b->da) && (l2flow_a->ethertype == l2flow_b->ethertype)
			&& (l2flow_a->session_id == l2flow_b->session_id)
			&& (l2flow_a->svlan_tag == l2flow_b->svlan_tag)
			&& (l2flow_a->cvlan_tag == l2flow_b->cvlan_tag);
}

static __inline int TESTEQ_L3FLOW(struct L3Flow *l3flow_a, struct L3Flow *l3flow_b)
{
	return !IPV6_CMP(l3flow_a->saddr, l3flow_b->saddr) && !IPV6_CMP(l3flow_a->daddr, l3flow_b->daddr)
			&& (l3flow_a->sport == l3flow_b->sport)
			&& (l3flow_a->dport == l3flow_b->dport)
			&& (l3flow_a->proto == l3flow_b->proto);
}

static __inline int TESTEQ_L2ENTRY(PL2Bridge_entry pEntry, void* daddr, U16 tid, U16 svlanid, U16 cvlanid, U16 session_id)
{
	int rc = 0;

	if (TESTEQ_MACADDR2(daddr, pEntry->da) && tid == pEntry->ethertype && svlanid == pEntry->input_svlan && cvlanid == pEntry->input_cvlan) {
		if ((tid == ETHERTYPE_PPPOE_END) && pEntry->session_id) {
			if (pEntry->session_id == session_id)
				rc = 1;
		}
		else
		 	rc =1 ;
	}
	return rc;
}

/* PBUF route entry memory chunk is used to allocated a new L2 flow entry */
#define bridge_l2_get_buffer()\
	((PVOID)(CLASS_ROUTE0_BASE_ADDR))

static inline PVOID bridge_l3_get_buffer(void)
{
	PVOID addr;
	addr = bridge_l2_get_buffer() + sizeof(L2Flow_entry);
	addr = (PVOID)ROUND_UP64(addr);
	return addr;
}

#endif /* _MODULE_BRIDGE_H_ */
