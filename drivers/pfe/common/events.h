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


#ifndef _EVENTS_H_
#define _EVENTS_H_

#include "config.h"

/* Event IDs */

#if defined(COMCERTO_2000_UTIL) || defined(COMCERTO_2000_CLASS)
enum EVENTS {
		EVENT_FIRST = 0,
		EVENT_IPS_IN = EVENT_FIRST,
		EVENT_IPS_OUT,
		EVENT_RTP_RELAY,
		EVENT_RTP_QOS,
		EVENT_IPS_IN_CB,
		EVENT_IPS_OUT_CB,
		EVENT_FRAG6,
		EVENT_FRAG4,
		EVENT_MAX
};
#elif !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)

enum EVENTS {
	EVENT_FIRST = 0,
	EVENT_EXPT = EVENT_FIRST,
	EVENT_QM,
	EVENT_PKT_TX,
	EVENT_TIMER,
	EVENT_PKT_RX,
#ifdef CFG_WIFI_OFFLOAD
	EVENT_PKT_WIFIRX,
#endif
	EVENT_MC6,
	EVENT_MC4,
	EVENT_BRIDGE,
	EVENT_VLAN,
#ifdef CFG_MACVLAN
	EVENT_MACVLAN,
#endif
	EVENT_PPPOE,
	EVENT_IPV4,
	EVENT_IPV6,
	EVENT_IPS_IN,
	EVENT_IPS_OUT,
	EVENT_IPS_IN_CB,
	EVENT_IPS_OUT_CB,
	EVENT_TNL_IN,
	EVENT_TNL_OUT,
#ifdef CFG_STATS
	EVENT_STAT,
#endif	
	EVENT_FRAG6,
	EVENT_FRAG4,
	EVENT_RTP_RELAY,
#ifdef COMCERTO_1000
	EVENT_MSP,
#endif
#ifdef CFG_NATPT
	EVENT_NATPT,
#endif
#ifdef CFG_PCAP
	EVENT_PKTCAP,
#endif
#if defined(COMCERTO_2000_CONTROL) && defined(CFG_ICC)
	EVENT_ICC,
#endif
	EVENT_L2TP,
	EVENT_HIDRV,
	EVENT_MAX
};
#endif /* !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL) */

#if !defined(COMCERTO_2000)

/* Events management functions*/
#define set_event(reg, mask) (reg |= (mask))
#define clear_event(reg, mask) (reg &= ~(mask))
#endif

#define set_event_handler(event, handler)	gEventDataTable[event] = (handler)



#endif /* _EVENTS_H_ */
