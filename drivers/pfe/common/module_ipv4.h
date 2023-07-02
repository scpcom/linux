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


#ifndef _MODULE_IPV4_H_
#define _MODULE_IPV4_H_

#include "channels.h"
#include "modules.h"
#include "fe.h"
#include "common_hdrs.h"
#include "layer2.h"
#include "module_socket.h"
#include "module_ipsec.h"


#define UDP_REPLY_TIMER_INIT 0xFFFFFFFF

#define MAX_L2_HEADER	18

#define GET_L2HDR(mtd)  (mtd->data + mtd->offset)

//Conntrack entry
#if !defined(COMCERTO_2000)
typedef struct _tCtEntry{
	// start of common header -- must match IPv6
	struct slist_entry list;
	U32 last_ct_timer;
	union {
	    U16 fwmark;
	    struct {
		U16 queue : 5;
		U16 vlan_pbits : 3;
		U16 dscp_mark_flag : 1;
		U16 dscp_mark_value : 6;
		U16 set_vlan_pbits : 1;
	    } __attribute__((packed));
	};
	U16 status;
	union {
		PRouteEntry pRtEntry;
		U32 route_id;
	};
	// end of common header
	U32 Saddr_v4;
	U32 Daddr_v4;
	U16 Sport;
	U16 Dport;
	U16 ip_chksm_corr;
	U16 tcp_udp_chksm_corr;
// DC line boundary
	U32 twin_Saddr;
	U32 twin_Daddr;
	U16 twin_Sport;
	U16 twin_Dport;
	U16 unused1;
	U16 unused2;
	//U32 unused2;
	PRouteEntry tnl_route;
	U16 hSAEntry[SA_MAX_OP];
	U8 rtpqos_slot;
}CtEntry, *PCtEntry;

static inline PRouteEntry ct_tnl_route(PCtEntry pCtEntry)
{
	return pCtEntry->tnl_route;
}

static inline PRouteEntry ct_route(PCtEntry pCtEntry)
{
	return pCtEntry->pRtEntry;
}

static inline U8 GET_PROTOCOL(PCtEntry pCtEntry)
{
	return ct_proto[pCtEntry->status & CONNTRACK_PROTO_MASK]; 
}

static inline void SET_PROTOCOL(PCtEntry pCtEntry_orig,PCtEntry pCtEntry_rep, U8 Proto)
{
	switch(Proto)
	{
	case IPPROTOCOL_UDP:
		pCtEntry_orig->status |= CONNTRACK_UDP;
		pCtEntry_rep->status |= CONNTRACK_UDP;
	break;
	case IPPROTOCOL_IPIP:
		pCtEntry_orig->status |= CONNTRACK_IPIP;
		pCtEntry_rep->status |= CONNTRACK_IPIP;
	break;
	case IPPROTOCOL_TCP:
	default:
	break;
}

#elif defined(COMCERTO_2000_CONTROL)

struct hw_ct {
	U32	next;
	U32	flags;

	/******* !!!!!!!!!!!!!!!!!! *********/
	/* Be careful about adding/moving fields in this structure -- hw_ct_delayed_remove() does a
	partial structure copy that depends on the exact layout of the structure */
	/******* !!!!!!!!!!!!!!!!!! *********/

	U16	Sport;
	U16	Dport;
	U8	proto;
	U8	rtpqos_slot;
	U16	hash;

	union {
		struct {
			U32 Saddr_v4;
			U32 Daddr_v4;
			U32 twin_Saddr;
			U32 twin_Daddr;
			U16 twin_Sport;
			U16 twin_Dport;
			struct hw_route_4o6 tnl4o6_route;
		};

		struct {
			U32 Saddr_v6[4];
			U32 Daddr_v6[4];
			struct hw_route tnl6o4_route;
		};

	};

	/* End of fields used by hardware */
	struct hw_route route;

	union {
		U16 fwmark;

		struct {
			U16 set_vlan_pbits : 1;
			U16 dscp_mark_value : 6;
			U16 dscp_mark_flag : 1;
			U16 vlan_pbits : 3;
			U16 queue : 5;
		};
	};

	U16 status;

	U32 active;

	U16 ip_chksm_corr;
	U16 tcp_udp_chksm_corr;

	U16 hSAEntry[SA_MAX_OP];

	U32 twin_dma_addr;

	/******* !!!!!!!!!!!!!!!!!! *********/
	/* Be careful about adding/moving fields in this structure -- hw_ct_delayed_remove() does a
	partial structure copy that depends on the exact layout of the structure */
	/******* !!!!!!!!!!!!!!!!!! *********/

	U32 dma_addr;			/** Physical address of the hardware conntrack, used for keepalive writeback and delayed removal*/

	/* The bellow fields are only used by host software, so keep them at the end of the structure */
	struct dlist_head list;		/** if the entry is in the hash array, this is the list head for the hash bucket,
					 otherwise it's a list entry in the hash bucket list */
	struct dlist_head rlist;
	struct _tCtEntry *pCtEntry;	/** pointer to the software conntrack */
	unsigned long removal_time;
};


typedef struct _tCtEntry {
	struct slist_entry list;
	U16	Sport;
	U16	Dport;
	U8	proto;
	U8	inPhyPortNum;
	U16	hash;

	union {
		struct {
			U32 Saddr_v4;
			U32 Daddr_v4;
			U32 unused1;
			U32 unused2;
			U32 twin_Saddr;
			U32 twin_Daddr;
			U16 twin_Sport;
			U16 twin_Dport;
			U32 unused3;
		};

		struct {
			U32 Saddr_v6[4];
			U32 Daddr_v6[4];
		};
	};

	/* End of fields used by hardware */

	U32 route_id;
	PRouteEntry pRtEntry;

	union {
		U16 fwmark;

		struct {
			U16 set_vlan_pbits : 1;
			U16 dscp_mark_value : 6;
			U16 dscp_mark_flag : 1;
			U16 vlan_pbits : 3;
			U16 queue : 5;
		};
	};

	U16 status;

	U32 last_ct_timer;

	U16 ip_chksm_corr;
	U16 tcp_udp_chksm_corr;

	PRouteEntry tnl_route;
	U16 hSAEntry[SA_MAX_OP];
	
	U8	rtpqos_slot;

	struct _tCtEntry *twin;

	struct hw_ct *ct;	/** pointer to the hardware conntrack */
}CtEntry, *PCtEntry;

static inline U8 GET_PROTOCOL(PCtEntry pCtEntry)
{
	return pCtEntry->proto; 
}

static inline void SET_PROTOCOL(PCtEntry pCtEntry_orig,PCtEntry pCtEntry_rep, U8 Proto)
{
	pCtEntry_orig->proto = Proto;
	pCtEntry_rep->proto  = Proto;
}

#define IS_HASH_ARRAY(ctrl, ct_addr)	(((unsigned long)(ct_addr) >= (unsigned long)(ctrl->hash_array_baseaddr)) \
	&& ((unsigned long)(ct_addr) < ((unsigned long)(ctrl->hash_array_baseaddr) + (NUM_CT_ENTRIES * CLASS_ROUTE_SIZE))))
#else	// defined(COMCERTO_2000)

void M_IPV4_rtp_process_from_util(PMetadata mtd, lmem_trailer_t *trailer);
void M_IPV4_process_from_util(PMetadata mtd, lmem_trailer_t *trailer);


typedef struct _tCtEntry{
	struct slist_entry list;
	U32	flags;
	U16	Sport;
	U16	Dport;
	U8	proto;
	U8	rtpqos_slot;
	U16	hash;

	union {

	struct {
			U32 Saddr_v4;
			U32 Daddr_v4;
			U32 twin_Saddr;
			U32 twin_Daddr;
			U16 twin_Sport;
			U16 twin_Dport;
			RouteEntry_4o6 tnl4o6_route;
		};

		struct {
			U32 Saddr_v6[4];
			U32 Daddr_v6[4];
			RouteEntry tnl6o4_route;
		};

	};

	/* End of fields used by hardware */
	RouteEntry route;

	union {
		U16 fwmark;

		struct {
			U16 set_vlan_pbits : 1;
			U16 dscp_mark_value : 6;
			U16 dscp_mark_flag : 1;
			U16 vlan_pbits : 3;
			U16 queue : 5;
		};
	};

	U16 status;
	U32 active;
	U16 ip_chksm_corr;
	U16 tcp_udp_chksm_corr;
	U16 hSAEntry[SA_MAX_OP];

	U32 twin_dma_addr;
	U32 dma_addr;			/** Physical address of the hardware conntrack, used for keepalive writeback and delayed removal*/
}CtEntry, *PCtEntry;


#if !defined(COMCERTO_2000_CONTROL)
static inline PRouteEntry_4o6 ct_tnl_route(PCtEntry pCtEntry)
{
	return &pCtEntry->tnl4o6_route;
}
#endif

static inline PRouteEntry ct_route(PCtEntry pCtEntry)
{
	return &pCtEntry->route;
}

#if defined(COMCERTO_2000_CLASS)
static inline PRouteEntry sa_route(PSAEntry pSAEntry)
{
        return &pSAEntry->route;
}
#endif


#if defined(CFG_STANDALONE)
static inline void temp_route_setup(PRouteEntry pRtEntry, PCtEntry pCtEntry, PMetadata mtd)
{
//#define TEST_PPPOE
#if !defined(TEST_PPPOE)
	pRtEntry->itf = (mtd->input_port == 0 ? &phy_port[1].itf : &phy_port[0].itf);
	
	if (pRtEntry->mtu == 0)
		pRtEntry->mtu = 1500;

#else /* to test PPPoE */
	pRtEntry->itf = (mtd->input_port == 0 ? &phy_port[1].itf : &pppoe_itf[0].itf);

	if (pRtEntry->mtu == 0)
		pRtEntry->mtu = 1492;
#endif
}
#endif

#endif	/* !defined(COMCERTO_2000) */

#if defined(COMCERTO_2000_CONTROL)
#define CT_TWIN(pentry)		(((PCtEntry)(pentry))->twin)
#define CT6_TWIN(pentry)	CT_TWIN(pentry)
#define CT_ORIG(pentry)		((((PCtEntry)(pentry))->status & CONNTRACK_ORIG) ? (PCtEntry)(pentry) : ((PCtEntry)(pentry))->twin)
#define CT6_ORIG(pentry)	CT_ORIG(pentry)
#define CT_REPLY_BIT(pentry)	(!(((PCtEntry)(pentry))->status & CONNTRACK_ORIG))
#elif defined(COMCERTO_2000)
#define CT_TWIN(pentry)		(((PCtEntry)(pentry))->twin_dma_addr)
#define CT6_TWIN(pentry)	CT_TWIN(pentry)
#else
#define CT_TWIN(pentry)		(PCtEntry)((U32)(pentry) ^ CTENTRY_UNIDIR_SIZE)
#define CT6_TWIN(pentry)	(PCtEntryIPv6)((U32)(pentry) ^ CTENTRY_UNIDIR_SIZE)
#define CT_ORIG(pentry)		(PCtEntry)((U32)(pentry) & ~CTENTRY_UNIDIR_SIZE)
#define CT6_ORIG(pentry)	(PCtEntryIPv6)((U32)(pentry) & ~CTENTRY_UNIDIR_SIZE)
#define CT_REPLY_BIT(pentry)	((U32)(pentry) & CTENTRY_UNIDIR_SIZE)
#endif



extern struct slist_head rt_cache[];

PCtEntry ct_alloc(void);
void ct_free(PCtEntry pEntry_orig);
int ct_add(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep);
void ct_update(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep);
void ct_remove(PCtEntry pEntry_orig, PCtEntry pEntry_rep, U32 hash_orig, U32 hash_rep);

int ipv4_init(void);
void ipv4_exit(void);

#if defined(COMCERTO_2000_CONTROL) || !defined(COMCERTO_2000)
U32 Get_Ctentry_Hash(PVOID pblock) __attribute__ ((noinline));
int IPv4_delete_CTpair(PCtEntry ctEntry) __attribute__ ((noinline));
void IP_deleteCt_from_onif_index(U32 if_index) __attribute__ ((noinline));
void IP_MarkSwap(PCtEntry pCtEntry, PCtEntry pCtTwin) __attribute__ ((noinline));
PRouteEntry IP_Check_Route(PCtEntry pCtEntry);
void IP_delete_CT_route(PCtEntry pCtEntry) __attribute__ ((noinline));
void IP_expire(PCtEntry pCtEntry);
U32 IP_get_fwmark(PCtEntry pOrigEntry, PCtEntry pReplEntry);
#endif

#if !defined(COMCERTO_2000)
void M_ipv4_entry(void) __attribute__((section ("fast_path")));
#endif

PCtEntry IPv4_get_ctentry(U32 saddr, U32 daddr, U16 sport, U16 dport, U16 proto) __attribute__ ((noinline));

void M_IPV4_process_packet(PMetadata mtd) __attribute__((section ("fast_path")));

void M_RTP_RELAY_process_packet(PMetadata mtd);


PMetadata IPv4_fragment_packet(PMetadata mtd, ipv4_hdr_t *ipv4_hdr, U32 mtu, U32 preL2_len, U32 if_type);
#if defined(COMCERTO_2000)
PMetadata IPv4_fragment_packet_ipsec(PMetadata mtd, ipv4_hdr_t *ipv4_hdr, U32 mtu, U32 preL2_len, U32 if_type);
#endif


int IPv4_xmit_on_socket(PMetadata mtd, PSockEntry pSocket, BOOL ipv4_update, U32 payload_diff, U8* inner_ipv4hdr);


#if defined(COMCERTO_2000)

#define CT_VALID	(1 << 0)
#define CT_USED		(1 << 1)
#define CT_UPDATING	(1 << 2)

extern U32 class_route_table_base;
extern U32 class_route_table_hash_mask;

#if defined(COMCERTO_2000_CONTROL)
extern struct slist_head ct_cache[];
#else
extern PCtEntry ct_cache[];

static inline void ct_keepalive(PCtEntry pCtEntry)
{
	/* If host has marked connection inactive, update ddr structure */
	if (!pCtEntry->active)
	{
		PCtEntry ddr_ct = (PCtEntry)pCtEntry->dma_addr;

		ddr_ct->active = 1;
	}
}
#endif

#define CRCPOLY_BE 0x04c11db7
static inline U32 crc32_be(U8 *data)
{
	int i, j;
	U32 crc = 0xffffffff;

	for (i = 0; i < 4; i++) {
		crc ^= *data++ << 24;

		for (j = 0; j < 8; j++)
			crc = (crc << 1) ^ ((crc & 0x80000000) ? CRCPOLY_BE : 0);
	}

	return crc;
}

static __inline U32 HASH_CT(U32 Saddr, U32 Daddr, U32 Sport, U32 Dport, U16 Proto)
{
	U32 sum;

	sum = Saddr ^ htonl(ntohs(Sport));
	sum = crc32_be((u8 *)&sum);

	sum += ntohl(Daddr);
	sum += Proto;
	sum += ntohs(Dport);
//	sum += phy_no;

	return sum & CT_TABLE_HASH_MASK;
}

PCtEntry M_ipv4_frag_ConntrackMatch(PMetadata mtd, ipv4_hdr_t * ipv4_hdr);

#else

extern struct slist_head ct_cache[];

static __inline U32 HASH_CT(U32 Saddr, U32 Daddr, U32 Sport, U32 Dport, U16 Proto)
{
	U32 sum;
	sum = Saddr + ((Daddr << 7) | (Daddr >> 25));
	sum ^= Sport + ((Dport << 11) | (Dport >> 21));
	sum ^= (sum >> 16);
	sum ^= (sum >> 8);
	return (sum ^ Proto) & CT_TABLE_HASH_MASK;
}

// Fast CTData

extern U8 Fast_CTData[];
extern U8 *pFast_CTData_freelist;

#define Is_FastCT_block(pblock) (((U32)(pblock) & 0xFF000000) == 0x0a000000)

void Fast_CTData_CopyBlock(PVOID newblock, PVOID origblock) __attribute__ ((noinline));

static INLINE U8 *Fast_CTData_Alloc(void)
{
	U8 *pblock;
	if (pFast_CTData_freelist != NULL)
	{
		pblock = pFast_CTData_freelist;
		pFast_CTData_freelist = *(U8 **)pblock;
	}
	else
	{
		pblock = Heap_Alloc(CTENTRY_BIDIR_SIZE);
	}
	return pblock;
}


static INLINE void CTData_Free(PVOID pblock)
{
	if (Is_FastCT_block(pblock))
	{
		*(U8 **)pblock = pFast_CTData_freelist;
		pFast_CTData_freelist = (U8 *)pblock;
	}
	else
		Heap_Free(pblock);
}

static inline void ct_keepalive(PCtEntry pCtEntry)
{
        // we update the timeout since we saw a new packet
        pCtEntry->last_ct_timer = ct_timer;
}


#endif /* COMCERTO_2000 */

#endif /* _MODULE_IPV4_H_ */
