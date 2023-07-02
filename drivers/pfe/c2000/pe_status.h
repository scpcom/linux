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


#ifndef _PE_STATUS_H_
#define _PE_STATUS_H_

#include "types.h"
#include "hal.h"

typedef struct tPE_STATUS
{
	U32	cpu_state;
	U32	activity_counter;
	U32	rx;
	union {
		U32	tx;
		U32	tmu_qstatus;
	};
	U32	drop;
#if defined(CFG_PE_DEBUG)
	U32	debug_indicator;
	U32	debug[16];
#endif
} __attribute__((aligned(16))) PE_STATUS;

extern volatile PE_STATUS pe_status;

#define PESTATUS_SETSTATE(newstate) do { pe_status.cpu_state = (U32)(newstate); } while (0)
#define PESTATUS_INCR_ACTIVE() do { pe_status.activity_counter++; } while (0)
#define PESTATUS_INCR_RX() do { pe_status.rx++; } while (0)
#define PESTATUS_INCR_TX() do { pe_status.tx++; } while (0)
#define PESTATUS_SET_TMU_QSTATUS(qstatus) do { pe_status.tmu_qstatus = (U32)(qstatus); } while (0)
#define PESTATUS_SETERROR(err) do { pe_status.activity_counter = (U32)(err); } while (0)
#define PESTATUS_INCR_DROP() do { pe_status.drop++; } while (0)
#if defined(CFG_PE_DEBUG)
#define PEDEBUG_SET(i, val) do { pe_status.debug[i] = (U32)(val); } while (0)
#define PEDEBUG_INCR(i) do { pe_status.debug[i]++; } while (0)
#else
#define PEDEBUG_SET(i, val) do { } while (0)
#define PEDEBUG_INCR(i) do { } while (0)
#endif

#endif /* _PE_STATUS_H_ */
