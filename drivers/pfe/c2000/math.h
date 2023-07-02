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


// Note that Macros are all uppercase

#ifndef _MATH_H_
#define _MATH_H_

#include "types.h"

int get_highest_bit(U32 val);
#if defined(COMCERTO_2000_TMU)
int get_lowest_bit(u32 val);
#endif
#if defined (CFG_PCAP) && (COMCERTO_2000_CLASS)
u32 mul(u32 n, u32 m);
u32 div(u32 a, u32 b);
#endif
#endif /* _MATH_H_ */
