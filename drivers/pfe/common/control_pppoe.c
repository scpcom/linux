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
#include "module_ethernet.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_pppoe.h"
#include "module_hidrv.h"
#include "module_timer.h"
#include "system.h"
#include "gemac.h"
#include "fpp.h"
#include "layer2.h"
#include "module_qm.h"
#include "module_wifi.h"

int PPPoE_Get_Next_SessionEntry(pPPPoECommand pSessionCmd, int reset_action);

#if defined(COMCERTO_2000)

static pPPPoE_Info pppoe_alloc(void)
{
	int i;

	for (i = 0; i < PPPOE_MAX_ITF; i++)
		if (!pppoe_itf[i].itf.type)
			return &pppoe_itf[i];

	return NULL;
}


static void pppoe_free(pPPPoE_Info pEntry)
{
	pEntry->itf.type = 0;
}


static void __pppoe_add(pPPPoE_Info pEntry, U16 hash_key)
{
	int id;
	PPPoE_Info pppoe;

	/* Add to our local hash */
	slist_add(&pppoe_cache[hash_key], &pEntry->list);

	/* Construct the hardware entry, converting virtual addresses and endianess where needed */
	memcpy(&pppoe, pEntry, sizeof(PPPoE_Info));
	pppoe.itf.phys = (void *)cpu_to_be32(virt_to_class(pEntry->itf.phys));
	slist_set_next(&pppoe.list, (void *)cpu_to_be32(virt_to_class_dmem(slist_next(&pEntry->list))));
	pppoe.relay = (void *)cpu_to_be32(virt_to_class_dmem(pEntry->relay));
	pppoe.ppp_flags = cpu_to_be32(pEntry->ppp_flags);

	/* Update the PE pppoe interface in DMEM, for each PE */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_memcpy_to32(id, virt_to_class_dmem(pEntry), &pppoe, sizeof(PPPoE_Info));

	/* Now add the interface to the PE pppoe hash in DMEM, for each PE and atomically */

	/* We use the fact that slist_add() always adds to the head of the list */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_write(id, cpu_to_be32(virt_to_class_dmem(&pEntry->list)), virt_to_class_dmem(&pppoe_cache[hash_key]), sizeof(struct slist_entry *));

}

static void pppoe_add(pPPPoE_Info pEntry, U16 hash_key)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	pe_sync_stop(ctrl, CLASS_MASK);
	__pppoe_add(pEntry, hash_key);
	pe_start(ctrl, CLASS_MASK);
}

static void pppoe_add_relay(pPPPoE_Info pEntry, U16 hash_key, pPPPoE_Info pRelayEntry, U16 relay_hash_key)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	pe_sync_stop(ctrl, CLASS_MASK);
	__pppoe_add(pEntry, hash_key);
	__pppoe_add(pRelayEntry, relay_hash_key);
	pe_start(ctrl, CLASS_MASK);
}

static void __pppoe_remove(pPPPoE_Info pEntry, U16 hash_key)
{
	struct slist_entry *prev;
	int id;

	/* Now remove the interface from the PE pppoe hash in DMEM, for each PE and atomically */

	prev = slist_prev(&pppoe_cache[hash_key], &pEntry->list);

	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_write(id, cpu_to_be32(virt_to_class_dmem(slist_next(&pEntry->list))), virt_to_class_dmem(prev), sizeof(struct slist_entry *));


	/* Remove from our local hash */
	slist_remove_after(prev);
}

static void pppoe_remove(pPPPoE_Info pEntry, U16 hash_key)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	pe_sync_stop(ctrl, CLASS_MASK);
	__pppoe_remove(pEntry, hash_key);
	pe_start(ctrl, CLASS_MASK);
}

static void pppoe_remove_relay(pPPPoE_Info pEntry, U16 hash_key, pPPPoE_Info pRelayEntry, U16 relay_hash_key)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	pe_sync_stop(ctrl, CLASS_MASK);
	__pppoe_remove(pEntry, hash_key);
	__pppoe_remove(pRelayEntry, relay_hash_key);
	pe_start(ctrl, CLASS_MASK);
}

void M_pppoe_encapsulate(PMetadata mtd, pPPPoE_Info ppp_info, U16 ethertype, U8 update)
{

}

#else

static pPPPoE_Info pppoe_alloc(void)
{
	return Heap_Alloc_ARAM(sizeof (PPPoE_Info));
}

static void pppoe_free(pPPPoE_Info pEntry)
{
	Heap_Free(pEntry);
}

static void pppoe_add(pPPPoE_Info pEntry, U16 hash_key)
{
	slist_add(&pppoe_cache[hash_key], &pEntry->list);
}

static void pppoe_add_relay(pPPPoE_Info pEntry, U16 hash_key, pPPPoE_Info pRelayEntry, U16 relay_hash_key)
{
	pppoe_add(pEntry, hash_key);
	pppoe_add(pRelayEntry, relay_hash_key);
}

static void pppoe_remove(pPPPoE_Info pEntry, U16 hash_key)
{
	slist_remove(&pppoe_cache[hash_key], &pEntry->list);
}

static void pppoe_remove_relay(pPPPoE_Info pEntry, U16 hash_key, pPPPoE_Info pRelayEntry, U16 relay_hash_key)
{
	pppoe_remove(pEntry, hash_key)
	pppoe_remove(pRelayEntry, relay_hash_key)
}


#endif

#if defined(COMCERTO_2000)

extern TIMER_ENTRY pppoe_timer;

static void M_pppoe_timer(void)
{
	pPPPoE_Info pEntry;
	struct slist_entry *entry;
	int hash_index;
	int id;
	BOOL entry_found = FALSE;
	for (hash_index = 0; hash_index < NUM_PPPOE_ENTRIES; hash_index++)
	{
		slist_for_each(pEntry, entry, &pppoe_cache[hash_index], list)
		{
			entry_found = TRUE;
			for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
			{
				if (pe_dmem_read(id, virt_to_class_dmem(&pEntry->last_pkt_rcvd), sizeof(pEntry->last_pkt_rcvd)))
				{
					pe_dmem_write(id, 0, virt_to_class_dmem(&pEntry->last_pkt_rcvd), sizeof(pEntry->last_pkt_rcvd));
					pEntry->last_pkt_rcvd = jiffies;
				}
				if (pe_dmem_read(id, virt_to_class_dmem(&pEntry->last_pkt_xmit), sizeof(pEntry->last_pkt_xmit)))
				{
					pe_dmem_write(id, 0, virt_to_class_dmem(&pEntry->last_pkt_xmit), sizeof(pEntry->last_pkt_xmit));
					pEntry->last_pkt_xmit = jiffies;
				}
			}
		}
	}
	if (!entry_found)
		timer_del(&pppoe_timer);
}
#endif

static int PPPoE_Handle_Get_Idle(U16* p, U16 Length)
{
	pPPPoEIdleTimeCmd cmd;
	POnifDesc ppp_if;
	pPPPoE_Info pppoeinfo;

	cmd = (pPPPoEIdleTimeCmd)p;
	if (Length != sizeof(PPPoEIdleTimeCmd)) 
		return ERR_WRONG_COMMAND_SIZE;

	ppp_if = get_onif_by_name(cmd->ppp_intf);
	if (!ppp_if || !ppp_if->itf || !(ppp_if->itf->type & IF_TYPE_PPPOE))
		return ERR_UNKNOWN_INTERFACE;
	
	pppoeinfo = (pPPPoE_Info)ppp_if->itf;

	cmd = (pPPPoEIdleTimeCmd)(p+1);
	SFL_memcpy(cmd->ppp_intf,ppp_if->name,sizeof(ppp_if->name));
#if defined(COMCERTO_2000)
	cmd->recv_idle = (jiffies - pppoeinfo->last_pkt_rcvd) / HZ;
	cmd->xmit_idle = (jiffies - pppoeinfo->last_pkt_xmit) / HZ;
#else
	cmd->recv_idle = (ct_timer - pppoeinfo->last_pkt_rcvd) / CT_TICKS_PER_SECOND;
	cmd->xmit_idle = (ct_timer - pppoeinfo->last_pkt_xmit) / CT_TICKS_PER_SECOND;
#endif
	return NO_ERR;
}


static int PPPoE_Handle_Relay_Entry(U16 *p, U16 Length)
{
	pPPPoERelayCommand cmd;
	pPPPoE_Info pEntry;
	pPPPoE_Info pRelayEntry;
	POnifDesc onif,relayonif;
	U16 hash_key,relay_hash_key;
	struct slist_entry *entry;

	cmd = (pPPPoERelayCommand) p;
	if (Length != sizeof(PPPoERelayCommand))
		return ERR_WRONG_COMMAND_SIZE;

	cmd->sesID = htons(cmd->sesID);
	cmd->relaysesID = htons(cmd->relaysesID);

	hash_key = HASH_PPPOE(cmd->sesID, cmd->peermac1);
	relay_hash_key = HASH_PPPOE(cmd->relaysesID, cmd->peermac2);

	switch (cmd->action)
	{
	case ACTION_REGISTER:

		slist_for_each(pEntry, entry, &pppoe_cache[hash_key], list)
		{
			 if((pEntry->sessionID == cmd->sesID) && TESTEQ_MACADDR(pEntry->DstMAC, cmd->peermac1))
				return ERR_PPPOE_ENTRY_ALREADY_REGISTERED; //trying to add the same pppoe session
		}

		slist_for_each(pRelayEntry, entry, &pppoe_cache[relay_hash_key], list)
		{
			if((pRelayEntry->sessionID == cmd->relaysesID) && TESTEQ_MACADDR(pRelayEntry->DstMAC, cmd->peermac2))
				return ERR_PPPOE_ENTRY_ALREADY_REGISTERED; //trying to add the same pppoe session
		}

		if ((pEntry = pppoe_alloc()) == NULL)
		{
			return ERR_NOT_ENOUGH_MEMORY;
		}
		memset(pEntry, 0, sizeof (PPPoE_Info));
		pEntry->itf.type = IF_TYPE_PPPOE;

		if ((pRelayEntry = pppoe_alloc()) == NULL)
		{
			pppoe_free(pEntry);
			return ERR_CREATION_FAILED;
		}
		memset(pRelayEntry, 0, sizeof (PPPoE_Info));
		pRelayEntry->itf.type = IF_TYPE_PPPOE;

		/* populate relay pairs */
		pEntry->sessionID = cmd->sesID;
		COPY_MACADDR(pEntry->DstMAC,cmd->peermac1);

		pRelayEntry->sessionID = cmd->relaysesID;
		COPY_MACADDR(pRelayEntry->DstMAC,cmd->peermac2);

		/*Check if the Physical interface is known by the Interface manager*/
		onif = get_onif_by_name(cmd->ipifname);
		relayonif = get_onif_by_name(cmd->opifname);

		if ((!onif) || (!relayonif)) {
			pppoe_free(pEntry);
			pppoe_free(pRelayEntry);
			return ERR_UNKNOWN_INTERFACE;
		}

#if defined(COMCERTO_2000)
		pEntry->last_pkt_rcvd = jiffies;
		pEntry->last_pkt_xmit = jiffies;
		pRelayEntry->last_pkt_rcvd = jiffies;
		pRelayEntry->last_pkt_xmit = jiffies;
		if (!pppoe_timer.running)
			timer_add(&pppoe_timer, TIMER_TICKS_PER_SEC);
#else
		pEntry->last_pkt_rcvd = ct_timer;
		pEntry->last_pkt_xmit = ct_timer;
		pRelayEntry->last_pkt_rcvd = ct_timer;
		pRelayEntry->last_pkt_xmit = ct_timer;
#endif

		pEntry->itf.phys = onif->itf;
		pRelayEntry->itf.phys = relayonif->itf;

		/*Now link these two entries by relay ptr */
		pEntry->relay = pRelayEntry;
		pRelayEntry->relay = pEntry;

		pppoe_add_relay(pEntry, hash_key, pRelayEntry, relay_hash_key);

		PPPoE_entries++;

#ifdef CFG_STATS
		gStatPPPoEQueryStatus = STAT_PPPOE_QUERY_NOT_READY;
#endif
		break;

	case ACTION_DEREGISTER:
		slist_for_each(pEntry, entry, &pppoe_cache[hash_key], list)
		{
			if (pEntry->relay)
			{
				if((pEntry->sessionID == cmd->sesID) && TESTEQ_MACADDR(pEntry->DstMAC, cmd->peermac1)
				&& (pEntry->relay->sessionID == cmd->relaysesID) &&
				TESTEQ_MACADDR(pEntry->relay->DstMAC, cmd->peermac2))
					goto found;

			}
	        }

		return ERR_PPPOE_ENTRY_NOT_FOUND;

	found:
		/* Now relay part as we already searched for relay link, just check for peer2mac and relaysesID */

		slist_for_each(pRelayEntry, entry, &pppoe_cache[relay_hash_key], list)
		{
			if((pRelayEntry->sessionID == cmd->relaysesID) &&
			     (TESTEQ_MACADDR(pRelayEntry->DstMAC, cmd->peermac2)) &&
			     (pRelayEntry->relay == pEntry))
				goto found_relay;
		}

		return ERR_PPPOE_ENTRY_NOT_FOUND;

	found_relay:
		pppoe_remove_relay(pEntry, hash_key, pRelayEntry, relay_hash_key);

		pppoe_free(pEntry);
		pppoe_free(pRelayEntry);

		PPPoE_entries--;

#ifdef CFG_STATS
		gStatPPPoEQueryStatus = STAT_PPPOE_QUERY_NOT_READY;
#endif

		 break;

	default :
		return ERR_UNKNOWN_COMMAND;
	}

	return NO_ERR;
}

static int PPPoE_Handle_Entry(U16 *p, U16 Length)
{
	pPPPoECommand cmd;
	pPPPoE_Info pEntry;
	POnifDesc phys_onif;
	U32 hash_key;
	struct slist_entry *entry;

	cmd = (pPPPoECommand) p;
	if (Length != sizeof(PPPoECommand))
		return ERR_WRONG_COMMAND_SIZE;

	cmd->sessionID = htons(cmd->sessionID);

	hash_key = HASH_PPPOE(cmd->sessionID, cmd->macAddr);

	switch (cmd->action)
	{
	case ACTION_DEREGISTER:
		slist_for_each(pEntry, entry, &pppoe_cache[hash_key], list)
		{
			if ((pEntry->sessionID == cmd->sessionID) && TESTEQ_MACADDR(pEntry->DstMAC, cmd->macAddr) &&
			    (pEntry->relay == NULL)  && !strcmp(get_onif_name(pEntry->itf.index), (char *)cmd->log_intf) )
				goto found;
		}

		return ERR_PPPOE_ENTRY_NOT_FOUND;

	found:
		pppoe_remove(pEntry, hash_key);

		/*Tell the Interface Manager to remove the pppoe IF*/
		remove_onif_by_index(pEntry->itf.index);

		pppoe_free(pEntry);

		PPPoE_entries--;

#ifdef CFG_STATS
		gStatPPPoEQueryStatus = STAT_PPPOE_QUERY_NOT_READY;
#endif
		break;

	case ACTION_REGISTER:

		if (get_onif_by_name(cmd->log_intf))
			return ERR_PPPOE_ENTRY_ALREADY_REGISTERED;

		/*Check if the Physical interface is known by the Interface manager*/
		phys_onif = get_onif_by_name(cmd->phy_intf);
		if (!phys_onif)
			return ERR_UNKNOWN_INTERFACE;

		slist_for_each(pEntry, entry, &pppoe_cache[hash_key], list)
		{
			if ((pEntry->sessionID == cmd->sessionID) && TESTEQ_MACADDR(pEntry->DstMAC, cmd->macAddr))
				return ERR_PPPOE_ENTRY_ALREADY_REGISTERED; //trying to add exactly the same vlan entry
		}

		if ((pEntry = pppoe_alloc()) == NULL)
		{
			return ERR_NOT_ENOUGH_MEMORY;
		}

		memset(pEntry, 0, sizeof (PPPoE_Info));

		/* populate pppoe_info entry */
		pEntry->sessionID = cmd->sessionID;
		COPY_MACADDR(pEntry->DstMAC,cmd->macAddr);

#if defined(COMCERTO_2000)
		pEntry->last_pkt_rcvd = jiffies;
		pEntry->last_pkt_xmit = jiffies;
		if (!pppoe_timer.running)
			timer_add(&pppoe_timer, TIMER_TICKS_PER_SEC);
#else
		pEntry->last_pkt_rcvd = ct_timer;
		pEntry->last_pkt_xmit = ct_timer;
#endif

		if (cmd->mode & PPPOE_AUTO_MODE)
			pEntry->ppp_flags |= PPPOE_AUTO_MODE;

		/*Now create a new interface in the Interface Manager and remember the index*/
		if (!add_onif(cmd->log_intf, &pEntry->itf, phys_onif->itf, IF_TYPE_PPPOE))
		{
			pppoe_free(pEntry);
			return ERR_CREATION_FAILED;
		}

		pppoe_add(pEntry, hash_key);

		PPPoE_entries++;

#ifdef CFG_STATS
		gStatPPPoEQueryStatus = STAT_PPPOE_QUERY_NOT_READY;
#endif
		break;

	case ACTION_QUERY:
	case ACTION_QUERY_CONT:
		{
		int rc;

		rc = PPPoE_Get_Next_SessionEntry(cmd, cmd->action == ACTION_QUERY);
		return rc;

		}

	default:
		return ERR_UNKNOWN_ACTION;
	}

	/* return success */
	return NO_ERR;
}


static U16 M_pppoe_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc = NO_ERR;
	U16 ret_len = 2;
	U16 action;

        switch (cmd_code)
	{
		case CMD_PPPOE_ENTRY:
			action = *pcmd;
			rc = PPPoE_Handle_Entry(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				ret_len = sizeof(PPPoECommand);
			break;

		case CMD_PPPOE_RELAY_ENTRY:
			action = *pcmd;
			rc = PPPoE_Handle_Relay_Entry(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				ret_len = sizeof(PPPoECommand);
			break;

		case CMD_PPPOE_GET_IDLE:
			rc = PPPoE_Handle_Get_Idle(pcmd, cmd_len);
			if (rc == NO_ERR)
				ret_len = sizeof(PPPoEIdleTimeCmd) + 2;
			break;	

		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
        }

	*pcmd = rc;
	return ret_len;
}


int pppoe_init(void)
{
	int i;

#if !defined(COMCERTO_2000)
	set_event_handler(EVENT_PPPOE, M_pppoe_ingress);
#endif

	set_cmd_handler(EVENT_PPPOE, M_pppoe_cmdproc);

	for (i = 0; i < NUM_PPPOE_ENTRIES; i++)
	{
		slist_head_init(&pppoe_cache[i]);
	}

#if defined(COMCERTO_2000)
	timer_init(&pppoe_timer, M_pppoe_timer);
#endif

	return 0;
}

void pppoe_exit(void)
{
	/* FIXME remove all pppoe interfaces */
#if defined(COMCERTO_2000)
	timer_del(&pppoe_timer);
#endif
}
