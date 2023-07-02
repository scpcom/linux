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


#ifndef _ICC_H_
#define _ICC_H_

#ifdef CFG_ICC
#include "types.h"
#include "channels.h"

#define ICC_DROP	1
#define ICC_NODROP	0

#define MAX_BMU1_THRESHOLD	128
#define MAX_BMU2_THRESHOLD	256

#define MAX_ICC_ETHERTYPE	4
#define MAX_ICC_IPV4ADDR	4
#define MAX_ICC_IPV6ADDR	4
#define MAX_ICC_PORT	4
#define MAX_ICC_VLAN	4

typedef struct {
	U32	addr;
	U8	masklen;
	U8	src_dst_flag;		// 0=src addr, 1=dst addr
} ICCTAB_IPADDR;

typedef struct {
	U32	addr[4];
	U8	masklen;
	U8	src_dst_flag;		// 0=src addr, 1=dst addr
} ICCTAB_IP6ADDR;

typedef struct {
	U16	sport_from;
	U16	sport_to;
	U16	dport_from;
	U16	dport_to;
} ICCTAB_PORT;

typedef struct {
	U16	vlan_from;
	U16	vlan_to;
	U16	prio_from;
	U16	prio_to;
} ICCTAB_VLAN;

typedef struct
{
	U32 bmu1_threshold;
	U32 bmu2_threshold;

	U8 ipproto[GEM_PORTS][256 / 8];

	U8 dscp[GEM_PORTS][64 / 8];

	U8 num_ethertype[GEM_PORTS];
	U8 num_ipv4addr[GEM_PORTS];
	U8 num_ipv6addr[GEM_PORTS];
	U8 num_port[GEM_PORTS];
	U8 num_vlan[GEM_PORTS];

	U16 ethertype[GEM_PORTS][MAX_ICC_ETHERTYPE];

	ICCTAB_IPADDR ipv4addr[GEM_PORTS][MAX_ICC_IPV4ADDR];
	ICCTAB_IP6ADDR ipv6addr[GEM_PORTS][MAX_ICC_IPV6ADDR];

	ICCTAB_PORT port[GEM_PORTS][MAX_ICC_PORT];

	ICCTAB_VLAN vlan[GEM_PORTS][MAX_ICC_VLAN];
} ICCTAB;


extern ICCTAB icctab;

void pfe_icc_init(void);
int pfe_icc(U32 interface, PMetadata mtd);

#if !defined(COMCERTO_2000_CONTROL)
static inline int pfe_icc_test(U32 interface, PMetadata mtd)
{
	U32 bmu1_current_depth;
	U32 bmu2_current_depth;
	bmu1_current_depth = readl(PERG_BMU1_CURRDEPTH) & 0xFFFF;
	bmu2_current_depth = readl(PERG_BMU2_CURRDEPTH) & 0xFFFF;

	if (unlikely(bmu1_current_depth < icctab.bmu1_threshold || bmu2_current_depth < icctab.bmu2_threshold))
	{
		return pfe_icc(interface, mtd); 
	}
	else
		return ICC_NODROP;
}
#endif


#if defined(COMCERTO_2000_CONTROL)

#define ICC_ACTION_ADD          0
#define ICC_ACTION_DELETE       1

enum {
	ICC_TABLETYPE_ETHERTYPE = 0,
	ICC_TABLETYPE_PROTOCOL,
	ICC_TABLETYPE_DSCP,
	ICC_TABLETYPE_SADDR,
	ICC_TABLETYPE_DADDR,
	ICC_TABLETYPE_SADDR6,
	ICC_TABLETYPE_DADDR6,
	ICC_TABLETYPE_PORT,
	ICC_TABLETYPE_VLAN,
	ICC_MAX_TABLETYPE
};

typedef struct icc_reset_cmd {
	U16	reserved1;
	U16	reserved2;
} __attribute__((__packed__)) icc_reset_cmd_t;

typedef struct icc_threshold_cmd {
	U16	bmu1_threshold;
	U16	bmu2_threshold;
} __attribute__((__packed__)) icc_threshold_cmd_t;

typedef struct icc_add_delete_cmd {
	U16	action;
	U8	interface;
	U8	table_type;
	union {
		struct {
			U16 type;
		} ethertype;
		struct {
			U8 ipproto[256 / 8];
		} protocol;
		struct {
			U8 dscp_value[64 / 8];
		} dscp;
		struct {
			U32 v4_addr;
			U8 v4_masklen;
		} ipaddr;
		struct {
			U32 v6_addr[4];
			U8 v6_masklen;
		} ipv6addr;
		struct {
			U16 sport_from;
			U16 sport_to;
			U16 dport_from;
			U16 dport_to;
		} port;
		struct {
			U16 vlan_from;
			U16 vlan_to;
			U16 prio_from;
			U16 prio_to;
		} vlan;
	};
} __attribute__((__packed__)) icc_add_delete_cmd_t;

typedef struct icc_query_cmd {
	u_int16_t	action;
	u_int8_t	interface;
	u_int8_t	reserved;
} __attribute__((__packed__)) icc_query_cmd_t;

typedef struct icc_query_reply {
	u_int16_t	rtncode;
	u_int16_t	query_result;
	u_int8_t	interface;
	u_int8_t	table_type;
	union {
		struct {
			u_int16_t type;
		} ethertype;
		struct {
			u_int8_t ipproto[256 / 8];
		} protocol;
		struct {
			u_int8_t dscp_value[64 / 8];
		} dscp;
		struct {
			u_int32_t v4_addr;
			u_int8_t v4_masklen;
		} ipaddr;
		struct {
			u_int32_t v6_addr[4];
			u_int8_t v6_masklen;
		} ipv6addr;
		struct {
			u_int16_t sport_from;
			u_int16_t sport_to;
			u_int16_t dport_from;
			u_int16_t dport_to;
		} port;
		struct {
			u_int16_t vlan_from;
			u_int16_t vlan_to;
			u_int16_t prio_from;
			u_int16_t prio_to;
		} vlan;
	};
} __attribute__((__packed__)) icc_query_reply_t;

int icc_control_init(void);
void icc_control_exit(void);

#endif //COMCERTO_2000_CONTROL

#endif /* CFG_ICC */
#endif /* _ICC_H_ */

