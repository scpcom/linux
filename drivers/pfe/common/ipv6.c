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

#include "ipv6.h"

static int ipv6_cmp_aligned(void *src, void *dst)
{
	u32 off = 0;

	if ((unsigned long)src & 0x2) {
		if (*(u16 *)src - *(u16 *)dst)
			return 1;

		off = 2;
		src += 2;
		dst += 2;
	}

	if ((*(u32 *)(src + 0) - *(u32 *)(dst + 0)) ||
	    (*(u32 *)(src + 4) - *(u32 *)(dst + 4)) ||
	    (*(u32 *)(src + 8) - *(u32 *)(dst + 8)))
		return 1;

	if (off)
		return (*(u16 *)(src + 12) - *(u16 *)(dst + 12)) ? 1 : 0;
	else
		return (*(u32 *)(src + 12) - *(u32 *)(dst + 12)) ? 1 : 0;
}

static int ipv6_cmp_unaligned(void *src, void *dst)
{
	return ((*(u32 *)(src + 0) - READ_UNALIGNED_INT(*(u32 *)(dst + 0))) ||
		(*(u32 *)(src + 4) - READ_UNALIGNED_INT(*(u32 *)(dst + 4))) ||
		(*(u32 *)(src + 8) - READ_UNALIGNED_INT(*(u32 *)(dst + 8))) ||
		(*(u32 *)(src + 12) - READ_UNALIGNED_INT(*(u32 *)(dst + 12))));
}

int ipv6_cmp(void *src, void *dst)
{
	if (((unsigned long)src & 0x3) == ((unsigned long)dst & 0x3))
		return ipv6_cmp_aligned(src, dst);
	else {
		if ((unsigned long)src & 0x3)
			return ipv6_cmp_unaligned(dst, src);
		else
			return ipv6_cmp_unaligned(src, dst);
	}
}

