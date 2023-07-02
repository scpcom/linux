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

#ifdef CFG_MACVLAN

#ifndef _MODULE_MACVLAN_H_
#define _MODULE_MACVLAN_H_

#include "types.h"
#include "modules.h"
//#include "layer2.h" /* FIXME recursive include */


#define NUM_MACVLAN_HASH_ENTRIES		16

typedef struct _tMacvlanEntry {
	struct itf itf;

	struct _tMacvlanEntry *next;
	struct _tMacvlanEntry *prev;

	U8 MACaddr[ETHER_ADDR_LEN];
}MacvlanEntry , *PMacvlanEntry;


typedef struct _tMacvlanCmd{
		unsigned short action;
		unsigned char macaddr[ETHER_ADDR_LEN];
		unsigned char macvlan_ifname[IF_NAME_SIZE];
		unsigned char phys_ifname[IF_NAME_SIZE];
}MacvlanCmd, *PMacvlanCmd;

extern PMacvlanEntry macvlan_cache[NUM_MACVLAN_HASH_ENTRIES];

BOOL M_macvlan_init(PModuleDesc pModule);

void M_MACVLAN_process_packet(PMetadata mtd);
#if !defined(COMCERTO_2000)
void M_macvlan_entry(void);
#endif

static __inline U8 HASH_MACVLAN(void *pmacaddr)
{
	U8  hash;
	U16 sum;
	U16  *macAddr  = (U16 *)pmacaddr;

	sum = ( macAddr[0] ^ macAddr[1] ^ macAddr[2]);
	hash = (sum >> 8) ^ (sum & 0xFF);

	return ((hash ^ (hash >> 4)) & (NUM_MACVLAN_HASH_ENTRIES - 1));
}

#endif /* _MODULE_MACVLAN_H_ */
#endif /* CFG_MACVLAN */

