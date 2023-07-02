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


#ifndef _MODULE_IPV6_H_
#define _MODULE_IPV6_H_

#include "types.h"
#include "modules.h"
#include "fpart.h"
#include "fe.h"
#include "channels.h"
#ifndef COMCERTO_2000_UTIL
#include "layer2.h"
#endif
#include "module_ipv4.h"
#include "common_hdrs.h"

typedef struct tIPV6_context {
	U32 fragmentation_id;
} IPV6_context;

/* IPv6 Conntrack entry */
#if !defined(COMCERTO_2000)
typedef struct _tCtEntryIPv6{
	// start of common header -- must match IPv4
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
	U16	Sport;
	U16	Dport;
	U16 hSAEntry[SA_MAX_OP];
	PRouteEntry tnl_route;
	U32	unused1;
// 1st DC line
	U32 Saddr_v6[4];
	U32 Daddr_v6[4];
	U8 rtpqos_slot; // !! we are over the second DC cache line
}CtEntryIPv6, *PCtEntryIPv6;

static inline PRouteEntry ct6_tnl_route(PCtEntryIPv6 pCtEntry)
{
	return pCtEntry->tnl_route;
}

#else

typedef CtEntry CtEntryIPv6, *PCtEntryIPv6;

#if !defined(COMCERTO_2000_CONTROL)
static inline PRouteEntry ct6_tnl_route(PCtEntryIPv6 pCtEntry)
{
	return &(pCtEntry->tnl6o4_route);
}
#endif
#endif

#define ct6_route(entry)	ct_route((PCtEntry)(entry))


#if defined(COMCERTO_2000_CONTROL) || !defined(COMCERTO_2000)
int IPv6_delete_CTpair(PCtEntryIPv6 pCtEntry);
#else
void M_IPV6_reassembly_process_from_util(PMetadata mtd, lmem_trailer_t *trailer);
#endif

int ipv6_init(void);
void ipv6_exit(void);

#if !defined(COMCERTO_2000)
void M_ipv6_entry(void) __attribute__((section ("fast_path")));
PCtEntryIPv6 CT6_lookup(PMetadata mtd, ipv6_hdr_t *ipv6_hdr, U16 SPort,U16 DPort , U8 Proto) __attribute__((section("fast_path"))) ;
#endif

PCtEntryIPv6 IPv6_get_ctentry(U32 *saddr, U32 *daddr, U16 sport, U16 dport, U16 proto)  __attribute__ ((noinline));


void M_IPV6_process_packet(PMetadata mtd) __attribute__((section ("fast_path")));
PMetadata IPv6_fragment_packet(PMetadata mtd, ipv6_hdr_t *pIp6h, U8 *phdr, U32 mtu, U32 hlen, U32 preL2_len, U32 if_type);
int IPv6_xmit_on_socket(PMetadata mtd, PSock6Entry pSocket, BOOL ipv6_update, U32 payload_diff);
void IPv6_tcp_termination(PCtEntryIPv6 pCtEntry);

int IPv6_handle_RESET(void);


#if defined(COMCERTO_2000)
static __inline U32 HASH_CT6(U32 *Saddr, U32 *Daddr, U32 Sport, U32 Dport, U16 Proto)
{
	int i;
	U32 sum;

	sum = 0;
	for (i = 0; i < 4; i++)
		sum += ntohl(READ_UNALIGNED_INT(Saddr[i]));
	sum = htonl(sum) ^ htonl(ntohs(Sport));
	sum = crc32_be((u8 *)&sum);

	for (i = 0; i < 4; i++)
		sum += ntohl(READ_UNALIGNED_INT(Daddr[i]));
	sum += Proto;
	sum += ntohs(Dport);
//	sum += phy_no;

	return sum & CT_TABLE_HASH_MASK;
}
#else
static __inline U32 HASH_CT6(U32 *Saddr, U32 *Daddr, U32 Sport, U32 Dport, U16 Proto)
{
	U32 sum;
	sum = Saddr[IP6_LO_ADDR] + ((Daddr[IP6_LO_ADDR] << 7) | (Daddr[IP6_LO_ADDR] >> 25));
	sum ^= Sport + ((Dport << 11) | (Dport >> 21));
	sum ^= (sum >> 16);
	sum ^= (sum >> 8);
	return (sum ^ Proto) & CT_TABLE_HASH_MASK;
}
#endif


#endif /* _MODULE_IPV6_H_ */
