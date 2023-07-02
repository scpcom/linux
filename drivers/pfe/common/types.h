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


/*******************************************************************
 *
 *    NAME: types.h     
 * 
 *    DESCRIPTION: Defines types
 *
 *******************************************************************/

#ifndef _TYPES_H_
#define _TYPES_H_

#include "config.h"

// Make sure ENDIAN variable is defined properly
#if !defined(ENDIAN_LITTLE) && !defined(ENDIAN_BIG)
#error Must define either ENDIAN_LITTLE or ENDIAN_BIG
#endif

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
// NOTE: The PFE GNU toolchain aligns doubleword variables on 4-byte boundaries.
//	To keep shared data structures compatible with the 8-byte alignment of
//	the ARM toolchain, we force U64/V64 to be 8-byte aligned.
typedef unsigned long long U64 __attribute__((aligned(8)));

typedef volatile unsigned char V8;
typedef volatile unsigned short V16;
typedef volatile unsigned int V32;
typedef volatile unsigned long long V64 __attribute__((aligned(8)));

typedef unsigned char BOOL;

typedef void VOID; 
typedef void *PVOID; 

typedef signed char  S8;
typedef signed short S16;
typedef signed int   S32;

#define IF_NAME_SIZE    16

typedef void (*INTVECHANDLER)(void);
typedef	void (*EVENTHANDLER)(void);
typedef int (*GETFILLHANDLER)(void);

typedef struct tSTIME {
	U32 msec;
	U32 cycles;
} STIME;

/** Structure common to all interface types */
struct itf {
	struct itf *phys;	/**< pointer to lower lever interface */
	U8 type;		/**< interface type */
	U8 index;		/**< unique interface index */
};

struct physical_port_util {
	U8 flags;
};

struct physical_port {
	struct itf itf;
	U8 mac_addr[6];
	U8 id;
	U8 flags;

#ifdef CFG_STATS
	/*stats*/
	U64 rx_bytes __attribute__((aligned(8)));
	U32 rx_pkts;
#endif
};

/*physical_port flags bit fields */
#define TX_ENABLED		(1 << 0)
#define L2_BRIDGE_ENABLED	(1 << 1)
#define QOS_ENABLED		(1 << 2)

typedef struct tDataQueue {
	void* head;
	void* tail;
} DataQueue, *PDataQueue;

struct tMetadata;

#if defined(COMCERTO_100) || defined(COMCERTO_1000)
typedef int channel_t;
#elif defined(COMCERTO_2000)
typedef void (*channel_t)(struct tMetadata *);
#endif

#define INLINE	__inline

#define TRUE	1
#define FALSE	0

#if !defined(COMCERTO_2000_CONTROL)
#define NULL	(PVOID)0		/* match rtxc def */
#endif

#define HANDLE	PVOID

#define K 			1024
#define M			(K*K)

#define __TOSTR(v)	#v
#define TOSTR(v)	__TOSTR(v)

// enum used to idenity mtd source 
enum FPP_L3_PROTO {
    PROTO_IPV4 = 0,
    PROTO_IPV6,
    PROTO_PPPOE,
    PROTO_MC4,    
    PROTO_MC6,
    MAX_L3_PROTO	
};
#define PROTO_NONE 0xFF

enum FPP_L4_PROTO {
	PROTO_L4_TCP=0,
	PROTO_L4_UDP,
	PROTO_L4_UNKNOWN,
	MAX_L4_PROTO
};



enum FPP_ITF {
	ITF_ETH0 = 0,
	ITF_ETH2
};

#define MTD_PRIV 3

#endif /* _TYPES_H_ */
