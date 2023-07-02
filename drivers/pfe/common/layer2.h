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


#ifndef _LAYER2_H_
#define _LAYER2_H_

#include "types.h"
#include "hal.h"
#include "modules.h"
#include "channels.h" 
#include "fe.h"
#if !defined(COMCERTO_2000_UTIL)
#include "module_ethernet.h"
#include "module_vlan.h"
#include "module_pppoe.h"
#endif

/**************************************************
* Layer 2 Management 
*
***************************************************/


#define L2_MAX_ONIF		255
#define L2_INVALID_ONIF		L2_MAX_ONIF

#define INTERFACE_NAME_LENGTH	12

/* FLAGS */
//Routing cache entry
#define UNICAST_L2_HEADER_SIZE	26	// 14 (eth) + 4 (vlan) + 8 (pppoe)

// INTERFACE FLAGS
#define ENTRY_VALID	0x80

#define	IF_TYPE_ETHERNET	(1 << 0)
#define	IF_TYPE_VLAN		(1 << 1)
#define	IF_TYPE_PPPOE		(1 << 2)
#define	IF_TYPE_TUNNEL		(1 << 3)
#define	IF_TYPE_MACVLAN		(1 << 4)
#define IF_TYPE_WLAN		(1 << 5)
#define IF_TYPE_L2TP		(1 << 6)
#define IF_TYPE_PHYSICAL	(1 << 7)


#if defined(COMCERTO_2000)
#if defined(COMCERTO_2000_CONTROL)
typedef struct _tRouteEntry {
	struct slist_entry list;
	U16 nbref;
	U16 id;
	struct itf *itf;
	U8 dstmac[ETHER_ADDR_LEN];
	U16 mtu;
	U16 flags;
	union
	{
		U32 Daddr_v4;
		U32 Daddr_v6[4];
	};
}RouteEntry, *PRouteEntry;

struct hw_route {
	U32 itf;
	U8 dstmac[ETHER_ADDR_LEN];
	U16 mtu;
//	U16 flags;
	U32 Daddr_v4;
};

struct hw_route_4o6 {
	U32 itf;
	U8 dstmac[ETHER_ADDR_LEN];
	U16 mtu;
//	U16 flags;
	U32 Daddr_v6[4];
};


#else
typedef struct _tRouteEntry {
	struct itf *itf;
	U8 dstmac[ETHER_ADDR_LEN];
	U16 mtu;
//	U16 flags;
	U32 Daddr_v4;
}RouteEntry, *PRouteEntry;

typedef struct _tRouteEntry_4o6 {
	struct itf *itf;
	U8 dstmac[ETHER_ADDR_LEN];
	U16 mtu;
//	U16 flags;
	U32 Daddr_v6[4];
}RouteEntry_4o6, *PRouteEntry_4o6;

#endif
#else
typedef struct _tRouteEntry {
	struct slist_entry list;
	U16 nbref;
	U16 id;
	struct itf *itf;
	U8 dstmac[ETHER_ADDR_LEN];
	U16 mtu;
	U16 flags;
}RouteEntry, *PRouteEntry;
#endif

#define RT_F_EXTRA_INFO 0x1

#if !defined(COMCERTO_2000)
#define IS_ARAM_ROUTE(pRoute) (((U32)(pRoute) & 0xFF000000) == 0x0A000000)
#define IS_DDR_ROUTE(pRoute) (((U32)(pRoute) & 0xFF000000) == 0x80000000)
#define IS_NULL_ROUTE(pRoute) (((U32)(pRoute) & 0xFF000000) == 0x00000000)
#define ROUTE_EXTRA_INFO(rt) ((void *)(ROUND_UP32((unsigned long)((rt) + 1))))
#elif defined(COMCERTO_2000_CONTROL)
#define IS_NULL_ROUTE(pRoute) (!(pRoute))
/* In the case of C2000 control or C2000 the structures hw_route and hw_route_4o6 are the same  
till the  first word of the Dstn address, so we can safely typecast the route to type hw_route *
and pass the address of Daddr_v4, even for the 4o6 case. */

#define ROUTE_EXTRA_INFO(rt) ((void *)(&(rt)->Daddr_v4))
#else
#define IS_NULL_ROUTE(pRoute) (!(pRoute) || ((pRoute)->itf == (void *)0xffffffff))
#define ROUTE_EXTRA_INFO(rt) ((void *)(&((PRouteEntry)rt)->Daddr_v4))
#endif


#if !defined(COMCERTO_2000_UTIL)

typedef struct tOnifDesc {
	U8	name[INTERFACE_NAME_LENGTH];	// interface name string as managed by linux
	struct itf *itf;

	U8	flags;
}OnifDesc, *POnifDesc;

extern OnifDesc gOnif_DB[] __attribute__((aligned(32))) ;

PRouteEntry L2_route_get(U32 id);
void L2_route_put(PRouteEntry pRtEntry);
PRouteEntry L2_route_find(U32 id) __attribute__ ((noinline));
int L2_route_remove(U32 id) __attribute__ ((noinline));
PRouteEntry L2_route_add(U32 id, int info_size);


POnifDesc get_onif_by_name(U8 *itf_name) __attribute__ ((noinline));
POnifDesc add_onif(U8 *input_itf_name, struct itf *itf, struct itf *phys_itf, U8 type) __attribute__ ((noinline));
void remove_onif_by_name(U8 *itf_name) __attribute__ ((noinline));
void remove_onif_by_index(U32 if_index) __attribute__ ((noinline));
void remove_onif(POnifDesc onif_desc) __attribute__ ((noinline));
U8 itf_get_phys_port(struct itf *itf);
U16 l2_prepend_header(PMetadata mtd, PRouteEntry pRtEntry, U16 family);
U16 l2_precalculate_header(struct itf *itf, U8 *data, U16 size, U16 ethertype, U8 *dstMac, U8 *output_port);

#ifdef CFG_STATS
#include "module_pppoe.h"
#include "module_stat.h"
#endif


#include "module_macvlan.h"



/**
 * get_onif_by_index()
 *
 *
 */
static __inline POnifDesc get_onif_by_index(U8 index)
{
	return  &gOnif_DB[index];
}

/**
 * get_onif_index()
 *
 *
 */
static __inline U32 get_onif_index(POnifDesc onif_desc)
{
	return onif_desc - &gOnif_DB[0];
}

/**
 * get_onif_name()
 *
 *
 */
static __inline char *get_onif_name(U8 onif_index)
{
	return (char *)gOnif_DB[onif_index].name;
}


static __inline void rte_set_mtu(PRouteEntry prte,U16 mtu) {
  prte->mtu = mtu == 0 ? 0xFFFF : mtu;
}

static __inline U32 l2_get_tid(U16 family)
{
	return family == PROTO_IPV6 ? ETHERTYPE_IPV6_END : ETHERTYPE_IPV4_END;
}

#if !defined(COMCERTO_2000_CONTROL)
__attribute__((always_inline))
static inline U16 __l2_prepend_header(PMetadata mtd, struct itf *itf, U8 *dst_mac, U16 ethertype, U8 update)
{
	U8 *srcMac = NULL;
	U16 l2_hdr_size = ETH_HEADER_SIZE;

	if (itf->type & IF_TYPE_PPPOE)
	{
		M_pppoe_encapsulate(mtd, (pPPPoE_Info)itf, ethertype, update);

		l2_hdr_size += PPPOE_HDR_SIZE;
		ethertype = ETHERTYPE_PPPOE_END;
		itf = itf->phys;
	}

#ifdef CFG_MACVLAN
	if (itf->type & IF_TYPE_MACVLAN)
	{
		srcMac = ((PMacvlanEntry)itf)->MACaddr;
		itf = itf->phys;
	}
#endif

	if (itf->type & IF_TYPE_VLAN)
	{
		M_vlan_encapsulate(mtd, (PVlanEntry)itf, ethertype, update);
		l2_hdr_size += VLAN_HDR_SIZE;
		ethertype = ETHERTYPE_VLAN_END;
		itf = itf->phys;

		/* QinQ */
		if (itf->type & IF_TYPE_VLAN)
		{
			M_vlan_encapsulate(mtd, (PVlanEntry)itf, ethertype, update);
			l2_hdr_size += VLAN_HDR_SIZE;
			itf = itf->phys;
		}
	}

	mtd->length += ETH_HEADER_SIZE;
	mtd->offset -= ETH_HEADER_SIZE;

#ifdef CFG_MACVLAN
	if (itf->type & IF_TYPE_MACVLAN)
	{
		srcMac = ((PMacvlanEntry)itf)->MACaddr;
		itf = itf->phys;
	}
	else
#endif
		if (!srcMac)
			srcMac = ((struct physical_port *)itf)->mac_addr;

	mtd->output_port = ((struct physical_port *)itf)->id;

	COPY_MACHEADER(mtd->data + mtd->offset, dst_mac, srcMac, ethertype);

	return l2_hdr_size;
}
#endif
#endif

#endif /* _LAYER2_H_ */

