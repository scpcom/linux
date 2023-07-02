/*  
 *  Copyright © 2011, 2014 Freescale Semiconductor, Inc.
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


/*****************************************************************************************
*       MMU.H                                                                           *
*                                                                                       *
*       This file MMU descriptor defines and MMU interface functions declaration        *
*                                                                                       *
*****************************************************************************************/ 

#ifndef MMU_H
#define MMU_H

#include "types.h"

#if	!defined(GenMMUTab)
extern void memset(void *dst, int val, unsigned long length);
#endif


/*
 * First Level Descriptor Attributes
 */
#define PGE_TYPE_FAULT			0x00000000
#define PGE_TYPE_COARSE			0x00000001
#define PGE_TYPE_SECTION		0x00000002
#define PGE_TYPE_SUPERSECTION	0x00040002  /* ARM11 only */
#define PGE_TYPE_RESERVED		0x00000003	/* ARM11 only */
#define PGE_TYPE_MASK			0x00040003
#define PGE_DOMAIN_MASK			0x000001E0
#define PGE_DOMAIN_SHIFT		5
#define PGE_DOMAIN(x)           (((x) << 5) & PGE_DOMAIN_MASK)
#define PGE_AP_MASK				0x00000C00
#define PGE_AP_SHIFT			10
#define PGE_CACHE_MASK			0x0000000C
#define PGE_CACHE_SHIFT			2

#define PGE_SECTION_BASE_MASK	0xFFF00000
#define PGE_SECTION_MASK		0x000FFFFF
#define PGE_SUPER_BASE_MASK		0xFF000000
#define PGE_SUPER_MASK			0x00FFFFFF
#define PGE_COARSE_BASE_MASK	0xFFFFFC00
#define PGE_COARSE_MASK			0x000003FF
#define PGE_COARSE_MASK_LARGE		0x0000FFFF

#define PGE_TYPE(pge) ((((pge) & 0x03) != PGE_TYPE_SECTION) ? (pge) & 0x03 : (pge) & PGE_TYPE_MASK)
/*
 * First Level Descriptor Base Address
 */
#define PGE_BASE_MASK(type) 	\
	(~(1 << PG_BASE_SHIFT(type)) + 1)

#define PGE_BASE_SHIFT(type)	\
	(((type) & PG_TYPE_MASK) == PG_TYPE_SECTION) ? 20 :  \
	(((type) & PG_TYPE_MASK) == PG_TYPE_COARSE)  ? 10 :  \
	(((type) & PG_TYPE_MASK) == PG_TYPE_FINE)    ? 12 :  \
	(((type) & PG_TYPE_MASK) == PG_TYPE_SUPERSECTION) ? 24 : 31

/*
 * Number of entries in First Level Table
 */
#define PGE_ENTRIES				4096
#define PGE_PAGE_SIZE			(4*PGE_ENTRIES)
#define PGE_ENTRY_SHIFT			20
#define PGE_ENTRY_SIZE			(1 << PGE_ENTRY_SHIFT)

/*
 * Second Level Descriptor Attributes
 */
#define PTE_TYPE_FAULT			0x00000000
#define PTE_TYPE_LARGE_PAGE		0x00000001
#define PTE_TYPE_SMALL_PAGE		0x00000002	// 
#define PTE_TYPE_TINY_PAGE		0x00000003	// ARM920T
#define PTE_TYPE_MASK			0x00000003
#define PTE_CACHE_SHIFT			2
#define PTE_AP0_SHIFT			4
#define PTE_AP1_SHIFT			6
#define PTE_AP2_SHIFT			8
#define PTE_AP3_SHIFT			10

/*
 * Second Level Descriptor Base Address
 */
#define PTE_BASE_MASK(type)	\
	(~(1 << PG_BASE_SHIFT(type)) + 1)

#define PTE_BASE_SHIFT(type)	\
	(((type) & PG_TYPE_MASK) == PTE_TYPE_LARGE_PAGE) ? 16 :  \
	(((type) & PG_TYPE_MASK) == PTE_TYPE_SMALL_PAGE) ? 12 :  \
	(((type) & PG_TYPE_MASK) == PTE_TYPE_TINY_PAGE)  ? 10 : 31

/*
 * Number of entries in Second Level Table
 */
#define PTE_ENTRIES				256
#define PTE_ENTRY_SHIFT			12
#define PTE_ENTRY_SIZE			(1 << PTE_ENTRY_SHIFT)
#define PTE_PAGE_SIZE			(4*PTE_ENTRIES)

/*
 * Minimal MMU unit size (based on small page)
 */
#define PAGE_SIZE				4096
#define PAGE_SHIFT				12
#define PAGE_MASK				0xFFF

#define PAGE_MASK_LARGE				0xFFFF

/* MMU_Map access_type values */
#define	MMU_NO_ACCESS			0x00
#define	MMU_SVC_RW				0x01
#define MMU_USR_RO				0x02
#define	MMU_ALL_ACCESS			0x03

/* MMU_Map cache_mode values */
#define	MMU_CACHE_OFF			0x00
#define	MMU_CACHE_B				0x01
#define	MMU_CACHE_C				0x02
#define	MMU_CACHE_CB			0x03

/* MMU_LockDownTLB tlb values */
#define MMU_DTLB				0x01
#define MMU_ITLB				0x02

/*
 * MMU Interface functions
 */
int  MMU_Init(void);
void MMU_LockDownTLB(U32 addr, U32 tlb, U32 victim);
int  MMU_Map(U64 start_addr, U64 size, U32 access_type, U32 cache_mode);

#endif /* MMU_H */

