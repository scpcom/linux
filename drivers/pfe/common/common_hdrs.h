/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
*/

#ifndef _COMMON_HDRS_H__
#define _COMMON_HDRS_H__

#include "types.h"
#include "ipv6.h"

#define C2000_MAX_FRAGMENTS 3

#define IPV4_HDR_SIZE		20
#define TCP_HDR_SIZE		20
#define UDP_TCP_HDR_SIZE	8
#define PPPOE_HDR_SIZE		8
#define IPSEC_ESP_HDR_SIZE	8
#define ETH_HDR_SIZE		14
#define VLAN_HDR_SIZE		4

#define MAX_L2_HEADER	18


/* 
* VLAN
*
*/


/** Structure that describes the VLAN header as it used by the code:
 * VID contains the actual VID value, while TPID contains the Ethertype of the inner data.
 * This makes the VLAN handling logic simpler and more intuitive.
 */
typedef struct VLAN_HDR_STRUCT
{
		U16 TCI;
		U16 TPID; /**< Tells the Protocol ID, like IPv4 or IPv6 */
}vlan_hdr_t;

#define VLAN_VID(vlan_hdr)	((vlan_hdr)->TCI & htons(0xfff))
#define VLAN_PCP(vlan_hdr)	(*(U8 *)&((vlan_hdr)->TCI) >> 5)

/** Structure that describes the real VLAN header format, as defined in the 802.1Q standard.
 */
typedef struct REAL_VLAN_HDR_STRUCT
{
	union {
		U32 tag;
		struct {
			U16 TPID; /**< should be 0x8100 in most VLAN tags */
			U16 TCI;
		};
	};
}real_vlan_hdr_t;


/* 
* IPV4
*
*/


/* IP flags. */
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#define IP_MF		0x2000		/* Flag: "More Fragments"	*/
#define IP_OFFSET	0x1FFF		/* "Fragment Offset" part	*/

typedef struct  IPv4_HDR_STRUCT
{
        unsigned char Version_IHL;
        unsigned char TypeOfService;
        unsigned short TotalLength;
        unsigned short Identification;
        unsigned short Flags_FragmentOffset;
        unsigned char  TTL;
        unsigned char  Protocol;
        unsigned short HeaderChksum;
        unsigned int SourceAddress;
        unsigned int DestinationAddress;
}  ipv4_hdr_t;

typedef struct _tNatt_Socket_v4{
        struct _tNatt_Socket_v4 *next;
        unsigned int src_ip;
        unsigned int dst_ip;
        unsigned short sport;
        unsigned short dport;
}Natt_Socket_v4, *PNatt_Socket_v4;



/* 
* IPV6
*
*/

typedef struct _tNatt_Socket_v6{
        struct _tNatt_Socket_v6 *next;
        unsigned int src_ip[4];
        unsigned int dst_ip[4];
        unsigned short sport;
        unsigned short dport;
}Natt_Socket_v6, *PNatt_Socket_v6;

#endif

