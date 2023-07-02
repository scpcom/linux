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


#ifndef _MODULE_IPV4FRAG_H_
#define _MODULE_IPV4FRAG_H_

#include "types.h"
#include "mtd.h"
#include "common_hdrs.h"


#if defined(COMCERTO_2000_UTIL) || !defined(COMCERTO_2000)
#include "fpart.h"
#include "channels.h"
#include "Module_ipv6frag.h" //use common definitions from ip6 reassembly include


typedef struct _tFragIP4
{
	struct _tFragIP4 	*next;
#if defined(COMCERTO_2000)
	struct _tFragIP4	*ddr_addr;
#endif
	PMetadata	mtd_frags;   	/* linked list of packets */

	U32		id;				/* fragment id		*/
	U32 		sAddr;
	U32 		dAddr;

	U32		refcnt;			/* number of fragments */
	int		timer;      	/* when will this queue expire */

	U32		cumul_len;   	/* cumulative len of received fragments */
	U8		last_in;    	/* first/last segment arrived? */
	U8		protocol;	/* layer 4 protocol */
	U16		end;
	U32		hash;
	U32		seq_num;
} FragIP4 , *PFragIP4;

void ipv4_frag_init(void);
BOOL M_ipv4_frag_init(PModuleDesc pModule);
void M_ipv4_frag_timer(void);
void M_ipv4_frag_entry(void);

#define HASH_FRAG4	HASH_FRAG6

extern FASTPART frag4Part;
extern FragIP4 Frag4Storage[];

#elif defined (COMCERTO_2000_CLASS)
void M_FRAG4_process_packet(PMetadata mtd);
#endif


#if defined(COMCERTO_2000_CONTROL) || !defined(COMCERTO_2000)
int IPv4_HandleIP_Set_FragTimeout(U16 *p, U16 Length, U16 sam);
#endif

#endif /* _MODULE_IPV4FRAG_H_ */
