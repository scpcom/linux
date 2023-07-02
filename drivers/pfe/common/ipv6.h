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

#ifndef _IPV6_H_
#define _IPV6_H_

#include <types.h>
#include <hal.h>

typedef struct IPv6_HDR_STRUCT
{
	union
	{
	    u32 Ver_TC_FL;
	    struct {
		u16 Version_TC_FLHi;
		u16 FlowLabelLo;
	    };
	};
	u16 TotalLength;
	u8  NextHeader;
	u8  HopLimit;
	u32 SourceAddress[4];
	u32 DestinationAddress[4];
} ipv6_hdr_t;

typedef struct IPv6_FRAG_HDR
{
	u8 NextHeader;
	u8 rsvd;
	u16 FragOffset;
	u32 Identification;
} ipv6_frag_hdr_t;


typedef struct IPv6_ROUTING_HDR
{
	u8 NextHeader;
	u8 HdrExtLen;
	u8 RoutingType;
	u8 SegmentsLeft;
	u8 TypeSpecificData[0];
} ipv6_routing_hdr_t;

/* IPv6 Next Header values */
#define IPV6_HOP_BY_HOP		0
#define IPV6_IPIP			4
#define IPV6_TCP			6
#define IPV6_UDP			17
#define IPV6_ROUTING		43
#define IPV6_FRAGMENT		44
#define IPV6_GRE		47
#define IPV6_ESP			50
#define IPV6_AUTHENTICATE	51
#define IPV6_ICMP			58
#define IPV6_NONE			59
#define IPV6_DESTOPT		60
#define IPV6_ETHERIP		IPPROTOCOL_ETHERIP

/* ICMPv6 Type Values */
#define ICMPV6_ROUTER_SOLICIT   133
#define ICMPV6_ROUTER_ADVT      134
#define ICMPV6_NEIGH_SOLICIT    135
#define ICMPV6_NEIGH_ADVT       136



#define	IP6_MF				0x0001
#define IP6_FRAGOFFSET		0xFFF8

#define IPV6_MIN_MTU		1280

#define IPV6_ADDRESS_LENGTH	16

#define IPV6_HDR_SIZE		sizeof(ipv6_hdr_t)


#ifdef ENDIAN_LITTLE

#define IPV6_GET_VER_TC_FL(phdr) READ_UNALIGNED_INT((phdr)->Ver_TC_FL)

#define IPV6_SET_VER_TC_FL(phdr, ver_tc_fl) do { \
		u32 temp = ver_tc_fl | htonl(0x60000000); \
		WRITE_UNALIGNED_INT((phdr)->Ver_TC_FL, temp); \
		} while (0)

#define IPV6_GET_TRAFFIC_CLASS(phdr) ((((phdr)->Version_TC_FLHi & 0x000F) << 4) | (phdr)->Version_TC_FLHi >> 12)

#define IPV6_SET_TRAFFIC_CLASS(phdr, tc) do { \
		u16 temp = (phdr)->Version_TC_FLHi & 0x0FF0; \
		temp |= (tc) >> 4; \
		temp |= ((tc) & 0xF) << 12; \
		(phdr)->Version_TC_FLHi = temp; \
		} while (0)

#define IPV6_GET_VERSION(phdr) (((phdr)->Version_TC_FLHi >> 4) & 0xF)

#define IPV6_SET_VERSION(phdr, vers) do { \
		u16 temp = (phdr)->Version_TC_FLHi & 0xFF0F; \
		temp |= (vers) << 4; \
		(phdr)->Version_TC_FLHi = temp; \
		} while (0)

#define IPV6_GET_FLOW_LABEL_HI(phdr) (((phdr)->Version_TC_FLHi >> 8) & 0x000F)

#define IPV6_SET_FLOW_LABEL_HI(phdr, flhi) do { \
		u16 temp = (phdr)->Version_TC_FLHi & 0xF0FF; \
		temp |= (flhi) << 8; \
		(phdr)->Version_TC_FLHi = temp; \
		} while (0)

#define IPV6_SET_FLOW_LABEL(phdr, fl) do { \
		u16 flhi = ((fl) >> 16) & 0x000f; \
		IPV6_SET_FLOW_LABEL_HI((phdr), flhi); \
		(phdr)->FlowLabelLo = htons((fl) & 0xffff); \
		} while (0)

#define IPV6_COPY_FLOW_LABEL(phdr_to, phdr_from) do { \
		IPV6_SET_FLOW_LABEL_HI((phdr_to), IPV6_GET_FLOW_LABEL_HI(phdr_from)); \
		(phdr_to)->FlowLabelLo = (phdr_from)->FlowLabelLo; \
		} while (0)

#else	// !LITTLE_ENDIAN => BIG_ENDIAN

#define IPV6_GET_VER_TC_FL(phdr) READ_UNALIGNED_INT((phdr)->Ver_TC_FL)

#define IPV6_SET_VER_TC_FL(phdr, ver_tc_fl) do { \
		u32 temp = ver_tc_fl | 0x60000000; \
		WRITE_UNALIGNED_INT((phdr)->Ver_TC_FL, temp); \
		} while (0)

#define IPV6_GET_TRAFFIC_CLASS(phdr) (((phdr)->Version_TC_FLHi >> 4) & 0xFF)

#define IPV6_SET_TRAFFIC_CLASS(phdr, tc) do { \
		u16 temp = (phdr)->Version_TC_FLHi & 0xF00F; \
		temp |= (tc) << 4; \
		(phdr)->Version_TC_FLHi = temp; \
		} while (0)

#define IPV6_GET_VERSION(phdr) ((phdr)->Version_TC_FLHi >> 12)

#define IPV6_SET_VERSION(phdr, vers) do { \
		u16 temp = (phdr)->Version_TC_FLHi & 0x0FFF; \
		temp |= (vers) << 12; \
		(phdr)->Version_TC_FLHi = temp; \
		} while (0)

#define IPV6_GET_FLOW_LABEL_HI(phdr) ((phdr)->Version_TC_FLHi & 0x000F)

#define IPV6_SET_FLOW_LABEL_HI(phdr, flhi) do { \
		u16 temp = (phdr)->Version_TC_FLHi & 0xFFF0; \
		temp |= (flhi); \
		(phdr)->Version_TC_FLHi = temp; \
		} while (0)

#define IPV6_SET_FLOW_LABEL(phdr, fl) do { \
		u16 flhi = ((fl) >> 16) & 0x000f; \
		IPV6_SET_FLOW_LABEL_HI((phdr), flhi); \
		(phdr)->FlowLabelLo = htons((fl) & 0xffff); \
		} while (0)

#define IPV6_COPY_FLOW_LABEL(phdr_to, phdr_from) do { \
		IPV6_SET_FLOW_LABEL_HI((phdr_to), IPV6_GET_FLOW_LABEL_HI(phdr_from)); \
		(phdr_to)->FlowLabelLo = (phdr_from)->FlowLabelLo; \
		} while (0)

#endif

int ipv6_cmp(void *src, void *dst);

#define IPV6_CMP(addr1, addr2) ipv6_cmp(addr1, addr2)


static inline u32 is_ipv6_addr_any(u32 *addr)
{
       return ((addr[0] | addr[1] | addr[2] | addr[3]) == 0);
}


/** Compares 2 IPv6 addresses based on a mask.
 * No assumptions are made on the alignment of addresses.
 * @param a1		Pointer to first IPv6 address.
 * @param a2		Pointer to second IPv6 address.
 * @param masklen	Length of mask to use during comparison (a length of 128 means the 2 addresses must be identical).
 * @return	1 if the 2 addresses match, 0 otherwise.
 */
#if defined(COMCERTO_2000_CLASS) || defined(COMCERTO_2000_UTIL)
static inline int ipv6_cmp_mask(u32 *a1, u32 *a2, unsigned int masklen)
{
	u16 *a1s = (u16 *)a1;
	u16 *a2s = (u16 *)a2;

	while (masklen > 0) {
		if (masklen >= 16) {
			if (*a1s != *a2s)
				return 0;

			masklen -= 16;
			a1s++;
			a2s++;
		} else {
			if ((ntohs(*a1s ^ *a2s) >> (16 - masklen)) != 0)
				return 0;
			else
				return 1;
		}
	}

	return 1;
}

#else

static inline int ipv6_cmp_mask(u32 *a1, u32 *a2, unsigned int masklen)
{

	// when memcmp is present - can be rewritten to burst-compare words.
	while (masklen > 0) {

		if (masklen >= 32) {
			if (*a1 != *a2)
				return 0;

			masklen -= 32;
			a1++;
			a2++;
		} else {

			if ((ntohl(*a1 ^ *a2) >> (32 - masklen)) != 0)
				return 0;
			else
				return 1;
		}
	}

	return 1;
}
#endif

static inline int IS_ICMPv6(void *phdr, u8* rx_dmem_end)
{
	u8 proto;
	u8 *pNextHdr;
	ipv6_hdr_t *ipv6_hdr = phdr;

	proto = ipv6_hdr->NextHeader;
	pNextHdr = (u8 *) (ipv6_hdr + 1);

	// parse through any extension headers
	while (proto != IPV6_ICMP)
	{
		u32 xHdrLen;

		if ((proto != IPV6_ROUTING) && (proto != IPV6_HOP_BY_HOP) && (proto != IPV6_DESTOPT))
			return 0;

		proto = *pNextHdr;
		if(pNextHdr >= rx_dmem_end)
			return 0;
		xHdrLen = (pNextHdr[1] + 1) * 8;
		pNextHdr += xHdrLen;
	}

	return 1;
}


static inline u32 ipv6_find_fragopt(ipv6_hdr_t *ip6h, u8 **nhdr, u32 pktlen)
{
	// returns length of unfragmentable part of ipv6 packet and pointer to nexthdr
	// loosely similar to ip6_find_1stfragopt
	u16 offset = sizeof(ipv6_hdr_t);
	*nhdr = &ip6h->NextHeader;

	while (offset <= pktlen) {

		switch (**nhdr) {
		case IPV6_DESTOPT:
		case IPV6_HOP_BY_HOP:
		case IPV6_ROUTING:
			break;

		default:
			return offset;
		}

		*nhdr = (u8 *)(ip6h) + offset;
		offset += (*(*nhdr + 1) + 1) << 3;
	}

	return offset;
}

static inline u8 ipv6_find_proto(ipv6_hdr_t *ip6h, u8 **nhdr, u32 pktlen)
{
	// very similar to ipv6_find_firstfragopt above except returns protocol found
	// and does not maintain offset
	u16 offset = sizeof(ipv6_hdr_t);
	*nhdr = &ip6h->NextHeader;

	while (offset <= pktlen) {
		switch (**nhdr) {
		case IPV6_DESTOPT:
		case IPV6_HOP_BY_HOP:
		case IPV6_ROUTING:
			break;

		default:
			return **nhdr;
		}

		*nhdr = (u8 *)(ip6h) + offset;
		offset += (*(*nhdr + 1) + 1) << 3;
	}

	return **nhdr;
}

static inline u8 *ipv6_find_frag_header(ipv6_hdr_t *ip6h, u32 pktlen)
{
	// returns length of unfragmentable part of ipv6 packet and pointer to nexthdr
	// loosely similar to ip6_find_1stfragopt
	u8 *nhdr;
	u16 offset = sizeof(ipv6_hdr_t);
	nhdr = &ip6h->NextHeader;

	while (offset <= pktlen) {
		if (*nhdr == IPV6_FRAGMENT)
			return (nhdr);

		nhdr = (u8 *)(ip6h) + offset;
		offset += (*(nhdr + 1) + 1) << 3;
	}

	return nhdr;
}

static inline u32 ipv6_pseudoheader_checksum(u32 *saddr, u32 *daddr, u16 length, u8 nexthdr)
{
	u32 sum = 0;
	int i;

	for (i = 0; i < 8; i++)
		sum += ((u16 *)saddr)[i] + ((u16 *)daddr)[i];

	sum += length;
	sum += ntohs((u16)nexthdr);

	return sum;
}

#endif /* _IPV6_H_ */

