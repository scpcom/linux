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


#include "modules.h"
#include "channels.h"
#include "events.h"
#include "module_tx.h"
#include "system.h"
#include "fpart.h"
#include "fpool.h"
#include "fpp.h"
#include "module_hidrv.h"

#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
U32 FCODE_TO_EVENT(U32 fcode)
{
	U32 eventid;
	switch((fcode & 0xFF00) >> 8)
	{
		case FC_RX:
			if (fcode >= L2BRIDGE_FIRST_COMMAND && fcode <= L2BRIDGE_LAST_COMMAND)
				eventid = EVENT_BRIDGE;
			else
				eventid = EVENT_PKT_RX;
			break;

		case FC_IPV4: eventid = EVENT_IPV4; break;

		case FC_IPV6: eventid = EVENT_IPV6; break;

		case FC_QM: eventid = EVENT_QM; break;

		case FC_TX: eventid = EVENT_PKT_TX; break;

		case FC_PPPOE: eventid = EVENT_PPPOE; break;

		case FC_MC: eventid = EVENT_MC6; break;

		case FC_RTP: eventid = EVENT_RTP_RELAY; break;

		case FC_VLAN: eventid = EVENT_VLAN; break;

		case FC_IPSEC: eventid = EVENT_IPS_IN; break;

		case FC_TRC: eventid = EVENT_IPS_OUT; break;

		case FC_TNL:eventid = EVENT_TNL_IN; break;

#ifdef CFG_MACVLAN
		case FC_MACVLAN: eventid = EVENT_MACVLAN; break;
#endif
#ifdef CFG_STATS
		case FC_STAT:eventid = EVENT_STAT; break;
#endif
		case FC_ALTCONF: eventid = EVENT_IPV4; break;

#ifdef CFG_WIFI_OFFLOAD
		case FC_WIFI_RX: eventid = EVENT_PKT_WIFIRX; break;
#endif

#ifdef CFG_NATPT
		case FC_NATPT: eventid = EVENT_NATPT; break;
#endif

#ifdef CFG_PCAP
		case FC_PKTCAP: eventid = EVENT_PKTCAP; break;
#endif

#ifdef CFG_DIAGS
		case FC_FPPDIAG: eventid = EVENT_IPV4; break;
#endif
#if defined(COMCERTO_2000_CONTROL) && defined(CFG_ICC)
		case FC_ICC: eventid = EVENT_ICC; break;
#endif
		case FC_L2TP: eventid = EVENT_L2TP; break;
		default: eventid = 0; break;
	}

	return eventid;
}
#endif /* !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL) */

#if !defined(COMCERTO_2000)

BOOL M_hidrv_init(PModuleDesc pModule)
{
	
	/* module entry point and channels registration */
	pModule->entry = &M_hidrv_entry;
	pModule->cmdproc = NULL;
	HAL_arm1_fiq_enable_1(IRQM_FROMHOST);
	host_cmd.code = 0xFFFF;
	return 0;
	
}

void msg_send(HostMessage *pmsg)
{
	DISABLE_INTERRUPTS();

	pmsg->next = NULL;
	if (hostmsg_queue.head) {
		HostMessage *ptail = hostmsg_queue.tail;
		ptail->next = pmsg;
		hostmsg_queue.tail = pmsg;
	}
	else {
		hostmsg_queue.tail = hostmsg_queue.head = pmsg;
		set_event(gEventStatusReg, 1 << EVENT_HIDRV);
	}

	ENABLE_INTERRUPTS();
}


void M_hidrv_entry(void)
{
	U32	   tmp;

	// process host command
	if (host_cmd.code != 0xFFFF)
	{
		CmdProc cmdproc;
		U16 retlen;
		U32 eventid;
		eventid = FCODE_TO_EVENT(host_cmd.code);
		cmdproc = gCmdProcTable[eventid];
		if (cmdproc)
		{
			retlen = (*cmdproc)(host_cmd.code, host_cmd.length, host_cmd.data);
			if (retlen == 0)
			{
				host_cmd.data[0] = NO_ERR;
				retlen = 2;
			}
		}
		else
		{
			host_cmd.data[0] = ERR_UNKNOWN_COMMAND;
			retlen = 2;
		}
		tmp = (U32)retlen | (((U32)host_cmd.code) << 16);
		*(V32*)CMD_MBOX_1_ADDR = tmp;
		SFL_memcpy((U32*)CMD_DATA_ADDR, host_cmd.data, retlen);
		*(V32*)CMD_MBOX_0_ADDR |= M0_ACK;

		DISABLE_INTERRUPTS();
		HAL_arm1_fiq_enable_1(IRQM_FROMHOST);
		ENABLE_INTERRUPTS();

		host_cmd.code = 0xFFFF;		// mark no host command
	}

	// process host messages
	DISABLE_INTERRUPTS();
	while (hostmsg_queue.head)
	{
		HostMessage *pmsg;
		if ( *(V32*)EVENT_MBOX_0_ADDR & M0_EVENT) {
			// SMI is busy (pending event in SMI)
			// enable interrupt raised when available
			HAL_arm1_fiq_enable_1(IRQM_TOHOST);
			break;
		}
		else {
			pmsg = hostmsg_queue.head;
		   	tmp = (U32)pmsg->length | ( ((U32)pmsg->code) << 16);
		   	*(V32*)EVENT_MBOX_1_ADDR = tmp;
		    	SFL_memcpy((U32*)EVENT_DATA_ADDR, pmsg->data, pmsg->length);
			HAL_clear_interrupt_1(IRQM_TOHOST);
			*(V32*)EVENT_MBOX_0_ADDR |= M0_EVENT;
			HAL_generate_int_1(IRQM_CSP_HIDRV);
			hostmsg_queue.head = pmsg->next;
			msg_free(pmsg);
		}
	}
	ENABLE_INTERRUPTS();
}


HostMessage *msg_alloc(void)
{
	return SFL_alloc_part_lock(&HostmsgPart);
}

void msg_free(HostMessage *msg)
{
	SFL_free_part(&HostmsgPart, msg);
}


#endif
