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


#ifndef _MODULE_TUNNEL_H_
#define _MODULE_TUNNEL_H_

#include "channels.h"
#include "modules.h"
#include "fpart.h"
#include "fe.h"
#include "module_ethernet.h"
#include "layer2.h"
#include "module_ipv6.h"
#include "module_ipv4.h"


#define TNL_MAX_HEADER		(40 + 14 + 4) /* Max header size matches that of a gre tunnel */
#define TNL_MAX_TUNNEL_DMEM	2
#define GRE_MAX_TUNNELS		32
#define NUM_GRE_TUNNEL_ENTRIES	64
#define GRE_TUNNEL_HASH_MASK	(NUM_GRE_TUNNEL_ENTRIES - 1)


#define TNL_STATE_FREE 				0x00
#define TNL_STATE_CREATED 			0x01
#define TNL_STATE_ENABLED			0x02
#define TNL_STATE_SA_COMPLETE			0x04
#define TNL_STATE_SAREPLY_COMPLETE		0x08
#define TNL_STATE_REMOTE_ANY			0x10

#define TNL_NOSET_PRIV				0
#define TNL_SET_PRIV				1


/* PBUF route entry memory chunk is used to allocated a new GRE tnl entry. At the time the packet reaches
 * the tunnel module, the Bridge data will no longer be used. So it is safe to use the same memory. */
#define M_tnl_get_DMEM_buffer()\
	((PVOID)(CLASS_ROUTE0_BASE_ADDR))

enum TNL_MODE {
	TNL_MODE_ETHERIPV6,
	TNL_MODE_6O4,
	TNL_MODE_4O6,
	TNL_MODE_ETHERIPV4,
	TNL_MODE_GRE_IPV6 = 4,
};

enum SAM_ID_CONV_TYPE {
	SAM_ID_CONV_NONE =0,
	SAM_ID_CONV_DUPSPORT =1,
	SAM_ID_CONV_PSID =2,
};

#define TNL_ETHERIP_VERSION 0x3000
#define TNL_ETHERIP_HDR_LEN 2

#define TNL_GRE_PROTOCOL	0x6558		// Transparent Ethernet Bridging
#define TNL_GRE_HDRSIZE		4
#define TNL_GRE_VERSION		0x0
#define TNL_GRE_FLAGS		(0x0000 | TNL_GRE_VERSION)	// no flags are supported
#define TNL_GRE_HEADER		((TNL_GRE_FLAGS << 16) | TNL_GRE_PROTOCOL)

//#define TNL_DBG
#undef TNL_DBG

#ifdef TNL_DBG
/* debug entries */
enum TNL_DBG_SLOT {
	TNL_DBG_START = 0,
	TNL_DBG_NUM_ENTRIES,
	TNL_DBG_COMPLETE,
	TNL_DBG_SA_IN_NOMATCH,
	TNL_DBG_SANUM_IN_NOMATCH,
	TNL_DBG_TNL_IN_NOMATCH,
	TNL_DBG_IN_RXBRIDGE,
	TNL_DBG_IN_EXCEPT,
	TNL_DBG_OUT_EXCEPT_1,
	TNL_DBG_OUT_EXCEPT_2,
	TNL_DBG_OUT_EXCEPT_3,
	TNL_DBG_L2BRIDGE_NO_MATCH,
	TNL_DBG_END,
};

/* debug flags */
#define TNL_DBG_SA_COMPLETE	0x2
#define TNL_DBG_SAREPLY_COMPLETE	0x4
#endif


/***********************************
* Tunnel API Command and Entry strutures
*
************************************/

typedef struct _tTNLCommand_create {
	U8	name[16];
	U32 	local[4];
	U32	remote[4];
	U8	output_device[16];
	U8	mode;
	/* options */
	U8 	secure;
	U8	elim;
	U8	hlim;
	U32	fl;
	U16	frag_off;
	U16	enabled;
	U32	route_id;
	U16	mtu;
	U16	pad;
}TNLCommand_create , *PTNLCommand_create;

typedef struct _tTNLCommand_delete {
	U8	name[16];
}TNLCommand_delete, *PTNLCommand_delete;


typedef struct _tTNLCommand_ipsec {
	U8	name[16];
	U16 	SA_nr;
	U16	SAReply_nr;
	U16	SA_handle[4];
	U16	SAReply_handle[4];
} TNLCommand_ipsec, *PTNLCommand_ipsec;

typedef struct _tTNLCommand_query{
        U16     result;
        U16     unused;
        U8      name[16];
        U32     local[4];
        U32     remote[4];
        U8      mode;
        U8      secure;
	U8	elim;
	U8	hlim;
        U32     fl;
        U16     frag_off;
        U16     enabled;
	U32	route_id;
	U16	mtu;
	U16	pad;
}TNLCommand_query , *PTNLCommand_query;

typedef struct {
        int  port_set_id;          /* Port Set ID               */
        int  port_set_id_length;   /* Port Set ID length        */
        int  psid_offset;          /* PSID offset               */
}sam_port_info_t;

typedef struct _tTNLCommand_IdConvDP {
        U16      IdConvStatus;
	U16	 Pad;
}TNLCommand_IdConvDP, *pTNLCommand_IdConvDP;

typedef struct _tTNLCommand_IdConvPsid {
        U8       name[16];
	sam_port_info_t sam_port_info;
        U32      IdConvStatus:1,
		 unused:31;
}TNLCommand_IdConvPsid, *pTNLCommand_IdConvPsid;



#if !defined(COMCERTO_2000)

typedef struct _tTnlEntry{
	struct itf itf;

	union {
	  U8	header[TNL_MAX_HEADER];
	  ipv4_hdr_t header_v4;
	};
	U8	header_size;
	U8	mode;
	U8	proto;
	U8	secure;
	U8	state;
	U32 	local[4];
	U32	remote[4];
	U8	hlim;
	U8	elim;
	U8	output_proto;
	U32 fl;
	U16 frag_off;
	PRouteEntry pRtEntry;
	U16 SAReply_nr;
	U16 SA_nr;
	U16 hSAEntry_in[SA_MAX_OP];
	U16 hSAEntry_out[SA_MAX_OP];
	U32 route_id;
	int tunnel_index;
	U16 sam_abit;
	U16 sam_abit_max;
	U16 sam_kbit;
	U16 sam_mbit;
	U16 sam_mbit_max;
	U8 sam_mbit_len;
	U8 sam_abit_len;
	U8 sam_kbit_len;
	U8 sam_id_conv_enable;
	U16 tnl_mtu;
	U16 pad;
}TnlEntry, *PTnlEntry;

#elif defined(COMCERTO_2000_CONTROL)

extern struct slist_head gre_tunnel_cache[];

typedef struct _tHw_TnlEntry_gre{
	struct itf itf;
	struct hw_route route;

	union {
	  U8	header[TNL_MAX_HEADER];
	  ipv4_hdr_t header_v4;
	};
	U8	header_size;
	U8	mode;
	U8	proto;
	U8	secure;
	U8	state;
	U8	hlim;
	U8	elim;
	U8	output_proto;
	U32 	local[4];
	U32	remote[4];
	U32 fl;
	U16 frag_off;
	U16 SAReply_nr;
	U16 SA_nr;
	U16 hSAEntry_in[SA_MAX_OP];
	U16 hSAEntry_out[SA_MAX_OP];
	U16 sam_abit;
	U16 sam_abit_max;
	U16 sam_kbit;
	U16 sam_mbit;
	U16 sam_mbit_max;
	U8 sam_mbit_len;
	U8 sam_abit_len;
	U8 sam_kbit_len;
	U8 sam_id_conv_enable;
	U16 tnl_mtu;
	U16 pad;

	// The above fields must exactly match the fields in the TnlEntry structure, up to
	//	the "first_unused_field".

	U32 next;
	U32 dma_addr;
#ifdef CFG_STATS
	U32 total_packets_received[NUM_PE_CLASS];
	U32 total_packets_transmitted[NUM_PE_CLASS];
	U64 total_bytes_received[NUM_PE_CLASS];
	U64 total_bytes_transmitted[NUM_PE_CLASS];
#endif

	// following entries not used in data path
	U32 route_id;
	PRouteEntry pRtEntry;
	struct dlist_head  list;
	unsigned long removal_time;
}Hw_TnlEntry_gre, *PHw_TnlEntry_gre;

/* Structure used by tunnel entries in sw corresponding to HW tunnel entries in DMEM.
 *
 * The structure should be the exact same size as that of the HW entry in DMEM, since
 * an array of structures are maintained across DDR and DMEM and the addresses mapped. */

typedef struct _tTnlEntry{
	struct itf itf;
	struct hw_route route;

	union {
	  U8	header[TNL_MAX_HEADER];
	  ipv4_hdr_t header_v4;
	};
	U8	header_size;
	U8	mode;
	U8	proto;
	U8	secure;
	U8	state;
	U8	hlim;
	U8	elim;
	U8	output_proto;
	U32 	local[4];
	U32	remote[4];
	U32 fl;
	U16 frag_off;
	U16 SAReply_nr;
	U16 SA_nr;
	U16 hSAEntry_in[SA_MAX_OP];
	U16 hSAEntry_out[SA_MAX_OP];
	U16 sam_abit;
	U16 sam_abit_max;
	U16 sam_kbit;
	U16 sam_mbit;
	U16 sam_mbit_max;
	U8 sam_mbit_len;
	U8 sam_abit_len;
	U8 sam_kbit_len;
	U8 sam_id_conv_enable;
	U16 tnl_mtu;
	U16 pad;

	// following entries not used in data path
#define first_unused_field route_id
	U32 route_id;
	PRouteEntry pRtEntry;
	union
	{
		int tunnel_index;
		PHw_TnlEntry_gre hw_tnl_entry;
	};
	struct slist_entry  list; /* This slist is used only for the GRE tunnels*/
}TnlEntry, *PTnlEntry;

typedef struct tTNL_context {
	TnlEntry	tunnel_table[TNL_MAX_TUNNEL_DMEM];
}TNL_context;

#else	// defined(COMCERTO_2000)

/* Structure used by tunnel entries in DMEM */
typedef struct _tTnlEntry{
	struct itf itf;
	RouteEntry route;

	union {
	  U8	header[TNL_MAX_HEADER];
	  ipv4_hdr_t header_v4;
	};
	U8	header_size;
	U8	mode;
	U8	proto;
	U8	secure;
	U8	state;
	U8	hlim;
	U8	elim;
	U8	output_proto;
	U32 	local[4];
	U32	remote[4];
	U32 fl;
	U16 frag_off;
	U16 SAReply_nr;
	U16 SA_nr;
	U16 hSAEntry_in[SA_MAX_OP];
	U16 hSAEntry_out[SA_MAX_OP];
	U16 sam_abit;
	U16 sam_abit_max;
	U16 sam_kbit;
	U16 sam_mbit;
	U16 sam_mbit_max;
	U8 sam_mbit_len;
	U8 sam_abit_len;
	U8 sam_kbit_len;
	U8 sam_id_conv_enable;
	U16 tnl_mtu;
	U16 pad;

	// following entries not used in data path
	U32 unused_route_id;
	U32 unused_pRtEntry;
	int unused_tunnel_index;
	int unused_list;
}TnlEntry, *PTnlEntry;


/* Structure used by GRE Tunnel entries*/
typedef struct _tTnlEntry_gre{
	struct itf itf;
	RouteEntry route;

	union {
	  U8	header[TNL_MAX_HEADER];
	  ipv4_hdr_t header_v4;
	};
	U8	header_size;
	U8	mode;
	U8	proto;
	U8	secure;
	U8	state;
	U8	hlim;
	U8	elim;
	U8	output_proto;
	U32 	local[4];
	U32	remote[4];
	U32 fl;
	U16 frag_off;
	U16 SAReply_nr;
	U16 SA_nr;
	U16 hSAEntry_in[SA_MAX_OP];
	U16 hSAEntry_out[SA_MAX_OP];
	U16 sam_abit;
	U16 sam_abit_max;
	U16 sam_kbit;
	U16 sam_mbit;
	U16 sam_mbit_max;
	U8 sam_mbit_len;
	U8 sam_abit_len;
	U8 sam_kbit_len;
	U8 sam_id_conv_enable;
	U16 tnl_mtu;
	U16 pad;

	U32 next;
	U32 dma_addr;
#ifdef CFG_STATS
	U32 total_packets_received[NUM_PE_CLASS];
	U32 total_packets_transmitted[NUM_PE_CLASS];
	U64 total_bytes_received[NUM_PE_CLASS];
	U64 total_bytes_transmitted[NUM_PE_CLASS];
#endif
}TnlEntry_gre, *PTnlEntry_gre;

typedef struct tTNL_context {
	TnlEntry	tunnel_table[TNL_MAX_TUNNEL_DMEM];
}TNL_context;

#endif	/* !defined(COMCERTO_2000) */

extern struct tTNL_context gTNLCtx;

void M_TNL_OUT_process_packet(PMetadata mtd);
void M_TNL_IN_process_packet(PMetadata mtd);

#if !defined(COMCERTO_2000)
BOOL M_tnl_in_init(PModuleDesc pModule);
BOOL M_tnl_out_init(PModuleDesc pModule);
void M_tnl_entry_in(void);
void M_tnl_entry_out(void);
#else
int tunnel_init(void);
void tunnel_exit(void);
#endif

int M_tnl_update_header(PTnlEntry pTunnelEntry, PMetadata mtd);
U16 Tnl_Get_Next_Hash_Entry(PTNLCommand_query pTnlCmd, int reset_action);
#ifdef TNL_DBG
void M_tnl_debug(void);
#endif

PTnlEntry M_tnl6_match_ingress(PMetadata mtd, ipv6_hdr_t *ipv6_hdr, U8 proto, U8 set_tnl);
PTnlEntry M_tnl4_match_ingress(PMetadata mtd, ipv4_hdr_t *ipv4_hdr, U8 proto);


static __inline void * M_tnl_add_header(PMetadata mtd,  PTnlEntry pTnlEntry)
{
	mtd->offset -= pTnlEntry->header_size;
	mtd->length += pTnlEntry->header_size;
	SFL_memcpy(mtd->data + mtd->offset, pTnlEntry->header, pTnlEntry->header_size);
	M_tnl_update_header(pTnlEntry, mtd);
	return (mtd->data + mtd->offset);
}


/* Inherited from linux-2.6.21.1/include/net/inet_ecn.h for tos management in 6o4 tunnels */
enum {
	INET_ECN_NOT_ECT = 0,
	INET_ECN_ECT_1 = 1,
	INET_ECN_ECT_0 = 2,
	INET_ECN_CE = 3,
	INET_ECN_MASK = 3,
};

static inline int INET_ECN_is_ce(U8 dsfield)
{
	return (dsfield & INET_ECN_MASK) == INET_ECN_CE;
}

static inline U8 INET_ECN_encapsulate(U8 outer, U8 inner)
{
	outer &= ~INET_ECN_MASK;
	outer |= !INET_ECN_is_ce(inner) ? (inner & INET_ECN_MASK) : INET_ECN_ECT_0;
	return outer;
}

static inline void ipv6_change_dsfield(ipv6_hdr_t *ipv6h,U8 mask, U8 value)
{
        U16 tmp;

        tmp = ntohs(*(U16*)ipv6h);
        tmp = (tmp & ((mask << 4) | 0xf00f)) | (value << 4);
        *(U16*) ipv6h = htons(tmp);
}

static __inline U32 HASH_GRE_TNL(U32 *addr)
{
	U16 *addr16 = (U16 *)addr;
	U32 sum;
	// since everything eventually gets folded into low byte, no need to do ntohs()
	sum = addr16[0] ^ addr16[1];
	sum ^= addr16[2] ^ addr16[3];
	sum ^= addr16[4] ^ addr16[5];
	sum ^= addr16[6] ^ addr16[7];
	sum ^= sum >> 8;
	sum &= 0xFF;
	sum ^= sum >> 4;
	return (sum & GRE_TUNNEL_HASH_MASK);
}

#endif /* _MODULE_TUNNEL_H_ */
