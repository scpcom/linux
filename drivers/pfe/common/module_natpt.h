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


#ifndef _MODULE_NATPT_H_
#define _MODULE_NATPT_H_

#include "types.h"
#include "modules.h"
#include "mtd.h"

#if !defined(COMCERTO_2000)
#define NUM_NATPT_ENTRIES 	2048
#else
#define NUM_NATPT_ENTRIES 	256
#endif

#define NATPT_CONTROL_6to4	0x0001
#define NATPT_CONTROL_4to6	0x0002


#if !defined(COMCERTO_2000)	

typedef struct _tNATPT_Entry {
	struct _tNATPT_Entry *next;
	U16	socketA;
	U16	socketB;
	U16	control;
	U8	protocol;
	U8	tcpfin_flag;
	U64	stat_v6_received;
	U64	stat_v6_transmitted;
	U64	stat_v6_dropped;
	U64	stat_v6_sent_to_ACP;
	U64	stat_v4_received;
	U64	stat_v4_transmitted;
	U64	stat_v4_dropped;
	U64	stat_v4_sent_to_ACP;
} NATPT_Entry, *PNATPT_Entry;

#elif defined (COMCERTO_2000_CONTROL)

/* control path HW natpt entry */

typedef struct _thw_natpt {
	U32 	dma_addr;

	U16	socketA;
	U16	socketB;
	U16	control;
	U8	protocol;
	U8	tcpfin_flag;
	struct {
	  U64	stat_v6_received;
	  U64	stat_v6_transmitted;
	  U64	stat_v6_dropped;
	  U64	stat_v6_sent_to_ACP;
	  U64	stat_v4_received;
	  U64	stat_v4_transmitted;
	  U64	stat_v4_dropped;
	  U64	stat_v4_sent_to_ACP;
	} stats[NUM_PE_CLASS] __attribute__((aligned(8)));

	/* HOST only , these fields are only used by host software, so keep them at the end of the structure */
	struct dlist_head	list;	/* only used for delayed removal list */
	struct _tNATPT_Entry *sw_natpt;	/*pointer to the software natpt entry */
	unsigned long		removal_time;
} hw_natpt;

/* control path SW natpt entry */

typedef struct _tNATPT_Entry {
	struct slist_entry	list;
	U32	hash;
	U16	socketA;
	U16	socketB;
	U16	control;
	U8	protocol;
	U8	tcpfin_flag;
	U64	stat_v6_received;
	U64	stat_v6_transmitted;
	U64	stat_v6_dropped;
	U64	stat_v6_sent_to_ACP;
	U64	stat_v4_received;
	U64	stat_v4_transmitted;
	U64	stat_v4_dropped;
	U64	stat_v4_sent_to_ACP;
	struct _thw_natpt   *hw_natpt;	/** pointer to the hardware natpt entry */
} NATPT_Entry, *PNATPT_Entry;

#else	// defined (COMCERTO_2000_CLASS)

/* class PE natpt entry */

typedef struct _tNATPT_Entry {
	U32 	dma_addr;

	U16	socketA;
	U16	socketB;
	U16	control;
	U8	protocol;
	U8	tcpfin_flag;
	struct {
	  U64	stat_v6_received;
	  U64	stat_v6_transmitted;
	  U64	stat_v6_dropped;
	  U64	stat_v6_sent_to_ACP;
	  U64	stat_v4_received;
	  U64	stat_v4_transmitted;
	  U64	stat_v4_dropped;
	  U64	stat_v4_sent_to_ACP;
	} stats[NUM_PE_CLASS] __attribute__((aligned(8)));
} NATPT_Entry, *PNATPT_Entry;

#endif	// defined(COMCERTO_2000)

typedef struct _tNATPTOpenCommand {
	U16	socketA;
	U16	socketB;
	U16	control;
	U16	reserved;
}NATPTOpenCommand, *PNATPTOpenCommand;

typedef struct _tNATPTCloseCommand {
	U16	socketA;
	U16	socketB;
}NATPTCloseCommand, *PNATPTCloseCommand;

typedef struct _tNATPTQueryCommand {
	U16	reserved1;
	U16	socketA;
	U16	socketB;
	U16	reserved2;
}NATPTQueryCommand, *PNATPTQueryCommand;

typedef struct _tNATPTQueryResponse {
	U16	retcode;
	U16	socketA;
	U16	socketB;
	U16	control;
	U64	stat_v6_received;
	U64	stat_v6_transmitted;
	U64	stat_v6_dropped;
	U64	stat_v6_sent_to_ACP;
	U64	stat_v4_received;
	U64	stat_v4_transmitted;
	U64	stat_v4_dropped;
	U64	stat_v4_sent_to_ACP;
}NATPTQueryResponse, *PNATPTQueryResponse;

#if !defined(COMCERTO_2000)
BOOL M_natpt_init(PModuleDesc pModule);
#else
BOOL natpt_init(void);
void natpt_exit(void);
#endif

void M_natpt_receive(void) __attribute__((section ("fast_path")));

void M_NATPT_process_packet(PMetadata mtd);

static __inline U32 HASH_NATPT(U16 socketA, U16 socketB)
{
	U32 hash;
	// This hash function is designed so that socketA and socketB can be reversed.
	hash = socketA ^ socketB;
	hash ^= hash >> 8;
	return hash & (NUM_NATPT_ENTRIES - 1);
}

#endif /* _MODULE_NATPT_H_ */
