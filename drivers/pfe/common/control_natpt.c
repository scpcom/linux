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

#include "module_natpt.h"
#include "modules.h"
#include "events.h"
#include "checksum.h"
#include "module_ethernet.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_hidrv.h"
#include "system.h"
#include "fpp.h"
#include "layer2.h"
#include "module_stat.h"
#include "module_qm.h"
#include "control_socket.h"

#ifdef CFG_NATPT

#if defined(COMCERTO_2000)
#include "control_common.h"

static struct dma_pool *natpt_dma_pool;

static TIMER_ENTRY natpt_delayed_remove_timer;

static struct dlist_head hw_natpt_removal_list;

static struct slist_head natpt_cache[NUM_NATPT_ENTRIES] __attribute__((aligned(32)));

#endif


#if !defined(COMCERTO_2000)
static int natpt_add(PNATPT_Entry pEntry, U32 hash)
{
	/* Add software entry to local hash */
	slist_add(&natpt_cache[hash], &pEntry->list);

	SOCKET_bind(pEntry->socketA, pEntry, SOCK_OWNER_NATPT);
	SOCKET_bind(pEntry->socketB, pEntry, SOCK_OWNER_NATPT);

	return NO_ERR;
}

static void natpt_remove(PNATPT_entry pEntry, U32 hash)
{
	SOCKET_unbind(pEntry->socketA);
	SOCKET_unbind(pEntry->socketB);

	slist_remove(&natpt_cache[hash], &pEntry->list);
}

#else	// defined(COMCERTO_2000

static void hw_natpt_schedule_remove(struct _thw_natpt *hw_natpt)
{
	hw_natpt->removal_time = jiffies + 2;
	dlist_add(&hw_natpt_removal_list, &hw_natpt->list);
}

/** Processes hardware natpt delayed removal list.
* Free all hardware natpt entries in the removal list that have reached their removal time.
*
*
*/
static void hw_natpt_delayed_remove(void)
{
	struct dlist_head *entry;
	struct _thw_natpt *hw_natpt;

	dlist_for_each_safe(hw_natpt, entry, &hw_natpt_removal_list, list)
	{
		if (!time_after(jiffies, hw_natpt->removal_time))
			continue;

		dlist_remove(&hw_natpt->list);

		dma_pool_free(natpt_dma_pool, hw_natpt, be32_to_cpu(hw_natpt->dma_addr));
	}
}

static int natpt_add(PNATPT_Entry pEntry, U32 hash)
{
	struct _thw_natpt *hw_natpt;
	int rc;
	dma_addr_t dma_addr;

	/* Allocate hardware entry */
	hw_natpt = dma_pool_alloc(natpt_dma_pool, GFP_ATOMIC, &dma_addr);
	if (!hw_natpt)
	{
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err;
	}

	memset(hw_natpt, 0, sizeof(struct _thw_natpt));
	hw_natpt->dma_addr = cpu_to_be32(dma_addr);

	/* Link software conntrack to hardware conntrack */
	pEntry->hw_natpt = hw_natpt;
	hw_natpt->sw_natpt = pEntry;

	pEntry->hash = hash;

	/* fill in the hw entry */
	hw_natpt->socketA = cpu_to_be16(pEntry->socketA);
	hw_natpt->socketB = cpu_to_be16(pEntry->socketB);
	hw_natpt->control = cpu_to_be16(pEntry->control);
	hw_natpt->protocol = pEntry->protocol;

	/* There is no hardware hash table needed for NAT-PT */

	/* Add software entry to local hash */
	slist_add(&natpt_cache[hash], &pEntry->list);

	SOCKET_bind(pEntry->socketA, (PVOID)hw_natpt->dma_addr, SOCK_OWNER_NATPT);
	SOCKET_bind(pEntry->socketB, (PVOID)hw_natpt->dma_addr, SOCK_OWNER_NATPT);

	return NO_ERR;

err:
	return rc;
}

static void natpt_remove(PNATPT_Entry pEntry, U32 hash)
{
	struct _thw_natpt *hw_natpt;

	SOCKET_unbind(pEntry->socketA);
	SOCKET_unbind(pEntry->socketB);

	/* Check if there is a hardware flow */
	if ((hw_natpt = pEntry->hw_natpt))
	{
		/* Detach from software entry */
		pEntry->hw_natpt = NULL;

		hw_natpt_schedule_remove(hw_natpt);
	}

	slist_remove(&natpt_cache[hash], &pEntry->list);
}

void natpt_update_stats(PNATPT_Entry pEntry)
{
	int id;
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct _thw_natpt *hw_natpt;

	pEntry->stat_v6_received = 0;
	pEntry->stat_v6_transmitted = 0;
	pEntry->stat_v6_dropped = 0;
	pEntry->stat_v6_sent_to_ACP = 0;
	pEntry->stat_v4_received = 0;
	pEntry->stat_v4_transmitted = 0;
	pEntry->stat_v4_dropped = 0;
	pEntry->stat_v4_sent_to_ACP = 0;

	hw_natpt = pEntry->hw_natpt;
	if (!hw_natpt)
		return;

	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
	{
		pe_sync_stop(ctrl, (1 << id));
		pEntry->stat_v6_received += be64_to_cpu(hw_natpt->stats[id].stat_v6_received);
		pEntry->stat_v6_transmitted += be64_to_cpu(hw_natpt->stats[id].stat_v6_transmitted);
		pEntry->stat_v6_dropped += be64_to_cpu(hw_natpt->stats[id].stat_v6_dropped);
		pEntry->stat_v6_sent_to_ACP += be64_to_cpu(hw_natpt->stats[id].stat_v6_sent_to_ACP);
		pEntry->stat_v4_received += be64_to_cpu(hw_natpt->stats[id].stat_v4_received);
		pEntry->stat_v4_transmitted += be64_to_cpu(hw_natpt->stats[id].stat_v4_transmitted);
		pEntry->stat_v4_dropped += be64_to_cpu(hw_natpt->stats[id].stat_v4_dropped);
		pEntry->stat_v4_sent_to_ACP += be64_to_cpu(hw_natpt->stats[id].stat_v4_sent_to_ACP);
		pe_start(ctrl, (1 << id));
	}
}

#endif	// !defined(COMCERTO_2000)

static PNATPT_Entry NATPT_find(U16 socketA, U16 socketB)
{
	U32 hash;
	struct slist_entry *entry;
	PNATPT_Entry pEntry;

	hash = HASH_NATPT(socketA, socketB);
	slist_for_each(pEntry, entry, &natpt_cache[hash], list)
	{
		if (pEntry->socketA == socketA && pEntry->socketB == socketB)
			return pEntry;
	}
	return NULL;
}


static int NATPT_Open(U16 *p, U16 Length)
{
	NATPTOpenCommand OpenCmd;
	PSockEntry pSocketA;
	PSockEntry pSocketB;
	PNATPT_Entry pEntry;
	U32 hash;

	// Check length
	if (Length != sizeof(NATPTOpenCommand))
		return ERR_WRONG_COMMAND_SIZE;
	// Ensure alignment
	SFL_memcpy((U8*)&OpenCmd, (U8*)p, sizeof(NATPTOpenCommand));

	// Make sure sockets exist but are unused
	pSocketA = SOCKET_find_entry_by_id(OpenCmd.socketA);
	pSocketB = SOCKET_find_entry_by_id(OpenCmd.socketB);
	if (!pSocketA || !pSocketB)
		return ERR_SOCKID_UNKNOWN;
	if (pSocketA->owner || pSocketB->owner)
		return ERR_SOCK_ALREADY_IN_USE;

	// Socket A must be IPv6, and Socket B must be IPv4
	if (pSocketA->SocketFamily != PROTO_IPV6 || pSocketB->SocketFamily != PROTO_IPV4)
		return ERR_WRONG_SOCK_FAMILY;

	pEntry = Heap_Alloc(sizeof(NATPT_Entry));
	if (!pEntry)
		return ERR_NOT_ENOUGH_MEMORY;

	memset(pEntry, 0, sizeof(NATPT_Entry));

	pEntry->socketA = OpenCmd.socketA;
	pEntry->socketB = OpenCmd.socketB;
	pEntry->control = OpenCmd.control;

	hash = HASH_NATPT(OpenCmd.socketA, OpenCmd.socketB);
	natpt_add(pEntry, hash);

	return NO_ERR;
}


static int NATPT_Close(U16 *p, U16 Length)
{
	NATPTCloseCommand CloseCmd;
	PNATPT_Entry pEntry;

	// Check length
	if (Length != sizeof(NATPTCloseCommand))
		return ERR_WRONG_COMMAND_SIZE;
	// Ensure alignment
	SFL_memcpy((U8*)&CloseCmd, (U8*)p, sizeof(NATPTCloseCommand));

	pEntry = NATPT_find(CloseCmd.socketA, CloseCmd.socketB);
	if (!pEntry)
		return ERR_NATPT_UNKNOWN_CONNECTION;

	natpt_remove(pEntry, pEntry->hash);
	Heap_Free((PVOID)pEntry);

	return NO_ERR;
}


static int NATPT_Query(U16 *p, U16 Length)
{
	NATPTQueryCommand QueryCmd;
	PNATPTQueryResponse pResp;
	PNATPT_Entry pEntry;

	// Check length
	if (Length != sizeof(NATPTQueryCommand))
		return ERR_WRONG_COMMAND_SIZE;
	// Ensure alignment
	SFL_memcpy((U8*)&QueryCmd, (U8*)p, sizeof(NATPTQueryCommand));

	pEntry = NATPT_find(QueryCmd.socketA, QueryCmd.socketB);
	if (!pEntry)
		return ERR_NATPT_UNKNOWN_CONNECTION;

	pResp = (PNATPTQueryResponse)p;
	memset((U8 *)pResp, 0, sizeof(NATPTQueryResponse));
	pResp->socketA = pEntry->socketA;
	pResp->socketB = pEntry->socketB;
	pResp->control = pEntry->control;
#if defined(COMCERTO_2000)
	natpt_update_stats(pEntry);
#endif
	pResp->stat_v6_received = pEntry->stat_v6_received;
	pResp->stat_v6_transmitted = pEntry->stat_v6_transmitted;
	pResp->stat_v6_dropped = pEntry->stat_v6_dropped;
	pResp->stat_v6_sent_to_ACP = pEntry->stat_v6_sent_to_ACP;
	pResp->stat_v4_received = pEntry->stat_v4_received;
	pResp->stat_v4_transmitted = pEntry->stat_v4_transmitted;
	pResp->stat_v4_dropped = pEntry->stat_v4_dropped;
	pResp->stat_v4_sent_to_ACP = pEntry->stat_v4_sent_to_ACP;

	return NO_ERR;
}


static U16 M_natpt_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 cmdrc;
	U16 cmdlen = 2;

	switch (cmd_code)
	{
	case CMD_NATPT_OPEN:
		cmdrc = NATPT_Open(pcmd, cmd_len);
		break;

	case CMD_NATPT_CLOSE:
		cmdrc = NATPT_Close(pcmd, cmd_len);
		break;

	case CMD_NATPT_QUERY:
		cmdrc = NATPT_Query(pcmd, cmd_len);
		cmdlen = sizeof(NATPTQueryResponse);
		break;

	default:
		cmdrc = ERR_UNKNOWN_COMMAND;
		break;
	}

	*pcmd = cmdrc;
	return cmdlen;
}

#if !defined(COMCERTO_2000)
BOOL M_natpt_init(PModuleDesc pModule)
{
	/* module entry point and channel registration */
	pModule->entry = &M_natpt_receive;
	pModule->cmdproc = &M_natpt_cmdproc;
	return 0;
}

#else // defined(COMCERTO_2000)

BOOL natpt_init(void)
{
	int i;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	set_cmd_handler(EVENT_NATPT, M_natpt_cmdproc);

	natpt_dma_pool = ctrl->dma_pool_512;

	for (i = 0; i < NUM_NATPT_ENTRIES; i++)
		slist_head_init(&natpt_cache[i]);

	dlist_head_init(&hw_natpt_removal_list);

	timer_init(&natpt_delayed_remove_timer, hw_natpt_delayed_remove);
	timer_add(&natpt_delayed_remove_timer, CT_TIMER_INTERVAL);

	return 0;
}

void natpt_exit(void)
{
	int i;
	struct slist_entry *sl_entry;
	struct dlist_head *dl_entry;
	struct _thw_natpt *hw_entry;
	PNATPT_Entry pEntry;

	for (i = 0; i < NUM_NATPT_ENTRIES; i++)
	{
		slist_for_each_safe(pEntry, sl_entry, &natpt_cache[i], list)
		{
			natpt_remove(pEntry, i);
		}
	}

	timer_del(&natpt_delayed_remove_timer);

	dlist_for_each_safe(hw_entry, dl_entry, &hw_natpt_removal_list, list)
	{
		dlist_remove(&hw_entry->list);
		dma_pool_free(natpt_dma_pool, hw_entry, be32_to_cpu(hw_entry->dma_addr));
	}
}

#endif /* !defined(COMCERTO_2000) */
#endif /* CFG_NATPT */
