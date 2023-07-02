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


#ifndef _CHANNELS_H_
#define _CHANNELS_H_

#include "types.h"
#include "events.h"
#include "system.h"
#include "fpp_globals.h"
#include "fpart.h"
#include "fpool.h"
#include "gemac.h"
#include "mtd.h"


#ifdef COMCERTO_100
extern FASTPOOL PktBufferPool;
extern FASTPOOL PktBufferDDRPool;
#endif


// pktqueue_put -- put packet onto packet queue
//	returns TRUE if queue was previously empty
static __inline BOOL pktqueue_put(PDataQueue pQueue, struct tMetadata *pMetadata)
{

	pMetadata->next = NULL;
	/* add new metadata to the list tail*/
	if (pQueue->head) {
		PMetadata ptail = pQueue->tail;
		ptail->next = pMetadata;
		pQueue->tail = pMetadata;
		return FALSE;
	}
	else {
		pQueue->tail = pQueue->head = pMetadata;
		return TRUE;
	}
}

// pktqueue_get -- get packet from packet queue
//	returns NULL if queue was empty
static __inline PMetadata pktqueue_get(PDataQueue pQueue)
{
	PMetadata pMetadata;
	/* get next metadata from list head */
	pMetadata = pQueue->head;
	if (pMetadata) {
		pQueue->head = pMetadata->next;
	}
	return pMetadata;	
}

// pktqueue_isempty -- test if packet queue is empty
//	returns TRUE if queue is empty
static __inline int pktqueue_isempty(PDataQueue pQueue)
{
	return pQueue->head == NULL;
}

// pktqueue_put_multiple -- put packet list onto packet queue
//	returns TRUE if queue was previously empty
#if !defined (COMCERTO_2000_UTIL)
static __inline BOOL pktqueue_put_multiple(PDataQueue pQueue, struct tMetadata *pBeg, struct tMetadata *pEnd)
{
	PMetadata mtd;
	mtd = pBeg;
	while (mtd != pEnd)
	{
		mtd->flags |= MTD_MULTIPLE;
		mtd = mtd->next;
	}
	pEnd->next = NULL;
	/* add new metadata to the list tail*/
	if (pQueue->head) {
		PMetadata ptail = pQueue->tail;
		ptail->next = pBeg;
		pQueue->tail = pEnd;
		return FALSE;
	}
	else {
		pQueue->head = pBeg;
		pQueue->tail = pEnd;
		return TRUE;
	}
}
#endif

extern void free_packet(struct tMetadata *, int counter);

#if defined(COMCERTO_100)
static __inline PVOID pktbuffer_alloc(void)
{
	PVOID pktbuf;
	pktbuf = SFL_alloc_pool(&PktBufferPool);
	  if (pktbuf == NULL)
		  pktbuf = SFL_alloc_pool(&PktBufferDDRPool);
	return pktbuf;
}


static __inline void pktbuffer_free(PVOID pktbuf)
{
	  if (SFL_free_pool(&PktBufferPool, pktbuf, 1))
		  SFL_free_pool(&PktBufferDDRPool, pktbuf, 0);
}	


static __inline void pktbuffer_free_noclean(PVOID pktbuf)
{
	  if (SFL_free_pool(&PktBufferPool, pktbuf, 0))
		  SFL_free_pool(&PktBufferDDRPool, pktbuf, 0);
}

#elif defined(COMCERTO_1000)

static __inline void *pktbuffer_alloc(void)
{
	return buffer_get(POOLA);
}


static __inline void pktbuffer_free(void *bufp)
{
	buffer_put(bufp, POOLA, 1);
}

#elif defined(COMCERTO_2000_CLASS) || defined(COMCERTO_2000_UTIL)

#if defined(COMCERTO_2000_CLASS)
void release_ro_block(struct tMetadata *mtd);
void free_bmu2_buffers(struct tMetadata *mtd);
static inline void __free_packet(struct tMetadata *mtd)
{
	release_ro_block(mtd);
	free_bmu2_buffers(mtd);
}
#else
void __free_packet(struct tMetadata *mtd);
#endif

static __inline void *pktbuffer_alloc(void)
{
	void *buf;

	/* allocate new buffer from BMU2 */
	do {
		buf =(void *)readl(BMU2_BASE_ADDR + BMU_ALLOC_CTRL);
	} while (!buf);

	return buf;
}

static __inline void *bmu1_alloc(void)
{
	void *buf;

	/* allocate new buffer from BMU1 */
	do {
		buf =(void *)readl(BMU1_BASE_ADDR + BMU_ALLOC_CTRL);
	} while (!buf);

	return buf;
}

static __inline void pktbuffer_free(PVOID pktbuf)
{
	/*Free buffer to BMU2 pool */
	writel(((U32)pktbuf & ~(DDR_BUF_SIZE - 1)), (BMU2_BASE_ADDR + BMU_FREE_CTRL));
}

void free_buffer(void *pkt_start, int counter);
#endif

#if defined(COMCERTO_100) || defined(COMCERTO_1000)

typedef struct tCtldata {
	struct tCtldata* next;
	U16	 length;
	U16	 code;
	U16	 data[128];
} CtlData, *PCtldata;


#define	PDataChannel	PDataQueue

extern DataQueue gCH_table[NUM_CHANNEL];

__attribute__((always_inline))
static inline void data_channel_put_event(U32 eventnum, struct tMetadata *pMetadata, BOOL do_set_event)
{

	PDataChannel pChannel = &gCH_table[eventnum];
	if (pktqueue_put(pChannel, pMetadata) && do_set_event)
		set_event(gEventStatusReg, 1 << eventnum);
}

static __inline U32 data_channel_count(PDataChannel pChannel)
{
	U32 n = 0;
	PMetadata mtd;
	mtd = pChannel->head;
	while (mtd)
	{
		n++;
		mtd = mtd->next;
	}
	return n;
}

static __inline void data_channel_put_event_multiple(U32 eventnum, struct tMetadata *pBeg, struct tMetadata *pEnd, BOOL do_set_event)
{
	PDataChannel pChannel = &gCH_table[eventnum];
	if (pktqueue_put_multiple(pChannel, pBeg, pEnd) && do_set_event)
		set_event(gEventStatusReg, 1 << eventnum);
}


static __inline PMetadata data_channel_get_event(U32 eventnum)
{
	PDataChannel pChannel = &gCH_table[eventnum];
	PMetadata pMetadata;

	/* get next metadata from list head */
	pMetadata = pChannel->head;
	if (pMetadata) {
		pChannel->head = pMetadata->next;
	}
	return pMetadata;	
}

static __inline void data_channel_limit(U32 eventnum, int maxqsize)
{
	PDataChannel pChannel = &gCH_table[eventnum];
	PMetadata pMtd;
	pMtd = pChannel->head;
	while (pMtd && (--maxqsize > 0 || (pMtd->flags & MTD_MULTIPLE)))
	{
		pMtd = pMtd->next;
	}
	if (pMtd)
	{
		PMetadata pNext;
		pNext = pMtd->next;
		pChannel->tail = pMtd;
		pMtd->next = NULL;
		while (pNext)
		{
			pMtd = pNext;
			pNext = pMtd->next;
			free_packet(pMtd);
			COUNTER_INCREMENT(packets_dropped_channel_full);
		}
	}
}

static __inline void drop_fragments(PMetadata mtd)
{
	PMetadata next;

	while (mtd)
	{
		next = mtd->next;
		COUNTER_INCREMENT(packets_dropped_fragments);
		free_packet(mtd);
		mtd = next;
	}
}

static __inline int data_channel_isempty(U32 eventnum)
{
	return gCH_table[eventnum].head == NULL;
}


#define CHANNEL_NULL	((channel_t)-1)

#define OUTPUT_CHANNEL(modname)				EVENT_##modname
#define	SEND_TO(modname, pmtd)				data_channel_put_event(OUTPUT_CHANNEL(modname), pmtd, TRUE)
#define	SEND_TO_CHANNEL(eventnum, pmtd)			data_channel_put_event(eventnum, pmtd, TRUE)
#define	SEND_TO_MULTIPLE(modname, pmtd1, pmtd2)		data_channel_put_event_multiple(OUTPUT_CHANNEL(modname), pmtd1, pmtd2, TRUE)
#define	SEND_TO_CHANNEL_MULTIPLE(eventnum, pmtd1, pmtd2) data_channel_put_event_multiple(eventnum, pmtd1, pmtd2, TRUE)
#define CHANNEL_GET(eventnum)				data_channel_get_event(eventnum)
#define CHANNEL_ISEMPTY(eventnum)			data_channel_isempty(eventnum)
#define CHANNEL_LIMIT(eventnum, maxqsize)		data_channel_limit(eventnum, maxqsize)

#elif defined(COMCERTO_2000_UTIL)

#define OUTPUT_CHANNEL(modname)			M_##modname##_process_packet
#define	SEND_TO(modname, pmtd)			OUTPUT_CHANNEL(modname)(pmtd)

#define SEND_TO_MULTIPLE(modname, pmtd1, pmtd2) \
{						\
	PMetadata tmpmtd, head = pmtd1;		\
	while (head) {				\
		tmpmtd = head;			\
		head = head->next; 		\
		SEND_TO(modname, tmpmtd);	\
	}					\
}						\


#define	PDataChannel	PDataQueue
extern DataQueue gCH_table[NUM_CHANNEL];
extern EVENTHANDLER gEventDataTable[EVENT_MAX];

/* TODO avoid dpulicating functions between C1k/C2k Util */
static __inline PMetadata data_channel_get_event(U32 eventnum)
{
        PDataChannel pChannel = &gCH_table[eventnum];
        PMetadata pMetadata;

        /* get next metadata from list head */
        pMetadata = pChannel->head;
        if (pMetadata) {
                pChannel->head = pMetadata->next;
        }
        return pMetadata;
}

extern U32 gEventStatusReg;
static __inline void data_channel_put_event(U32 eventnum, struct tMetadata *pMetadata, BOOL do_set_event)
{

        PDataChannel pChannel = &gCH_table[eventnum];
	if (pktqueue_put(pChannel, pMetadata) && do_set_event)
                gEventStatusReg = 1;
}

static __inline int data_channel_isempty(U32 eventnum)
{
	return gCH_table[eventnum].head == NULL;
}

static __inline void drop_fragments(PMetadata mtd)
{
	PMetadata next;

	while (mtd)
	{
		next = mtd->next;
		DROP_PACKET(mtd, FRAGMENTER);
		mtd = next;
	}
}


#define	SEND_TO_CHANNEL(eventnum, pmtd)	data_channel_put_event(eventnum, pmtd, TRUE)

#define CHANNEL_GET(eventnum) data_channel_get_event(eventnum)
#define CHANNEL_ISEMPTY(eventnum)			data_channel_isempty(eventnum)

#elif defined(COMCERTO_2000)

#define CHANNEL_NULL	((channel_t)NULL)

#define OUTPUT_CHANNEL(modname)			M_##modname##_process_packet
#define	SEND_TO(modname, pmtd)			OUTPUT_CHANNEL(modname)(pmtd)
#define	SEND_TO_CHANNEL(modname, pmtd)		(*modname)(pmtd)

#define SEND_TO_MULTIPLE(modname, pmtd1, pmtd2) \
{						\
	PMetadata tmpmtd, head = pmtd1;		\
	while (head) {				\
		tmpmtd = head;			\
		head = head->next; 		\
		SEND_TO(modname, tmpmtd);	\
	}					\
}						\

#define SEND_TO_CHANNEL_MULTIPLE(modname, pmtd1, pmtd2) \
{						\
	PMetadata tmpmtd, head = pmtd1;		\
	while (head) {				\
		tmpmtd = head;			\
		head = head->next; 		\
		SEND_TO_CHANNEL(modname, tmpmtd);	\
	}					\
}						\

#endif


#endif /* _CHANNELS_H_ */

