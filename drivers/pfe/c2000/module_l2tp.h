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
#include "fe.h"

#if defined (COMCERTO_2000)
/* Commands */ 
typedef struct _tl2tp_itf_add_cmd {
	U8 ifname[16];
	U16 sock_id;
	U16	local_tun_id;
	U16	peer_tun_id;
	U16	local_ses_id;
	U16	peer_ses_id;
	U16	options;	
}__attribute__((__packed__)) l2tp_itf_add_cmd, *pl2tp_itf_add_cmd;

typedef struct _tl2tp_itf_del_cmd {
	char ifname[16];
}__attribute__((__packed__)) l2tp_itf_del_cmd, *pl2tp_itf_del_cmd;

/* Entries */
typedef struct _tl2tp_entry {
	struct itf itf;
	struct slist_entry	list;
	U16 local_tun_id;
	U16 peer_tun_id;
	U16 local_ses_id;
	U16 peer_ses_id;
	U16 sock_id;
	U16 options;
}l2tp_entry, *pl2tp_entry;;

#define MAX_L2TP_ITF	60
#define L2TP_IF_FREE	0
static __inline U32 HASH_L2TP(U16 tun_id, U16 ses_id)
{
	U32 sum;

	tun_id = ntohs(tun_id);
	ses_id = ntohs(ses_id);

	sum = tun_id + ses_id;
	sum ^= (sum >> 8);
	return (sum & (NUM_L2TP_ENTRIES - 1));
}

extern struct slist_head l2tp_cache[];
extern l2tp_entry l2tp_table[];


#if defined(COMCERTO_2000_CLASS)
/* PBUF route entry memory chunk is used to allocated a socket entry */
#define L2TP_BUFFER()\
    ((PVOID)(CLASS_ROUTE0_BASE_ADDR))
#endif
int l2tp_init(void);
void l2tp_exit(void);

void M_L2TP_RX_process_packet(PMetadata mtd);
void M_L2TP_TX_process_packet(PMetadata mtd);

/* Bit definitions */
#define TYPE_BIT         0x80
#define LENGTH_BIT       0x40
#define SEQUENCE_BIT     0x08
#define OFFSET_BIT       0x02
#define PRIORITY_BIT     0x01
#define RESERVED_BITS    0x34
#define VERSION_MASK     0x0F
#define VERSION_RESERVED 0xF0

#define L2TP_VERSION_TWO	2
/* L2TP Options */
#define L2TP_OPT_LENGTH		0x0001
#define L2TP_OPT_SEQ		0x0002

#define L2TP_HDR_LEN		(6 + 4)	//No options hdr + PPP
#define L2TP_OPT_LENGTH_LEN	2
#define L2TP_OPT_SEQ_LEN	4

#define PPP_L2TP_CTRL_ADDR	0xff03

#endif
