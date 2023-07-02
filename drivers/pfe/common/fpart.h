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


#ifndef _FASTPART_H_
#define _FASTPART_H_

#include "types.h"
#include "system.h"

typedef struct tagFASTPART {
	volatile U8 lock;
	U8 reserved1;
	U32 * volatile freeblk;	// free block to allocate
	U32 *storage;			// storage start address
	U32 blksz;				// block size DWORDs!
	U32 blkcnt;				// block count 
	U32 *end_of_storage;	// storage end address
	U16 reserved2;	
	U16 reserved3;	
	U16 freecnt;			 // number of available blocks
} FASTPART, *PFASTPART;


void	SFL_defpart(FASTPART *fp, void *storage, U32 blksz, U32 blkcnt) __attribute__ ((noinline));

static __inline void *SFL_alloc_part_lock(FASTPART *fp)
{
	U32 *p;
	DISABLE_INTERRUPTS();
	
	p = fp->freeblk;
	if (p){
		fp->freeblk = (U32 *)(*p);
		//fp->freecnt--;
	}
	ENABLE_INTERRUPTS();
	return (void *)p;
}



static __inline void *SFL_alloc_part(FASTPART *fp)
{
	U32 *p;

	p = fp->freeblk;
	if (p){
		fp->freeblk = (U32 *)(*p);
		//fp->freecnt--;
	}
	return (void *)p;
}


static __inline void SFL_free_part(FASTPART *fp, void *blk)
{

	*(U32 *)blk = (U32)fp->freeblk;
	fp->freeblk =  blk;
	//fp->freecnt++;
}

void *SFL_alloc_partlist(FASTPART *fp, U32 cnt, U32* nb);
U32 SFL_free_partlist(FASTPART *fp, void *blk, void* last, U32 n);

static __inline int SFL_querysize_part(FASTPART *fp) {
    // report max number of poolA buffers which are held under mtds
    return fp->blkcnt;
}

#endif /* _FASTPART_H_ */
