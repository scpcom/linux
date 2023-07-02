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


#ifndef _FPP_H_
#define _FPP_H_

#include "fpp_globals.h"
#include "modules.h"
#include "channels.h"
#include "fpart.h"
#include "fpool.h"
#include "version.h"


#define ARAM_ABI		0x0A000120
#define ARAM_HEAP_STATS		0x0A000124
#define DDR_HEAP_STATS		0x0A00012C

#define CPU_BUSY_AVG		*((V32 *)0x0A000134)
#define CPU_IDLE_AVG		*((V32 *)0x0A000138)

#define GEM0_RESET_COUNT	*((V32 *)0x0A00013C)
#define GEM1_RESET_COUNT	*((V32 *)0x0A000140)


void fp_main(void);
void fp_scheduler(void) __attribute__((section ("fast_path"))) __attribute__ ((noinline)) __attribute__((noreturn));


extern FASTPART HostmsgPart;
#ifdef COMCERTO_100
extern FASTPART QMRateLimitEntryPart;
extern FASTPOOL PktBufferPool;
extern FASTPOOL PktBufferDDRPool;
#endif

/* global heap */
extern U8 GlobalAramHeap[];
extern U8 GlobalHeap[];

extern U8	DSCP_to_Q[];
extern U8	CTRL_to_Q;

#ifdef CFG_DIAGS
extern void fppdiag_init(void);
extern void fppdiag_print(unsigned short log_id, unsigned int mod_id, const char* fmt, ...);
extern void fppdiag_exception_dump(U32* registers);
#endif

#endif /* _FPP_H_ */
