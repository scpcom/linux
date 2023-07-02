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


#ifndef _MODULE_TIMER_H_
#define _MODULE_TIMER_H_

#if !defined (COMCERTO_2000)
#include "modules.h"
#endif

#if !defined (COMCERTO_2000_CONTROL)
#include "timer.h"
typedef void (*TIMER_HANDLER)(void);
/** Timer
*
*/
typedef struct {
	struct slist_entry list;
	TIMER_HANDLER handler;
	U16 tick;
	U16 granularity;
} TIMER_ENTRY;

struct tTIMER_context {
	struct slist_head timers;
	U32 count;
};

extern struct tTIMER_context gTimerCtx;

BOOL sw_timer_init(void);

/** Run programmed periodic tasks.
 * If a timer tick has elapsed, loops through all configured timer entries,
 * and executes callback for timers that have expired.
 * Called in the Util PE main loop.
 */
void M_timer_entry(void);

/** Initializes a timer structure.
* Must be called once for each TIMER_ENTRY structure.
*
* @param timer		pointer to the timer to be initialized
* @param handler	timer handler function pointer
*
*/
void timer_init(TIMER_ENTRY *timer, TIMER_HANDLER handler);


/** Adds a timer to the running timer list.
* It's safe to call even if the timer was already running. In this case we just update the granularity.
*
* @param timer		pointer to the timer to be added
* @param granularity	granularity of the timer (in timer tick units)
*
*/
void timer_add(TIMER_ENTRY *timer, U16 granularity) __attribute__ ((noinline));


/** Deletes a timer from the running timer list.
* It's safe to call even if the timer is no longer running.
*
* @param timer	pointer to the timer to be removed
*/
void timer_del(TIMER_ENTRY *timer) __attribute__ ((noinline));

/* Resets the timer tick to 0 */
void timer_reset(TIMER_ENTRY *timer);
#endif

#endif /* _MODULE_TIMER_H_ */
