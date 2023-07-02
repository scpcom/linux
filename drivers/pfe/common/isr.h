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


#ifndef _ISR_H_
#define _ISR_H_

#include "types.h"

#define LO_EXPVECT_BASE 0x00000000
#define HI_EXPVECT_BASE 0xFFFF0000

#define RESETVEC		0x0
#define UNDEFVEC		0x4
#define SWIVEC			0x8
#define PABTVEC			0xc
#define DABTVEC			0x10
#define IRQVEC			0x18
#define FIQVEC			0x1c

BOOL ISR_setVector(int vecnum, INTVECHANDLER hdlr);

void irq_fromhost(void);
void irq_tohost(void);

void irq_timerA(void);
void irq_timerSW(void);

#endif /* _ISR_H */
