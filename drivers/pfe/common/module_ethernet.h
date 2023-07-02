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


#ifndef _MODULE_ETHERNET_H_
#define _MODULE_ETHERNET_H_

#include "hal.h"

#define ETHER_ADDR_LEN				6
#define ETHER_TYPE_LEN			2

#define ETH_HEADER_SIZE			14
#define ETH_VLAN_HEADER_SIZE		18	// DST MAC + SRC MAC + TPID + TCI + Packet/Length
#define ETH_MAX_HEADER_SIZE		ETH_VLAN_HEADER_SIZE

/* ethernet packet types */
#define ETHERTYPE_IPV4			0x0800	// 	IP protocol version 4
#define ETHERTYPE_ARP			0x0806	//  ARP
#define ETHERTYPE_VLAN			0x8100	// 	VLAN 
#define ETHERTYPE_IPV6			0x86dd	//	IP protocol version 6
#define ETHERTYPE_MPT			0x889B 	//  IEEE mindspeed packet type
#define ETHERTYPE_PPPOE			0x8864  //  PPPoE Session packet
#define ETHERTYPE_PPPOED		0x8863  //  PPPoE Discovery packet
#define ETHERTYPE_PAE                   0x888E   /* Port Access Entity (IEEE 802.1X) */
#define ETHERTYPE_VLAN_STAG		0x88a8	// 	VLAN S-TAG
#define ETHERTYPE_UNKNOWN		0xFFFF

/* Packet type in big endianness	*/
#define ETHERTYPE_IPV4_END		htons(ETHERTYPE_IPV4)
#define ETHERTYPE_ARP_END		htons(ETHERTYPE_ARP)
#define ETHERTYPE_VLAN_END		htons(ETHERTYPE_VLAN)
#define ETHERTYPE_IPV6_END		htons(ETHERTYPE_IPV6)
#define ETHERTYPE_MPT_END		htons(ETHERTYPE_MPT)
#define ETHERTYPE_PPPOE_END		htons(ETHERTYPE_PPPOE)
#define ETHERTYPE_PPPOED_END		htons(ETHERTYPE_PPPOED)
#define ETHERTYPE_PAE_END               htons(ETHERTYPE_PAE)

typedef struct _tETHVLANHDR
{
	U8	DstMAC[6];				// Ethernet EMAC Destionation address
	U8	SrcMAC[6];				// Ethernet EMAC Source address
	U16	TPID;					// Tag Protocol Identifier or Ethernet Packet Type if packet not tagged
	U16	TCI;					// Tag Control Identifier
	U16	PacketType;				// Ethernet Packet Type / Length
	U16	RC;						// E-RIF Route Control
} ETHVLANHdr, *PETHVLANHdr;

typedef struct tEthernetFrame {
	union {
		struct {
			U8		DstMAC[ETHER_ADDR_LEN];
			U8		SrcMAC[ETHER_ADDR_LEN];
		};
		U32	dst_src_x[3];
	};
	U16		PacketType;
	U8		Payload[0];
} EthernetFrame, *PEthernetFrame;

typedef struct tEthernetHdr {
	U8	Header[ETH_MAX_HEADER_SIZE];
	U8	Length;
}EthernetHdr, *PEthernetHdr;


#endif /* _MODULE_ETHERNET_H_ */
