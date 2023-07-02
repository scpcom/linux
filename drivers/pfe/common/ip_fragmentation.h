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

#ifndef _IP_FRAGMENTATION_H_
#define _IP_FRAGMENTATION_H_

#define FIRST_FRAG	0
#define INTER_FRAG	1
#define LAST_FRAG	2

typedef struct _tFrag_info {
	PVOID ddr_pkt_start;
	void *l3_hdr;
	U32 if_type;
	void *frag_data[C2000_MAX_FRAGMENTS];
	U16 frag_len[C2000_MAX_FRAGMENTS];
	U16 dmem_header_len;
	U16 mtu;
	U16 preL2_len;
	U8 num_frag;
	U8 do_ipsec;
} Frag_info, *PFrag_info;

int do_ipv4_fragmentation(PMetadata mtd, PFrag_info pFragInfo);
int do_ipv6_fragmentation(PMetadata mtd, PFrag_info pFragInfo, u8 *phdr, u32 hlen);
void ip_fragmentation_init(void);


#if defined (COMCERTO_2000_CLASS)
#define FRAG_DMEM_BUFFER	CLASS_GP_DMEM_BUF
#define FRAG_DMEM_BUFFER_SIZE	CLASS_GP_DMEM_BUF_SIZE
#endif
#if defined (COMCERTO_2000_UTIL)
#define FRAG_DMEM_BUFFER	MEMCPY_TMP_BUF
#define FRAG_DMEM_BUFFER_SIZE	MEMCPY_TMP_BUF_SIZE
#endif

#endif /* _IP_FRAGMENTATION_H_ */
