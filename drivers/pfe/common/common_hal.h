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

#ifndef _COMMON_HAL_H_
#define _COMMON_HAL_H_


#define ALIGN64(x)      (((u32)(x)) & ~0x7)
#define ROUND_UP64(x)   (((u32)(x) + 0x7) & ~0x7)

#define ALIGN32(x)      (((u32)(x)) & ~0x3)
#define ROUND_UP32(x)   (((u32)(x) + 0x3) & ~0x3)


#if !defined(COMCERTO_2000_CONTROL)

#if !defined(COMCERTO_2000)

#include "heapmgr.h"

#define memset(d,v,l)		SFL_memset(d,v,l)
#define SFL_memcpy(d,s,l)	SFL_burstmemcpy(d,s,l)

/* Uses ARM tools library functions */
int strcmp(const char *s1, const char *s2);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, unsigned int size);

#else /* defined(COMCERTO_2000) */

/* Not used in data path */

#define SFL_memcpy(d,s,l)	memcpy(d,s,l)	/* FIXME, this may not be the best option */

#endif /* defined(COMCERTO_2000) */

#define unlikely(arg)	__builtin_expect(arg, 0)
#define likely(arg)	__builtin_expect(arg, 1)

#define container_of(entry, type, member)	((type *)((char *)(entry) - (char *)&(((type *)0)->member)))

#define offsetof(type, member) ((unsigned int)(&((type *)0)->member))

#define max(a,b) (((a) < (b)) ? (b) : (a))
#define min(a,b) (((a) > (b)) ? (b) : (a))

static __inline U32 __swab32(U32 x)
{
        U32 __x = x;
        __x = (((U32)(__x) & (U32)0x000000ffUL) << 24) |
                (((U32)(__x) & (U32)0x0000ff00UL) << 8) |
                (((U32)(__x) & (U32)0x00ff0000UL) >> 8) |
                (((U32)(__x) & (U32)0xff000000UL) >> 24);
        return __x;
}


#ifdef ENDIAN_LITTLE
#define ntohs(x) ((U16)((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8)))
#define htons ntohs
#define htonl(x)	__swab32(x)
#define ntohl(x)	__swab32(x)

#else

#define ntohs(x) (x)
#define htons(x) (x)
#define ntohl(x) (x)
#define htonl(x) (x)


#define __cpu_to_le16(x) ((U16)((((x) & 0xFF00) >> 8) | (((x) & 0x00FF) << 8)))
#define __le16_to_cpu(x)  __cpu_to_le16(x)
#define __le32_to_cpu(x)  __swab32(x)
#define __cpu_to_le32(x)  __swab32(x)

#endif

#else /* defined(COMCERTO_2000_CONTROL) */

#define SFL_memcpy(d,s,l)	memcpy(d,s,l)

#endif /* defined(COMCERTO_2000_CONTROL) */



#if !defined(COMCERTO_2000)

// Note : it appears that __packed attribute is only taken into account for structures by armcc
// Using __packed U32* doesn't prevent the compiler from generating LDM instructions
// on unaligned pointer, which results in alignment abort!

typedef struct
{
	U32 x;
} __attribute__((packed)) PACKED_U32;

#define READ_UNALIGNED_INT(var) ((PACKED_U32 *)(&(var)))->x

static __inline void __WRITE_UNALIGNED_INT(void *_addr, U32 _val)
{
	PACKED_U32 *p32 = (PACKED_U32 *)_addr;
	p32->x =_val;
}

#define WRITE_UNALIGNED_INT(var, val) __WRITE_UNALIGNED_INT((void *)&(var), (val))

#else /* defined(COMCERTO_2000) */

typedef struct
{
	U32 x;
} PACKED_U32;	// FIXME -- need to remove all references to this for c2000

static __inline U32 __READ_UNALIGNED_INT(void *_addr) 
{
	U16 *addr16 = (U16 *)_addr;

#if defined(ENDIAN_LITTLE)
	return ((addr16[1] << 16) | addr16[0]);
#else
	return ((addr16[0] << 16) | addr16[1]);
#endif
}

#define READ_UNALIGNED_INT(var) __READ_UNALIGNED_INT(&(var))

static __inline void __WRITE_UNALIGNED_INT(void *_addr, U32 _val)
{
	U16 *addr16 = (U16 *)_addr;

#if defined(ENDIAN_LITTLE)
	addr16[0] = _val & 0x0000ffff;
	addr16[1] = _val >> 16;
#else
	addr16[0] = _val >> 16;
	addr16[1] = _val & 0x0000ffff;
#endif
}

#define WRITE_UNALIGNED_INT(var, val) __WRITE_UNALIGNED_INT(&(var), (val))

#endif /* defined(COMCERTO_2000) */


#endif /* _COMMON_HAL_H_ */
