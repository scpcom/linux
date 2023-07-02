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


#include "Module_ipv4frag.h"
#include "Module_ipv6frag.h"
#include "fe.h"

#if defined(COMCERTO_2000_UTIL)
static TIMER_ENTRY ipv4_frag_timer;
#endif

#if defined(COMCERTO_2000_CONTROL) || !defined(COMCERTO_2000)
extern struct ip_frag_ctrl ipv4_frag_ctrl;
int IPv4_HandleIP_Set_FragTimeout(U16 *p, U16 Length, U16 sam)
{
	FragTimeoutCommand FragCmd;
	int rc = NO_ERR;

	// Check length
	if (Length != sizeof(FragTimeoutCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&FragCmd, (U8*)p, sizeof(FragTimeoutCommand));

#if !defined(COMCERTO_2000)
	if(sam) {
		ipv4_samfrag_timeout =   FragCmd.timeout;
	}else {
		ipv4_frag_timeout = FragCmd.timeout; // in ms
		ipv4_frag_expirydrop = FragCmd.mode;
	}
#else
	if(sam) {
		ipv4_frag_ctrl.sam_timeout =   cpu_to_be16(FragCmd.timeout);
	}else {
		ipv4_frag_ctrl.timeout = cpu_to_be16(FragCmd.timeout);
		ipv4_frag_ctrl.expire_drop = cpu_to_be16(FragCmd.mode);
	}

	pe_dmem_memcpy_to32(UTIL_ID, virt_to_util_dmem(&ipv4_frag_ctrl), &ipv4_frag_ctrl, sizeof(ipv4_frag_ctrl));
#endif

	return rc;
}
#else

void ipv4_frag_init(void)
{

	SFL_defpart(&frag4Part, (void*)Frag4Storage, sizeof(FragIP4), NUM_FRAG4_Q);

	/* module entry point and channels registration */
	set_event_handler(EVENT_FRAG4, M_ipv4_frag_entry);
#if !defined(COMCERTO_2000)
	set_cmd_handler(EVENT_FRAG4, NULL);
#endif

	timer_init(&ipv4_frag_timer, M_ipv4_frag_timer);
	timer_add(&ipv4_frag_timer, IP_FRAG_TIMER_INTERVAL);
}
#endif
