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

#include "module_rtp_relay.h"
#include "modules.h"
#include "events.h"
#include "module_ethernet.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_hidrv.h"
#include "system.h"
#include "fpp.h"
#include "layer2.h"
#include "module_stat.h"
#include "module_timer.h"
#include "module_mc4.h"
#include "module_mc6.h"
#include "voicebuf.h"
#include "fe.h"
#include "module_socket.h" 
#include "control_socket.h"
#include "control_rtp_relay.h"

#if defined(COMCERTO_2000)
#include "control_common.h"

struct dma_pool *rtp_dma_pool;
PVOID UTIL_DMEM_SH2(rtpflow_cache)[NUM_RTPFLOW_ENTRIES] __attribute__((aligned(32)));
U8 UTIL_DMEM_SH2(gDTMF_PT)[2];
extern TIMER_ENTRY rtpflow_timer;
extern struct slist_head rtpcall_list;
struct dlist_head hw_flow_active_list[NUM_RTPFLOW_ENTRIES];
struct dlist_head hw_flow_removal_list;

PVOID UTIL_DMEM_SH2(rtpqos_cache)[MAX_RTP_STATS_ENTRY] __attribute__((aligned(32)));
extern TIMER_ENTRY rtpqos_timer;
extern struct slist_head rtpqos_list;
struct dlist_head hw_rtpqos_removal_list;

#endif

static void RTP_release_flow(PRTPflow pFlow);


#if !defined(COMCERTO_2000)

PVOID rtp_call_alloc(void)
{
	return Heap_Alloc(sizeof(struct _tRTPcall));
}

void rtp_call_free(PRTPCall pCall)
{
	Heap_Free((PVOID) pCall);
}


PVOID rtp_flow_alloc(void)
{
	return Heap_Alloc(sizeof(struct _tRTPflow));
}

void rtp_flow_free(PRTPflow pFlow)
{
	Heap_Free((PVOID) pFlow);
}

static int rtp_flow_add(PRTPflow pFlow, U32 hash)
{
	/* Add software entry to local hash */
	slist_add(&rtpflow_cache[hash], &pFlow->list);

	return 0;
}

static void rtp_flow_remove(PRTPflow pFlow)
{
	U32 hash;

	hash = HASH_RTP(pFlow->ingress_socketID);

	slist_remove(&rtpflow_cache[hash], &pFlow->list);

	RTP_release_flow(pFlow);
}

#else

static PVOID rtp_call_alloc(void)
{
	return pfe_kzalloc(sizeof(struct _tRTPcall), GFP_KERNEL);
}

static void rtp_call_free(PRTPCall pCall)
{
	pfe_kfree((PVOID)pCall);
}

static void rtp_pt_add_to_pe(U8 pt1, U8 pt2)
{
	pe_dmem_write(UTIL_ID, (U32)pt1, virt_to_util_dmem((void*)&util_gDTMF_PT[0]), 1);
	pe_dmem_write(UTIL_ID, (U32)pt2, virt_to_util_dmem((void*)&util_gDTMF_PT[1]), 1);
}

static void rtp_flow_add_to_util(U32 host_dma_addr, U32 hash)
{
	U32	*dmem_addr = (U32*)&util_rtpflow_cache[hash];

	pe_dmem_write(UTIL_ID, host_dma_addr, virt_to_util_dmem(dmem_addr), 4);
}

static void rtp_flow_add_to_pe(U32 dma_addr, U32 hash)
{
	rtp_flow_add_to_util(dma_addr, hash);
}

static void hw_rtp_flow_schedule_remove(struct _thw_rtpflow *hw_flow)
{
	hw_flow->removal_time = jiffies + 2;
	dlist_add(&hw_flow_removal_list, &hw_flow->list);
}

/** Processes hardware socket delayed removal list.
* Free all hardware socket in the removal list that have reached their removal time.
*
*
*/
static void hw_rtp_flow_delayed_remove(void)
{
	struct dlist_head *entry;
	struct _thw_rtpflow *hw_flow;

	dlist_for_each_safe(hw_flow, entry, &hw_flow_removal_list, list)
	{
		if (!time_after(jiffies, hw_flow->removal_time))
			continue;

		dlist_remove(&hw_flow->list);

		dma_pool_free(rtp_dma_pool, hw_flow, be32_to_cpu(hw_flow->dma_addr));
	}
}


static PVOID rtp_flow_alloc(void)
{
	return pfe_kzalloc(sizeof(struct _tRTPflow), GFP_KERNEL);
}

static void rtp_flow_free(PRTPflow pFlow)
{
	pfe_kfree((PVOID)pFlow);
}

static int rtp_flow_add(PRTPflow pFlow, U32 hash)
{
	struct _thw_rtpflow *hw_flow;
	struct _thw_rtpflow *hw_flow_first;
	dma_addr_t dma_addr;
	int rc = NO_ERR;

	/* Allocate hardware entry */
	hw_flow = dma_pool_alloc(rtp_dma_pool, GFP_ATOMIC, &dma_addr);
	if (!hw_flow)
	{
		printk(KERN_ERR "%s: dma alloc failed\n", __func__);
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err;
	}

	memset(hw_flow, 0, sizeof(struct _thw_rtpflow));

	hw_flow->dma_addr = cpu_to_be32(dma_addr);

	/* Link software conntrack to hardware conntrack */
	pFlow->hw_flow = hw_flow;
	hw_flow->sw_flow = pFlow;

	pFlow->hash = hash;

	/* add hw entry to active list and update next pointer */
	if(!dlist_empty(&hw_flow_active_list[pFlow->hash]))
	{
		/* list is not empty, and we'll be added at head, so current first will become our next pointer */
		hw_flow_first = container_of(dlist_first(&hw_flow_active_list[pFlow->hash]), typeof(struct _thw_rtpflow), list);
		hw_entry_set_field(&hw_flow->next, hw_entry_get_field(&hw_flow_first->dma_addr));
	}
	else 
	{
		/* entry is empty, so we'll be the first and only one entry */
		hw_entry_set_field(&hw_flow->next, 0);
	}

	dlist_add(&hw_flow_active_list[pFlow->hash], &hw_flow->list);

	/* reflect changes to hardware flow */
	hw_flow->state = pFlow->state;
	hw_flow->ingress_socketID = cpu_to_be16(pFlow->ingress_socketID);
	hw_flow->egress_socketID = cpu_to_be16(pFlow->egress_socketID);
	hw_flow->SSRC = cpu_to_be32(pFlow->SSRC);
	hw_flow->TimestampBase = cpu_to_be32(pFlow->TimestampBase);
	hw_flow->TimeStampIncr = cpu_to_be32(pFlow->TimeStampIncr);
	hw_flow->takeover_mode = pFlow->takeover_mode;
	hw_flow->Seq = cpu_to_be16(pFlow->Seq);
	hw_flow->SSRC_takeover = pFlow->SSRC_takeover;
	hw_flow->TS_takeover = pFlow->TS_takeover;
	hw_flow->Seq_takeover = pFlow->Seq_takeover;
	hw_flow->takeover_resync = pFlow->takeover_resync;
	hw_flow->rtp_info.first_packet = pFlow->rtp_info.first_packet;
	hw_flow->call_update_seen = FALSE;

	hw_flow->Special_tx_active = pFlow->RTPcall->Special_tx_active;
	hw_flow->Special_tx_type = pFlow->RTPcall->Special_tx_type;
	memcpy(hw_flow->Special_payload1, pFlow->RTPcall->Special_payload1, RTP_SPECIAL_PAYLOAD_LEN);
	memcpy(hw_flow->Special_payload2, pFlow->RTPcall->Special_payload2, RTP_SPECIAL_PAYLOAD_LEN);

	hw_flow->rtp_info.probation = pFlow->rtp_info.probation;
	hw_flow->hash = pFlow->hash;

	/* Update PE's internal memory socket cache tables with the HW entry's DDR address */
	rtp_flow_add_to_pe(hw_flow->dma_addr, hash);

	/* Add software entry to local hash */
	slist_add(&rtpflow_cache[hash], &pFlow->list);

	return NO_ERR;

err:
	return rc;
}

/* add a hardware flow entry to packet engine hash */
static void rtp_flow_link(struct _thw_rtpflow *hw_flow, U32 hash)
{
	struct _thw_rtpflow *hw_flow_first;

	/* add hw entry to active list and update next pointer */
	if(!dlist_empty(&hw_flow_active_list[hash]))
	{
		/* list is not empty, and we'll be added at head, so current first will become our next pointer */
		hw_flow_first = container_of(dlist_first(&hw_flow_active_list[hash]), typeof(struct _thw_rtpflow), list);
		hw_entry_set_field(&hw_flow->next, hw_entry_get_field(&hw_flow_first->dma_addr));
	}
	else
	{
		/* entry is empty, so we'll be the first and only one entry */
		hw_entry_set_field(&hw_flow->next, 0);
	}

	/* this rtp flow is now the head of the hw entry list, so put it also to pfe's internal hash */
	rtp_flow_add_to_pe(hw_flow->dma_addr, hash);

	dlist_add(&hw_flow_active_list[hash], &hw_flow->list);
}

/* remove a hardware flow entry from the packet engine hash */
static void rtp_flow_unlink(struct _thw_rtpflow *hw_flow, U32 hash)
{
	struct _thw_rtpflow *hw_flow_prev;

	if (&hw_flow->list == dlist_first(&hw_flow_active_list[hash])) 
	{
		rtp_flow_add_to_pe(hw_entry_get_field(&hw_flow->next), hash);
	}
	else
	{
		hw_flow_prev = container_of(hw_flow->list.prev, typeof(struct _thw_rtpflow), list);
		hw_entry_set_field(&hw_flow_prev->next, hw_entry_get_field(&hw_flow->next));
	}
	dlist_remove(&hw_flow->list);
}

static void rtp_flow_remove(PRTPflow pFlow)
{
	struct _thw_rtpflow *hw_flow;
	struct _thw_rtpflow *hw_flow_prev;
	U32 hash = HASH_RTP(pFlow->ingress_socketID);

	/* Check if there is a hardware flow */
	if ((hw_flow = pFlow->hw_flow))
	{
		/* Detach from software socket */
		pFlow->hw_flow = NULL;

		/* if the removed entry is first in hash slot then only PE dmem hash need to be updated */
		if (&hw_flow->list == dlist_first(&hw_flow_active_list[hash])) 
		{
			rtp_flow_add_to_pe(hw_entry_get_field(&hw_flow->next), hash);
		}
		else
		{
			hw_flow_prev = container_of(hw_flow->list.prev, typeof(struct _thw_rtpflow), list);
			hw_entry_set_field(&hw_flow_prev->next, hw_entry_get_field(&hw_flow->next));
		}

		dlist_remove(&hw_flow->list);

		hw_rtp_flow_schedule_remove(hw_flow);
	}

	slist_remove(&rtpflow_cache[hash], &pFlow->list);

	RTP_release_flow(pFlow);
}

#endif

static PRTPflow RTP_create_flow(U16 in_socket, U16 out_socket)
{
	struct _tRTPflow* pFlow;

	pFlow = rtp_flow_alloc();
	if (pFlow) {
		memset(pFlow, 0, sizeof(struct _tRTPflow));
		pFlow->ingress_socketID = in_socket;
		pFlow->egress_socketID = out_socket;
		pFlow->rtp_info.first_packet = TRUE;
		pFlow->rtp_info.probation = RTP_MIN_SEQUENTIAL;

		SOCKET_bind(in_socket, pFlow, SOCK_OWNER_RTP_RELAY);
	}

	return pFlow;
}

static void RTP_release_flow(PRTPflow pFlow)
{
	SOCKET_unbind(pFlow->ingress_socketID);

	rtp_flow_free(pFlow);
}

static void RTP_change_flow(PRTPflow pFlow, U16 ingress_socketID, U16 egress_socketID)
{
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct _thw_rtpflow *hw_flow = pFlow->hw_flow;
#endif
	U32	hash;

	pFlow->takeover_resync = TRUE;
	pFlow->rtp_info.first_packet = TRUE;

	hash = HASH_RTP(pFlow->ingress_socketID);

#if defined (COMCERTO_2000)
	/* unlink (not destroy) hardware flow from packet engine since hash slot may have changed for this flow */
	rtp_flow_unlink(hw_flow, hash);
#endif

	/* now managing changes in software flow */
	slist_remove(&rtpflow_cache[hash], &pFlow->list);

	SOCKET_unbind(pFlow->ingress_socketID);

	pFlow->ingress_socketID = ingress_socketID;
	pFlow->egress_socketID = egress_socketID;

	SOCKET_bind(ingress_socketID, pFlow, SOCK_OWNER_RTP_RELAY);

	hash = HASH_RTP(ingress_socketID);

	slist_add(&rtpflow_cache[hash], &pFlow->list);

#if defined(COMCERTO_2000)
	/* reflect changes to hardware flow */

	/* We must stop UTIL since some of these flags are written to by UTIL */
	pe_sync_stop(ctrl, (1 << UTIL_ID));

	hw_flow->ingress_socketID = cpu_to_be16(pFlow->ingress_socketID);
	hw_flow->egress_socketID = cpu_to_be16(pFlow->egress_socketID);
	hw_flow->takeover_resync = pFlow->takeover_resync;
	hw_flow->rtp_info.first_packet = pFlow->rtp_info.first_packet;
	hw_flow->call_update_seen = TRUE;

	pe_start(ctrl, (1 << UTIL_ID));
#else
	pFlow->call_update_seen = TRUE;
#endif

#if defined (COMCERTO_2000)
	/* add back hardware flow to its new location in packet engine hash */
	rtp_flow_link(hw_flow, hash);
#endif
}


int rtp_flow_reset(void)
{
	PRTPflow pEntry;
	struct slist_entry *entry;
	int hash;

	for(hash = 0; hash < NUM_RTPFLOW_ENTRIES; hash++)
	{
		slist_for_each_safe(pEntry, entry, &rtpflow_cache[hash], list)
		{
			rtp_flow_remove(pEntry);
		}
	}

	return NO_ERR;
}

static PRTPCall RTP_find_call(U16 CallID)
{
	PRTPCall pCall;
	struct slist_entry *entry;

	slist_for_each_entry(entry, &rtpcall_list)
	{
		pCall = container_of(entry, typeof(struct _tRTPcall), list);
		if (pCall->valid && (pCall->call_id == CallID))
			return 	pCall;
	}

	return NULL;
}


static PRTPCall RTP_create_call(U16 CallID)
{
	PRTPCall pCall = NULL;

	if((pCall = rtp_call_alloc()))
	{
		pCall->valid = TRUE;
		pCall->call_id = CallID;

		slist_add(&rtpcall_list, &pCall->list);
	}

	return pCall;
}


static void RTP_release_call(PRTPCall pCall)
{
	if(pCall)
	{
		slist_remove(&rtpcall_list, &pCall->list);

		rtp_call_free(pCall);
	}
}

int rtp_call_reset(void)
{
	PRTPCall pEntry;
	struct slist_entry *entry;

	slist_for_each_safe(pEntry, entry, &rtpcall_list, list)
	{
		RTP_release_call(pEntry);
	}

	return NO_ERR;
}

static U16 RTP_Call_Open (U16 *p, U16 Length)
{
	RTPOpenCommand RTPCmd;
	PRTPCall pCall;
	PSockEntry pSocketA = NULL, pSocketB = NULL;
	int rc = NO_ERR;

	// Check length
	if (Length != sizeof(RTPOpenCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RTPCmd, (U8*)p, sizeof(RTPOpenCommand));

	// Make sure sockets exist but are unused
	if (RTPCmd.SocketA) {
		pSocketA = SOCKET_find_entry_by_id(RTPCmd.SocketA);
		if (!pSocketA)
			return ERR_SOCKID_UNKNOWN;

		if (pSocketA->owner)
			return ERR_SOCK_ALREADY_IN_USE;

		if (RTPCmd.SocketA == RTPCmd.SocketB)
			return ERR_SOCK_ALREADY_IN_USE;
	}

	if (RTPCmd.SocketB) {
		pSocketB = SOCKET_find_entry_by_id(RTPCmd.SocketB);
		if (!pSocketB)
			return ERR_SOCKID_UNKNOWN;

		if (pSocketB->owner)
			return ERR_SOCK_ALREADY_IN_USE;
	}

	if (pSocketA && pSocketB)
	{
		if (pSocketA->SocketFamily != pSocketB->SocketFamily)
			return ERR_WRONG_SOCK_FAMILY;

		if (pSocketA->proto != pSocketB->proto)
			return ERR_WRONG_SOCK_PROTO;
	}

	if (RTP_find_call(RTPCmd.CallID))
		return ERR_RTP_CALLID_IN_USE;

	pCall = RTP_create_call(RTPCmd.CallID);
	if (pCall == NULL)
		return ERR_CREATION_FAILED;

	pCall->AtoB_flow = RTP_create_flow(RTPCmd.SocketA, RTPCmd.SocketB);
	if (pCall->AtoB_flow == NULL) {
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err_flow_a;
	}

	pCall->AtoB_flow->RTPcall = pCall;

	pCall->BtoA_flow = RTP_create_flow(RTPCmd.SocketB, RTPCmd.SocketA);
	if (pCall->BtoA_flow == NULL) {
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err_flow_b;
	}

	pCall->BtoA_flow->RTPcall = pCall;

	/* Now adding hardware flows to packet engine's flow cache */	
	if(rtp_flow_add(pCall->AtoB_flow, HASH_RTP(RTPCmd.SocketA)) != NO_ERR)
	{
		printk(KERN_ERR "%s: AtoB ERR_NOT_ENOUGH_MEMORY\n", __func__);
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err_hw_flow_a;
	}

	if(rtp_flow_add(pCall->BtoA_flow, HASH_RTP(RTPCmd.SocketB)) != NO_ERR)
	{
		printk(KERN_ERR "%s: BtoA ERR_NOT_ENOUGH_MEMORY\n", __func__);
		rtp_flow_remove(pCall->AtoB_flow);
		RTP_release_flow(pCall->BtoA_flow);
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err_hw_flow_b;
	}

	return NO_ERR;

err_hw_flow_a:
	RTP_release_flow(pCall->BtoA_flow);

err_flow_b:
	RTP_release_flow(pCall->AtoB_flow);

err_flow_a:
err_hw_flow_b:
	RTP_release_call(pCall);

	return rc;
}


static U16 RTP_Call_Update (U16 *p, U16 Length)
{
	RTPOpenCommand RTPCmd;
	PRTPCall pCall;
	PSockEntry pSocketA = NULL, pSocketB = NULL;

	// Check length
	if (Length != sizeof(RTPOpenCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RTPCmd, (U8*)p, sizeof(RTPOpenCommand));

	if ((pCall = RTP_find_call(RTPCmd.CallID)) == NULL)
		return ERR_RTP_UNKNOWN_CALL;

	if (RTPCmd.SocketA && (RTPCmd.SocketA == RTPCmd.SocketB))
		return ERR_SOCK_ALREADY_IN_USE;

	if (RTPCmd.SocketA)
	{
		pSocketA = SOCKET_find_entry_by_id(RTPCmd.SocketA);
		if (!pSocketA)
			return ERR_SOCKID_UNKNOWN;

		if (pSocketA->owner && (pCall->AtoB_flow->ingress_socketID != RTPCmd.SocketA))
			return ERR_SOCK_ALREADY_IN_USE;
	}
	else
		pSocketA = SOCKET_find_entry_by_id(pCall->AtoB_flow->ingress_socketID);

	if (RTPCmd.SocketB)
	{
		pSocketB = SOCKET_find_entry_by_id(RTPCmd.SocketB);
		if (!pSocketB)
			return ERR_SOCKID_UNKNOWN;

		if (pSocketB->owner && (pCall->BtoA_flow->ingress_socketID != RTPCmd.SocketB))
			return ERR_SOCK_ALREADY_IN_USE;
	}
	else
		pSocketB = SOCKET_find_entry_by_id(pCall->BtoA_flow->ingress_socketID);

	if (pSocketA && pSocketB)
	{
		if (pSocketA->SocketFamily != pSocketB->SocketFamily)
			return ERR_WRONG_SOCK_FAMILY;

		if (pSocketA->proto != pSocketB->proto)
			return ERR_WRONG_SOCK_PROTO;
	}

	RTP_change_flow(pCall->BtoA_flow, RTPCmd.SocketB, RTPCmd.SocketA);
	RTP_change_flow(pCall->AtoB_flow, RTPCmd.SocketA, RTPCmd.SocketB);

	return NO_ERR;
}


static U16 RTP_Call_Close (U16 *p, U16 Length)
{
	RTPCloseCommand RTPCmd;
	PRTPCall pCall;
	int rc = NO_ERR;

	// Check length
	if (Length != sizeof(RTPCloseCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RTPCmd, (U8*)p, sizeof(RTPCloseCommand));

	if ((pCall = RTP_find_call(RTPCmd.CallID)) == NULL)
		return ERR_RTP_UNKNOWN_CALL;

	/* remove hardware flow from packet engine */
	rtp_flow_remove(pCall->BtoA_flow);
	rtp_flow_remove(pCall->AtoB_flow);

	RTP_release_call(pCall);

	return rc;
}

static U16 RTP_Call_Control (U16 *p, U16 Length)
{
	RTPControlCommand RTPCmd;
	PRTPCall pCall;
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
#endif

	// Check length
	if (Length != sizeof(RTPControlCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RTPCmd, (U8*)p, sizeof(RTPControlCommand));

	if ((pCall = RTP_find_call(RTPCmd.CallID)) == NULL)
		return ERR_RTP_UNKNOWN_CALL;

#if !defined(COMCERTO_2000)
	pCall->AtoB_flow->state = (RTPCmd.ControlDir & 0x1);
	pCall->BtoA_flow->state = (RTPCmd.ControlDir & 0x2);
#else
	/* FIXME, once UTIL stops overwritting all the contents of DDR this can be relaxed
	to a UPDATING flag since data path never updates these variables */
	pe_sync_stop(ctrl, (1 << UTIL_ID));

	pCall->AtoB_flow->hw_flow->state = RTPCmd.ControlDir & 0x1;
	pCall->BtoA_flow->hw_flow->state = RTPCmd.ControlDir & 0x2;

	pe_start(ctrl, (1 << UTIL_ID));
#endif

	return NO_ERR;
}


static U16 RTP_Call_TakeOver (U16 *p, U16 Length)
{
	RTPTakeoverCommand RTPCmd;
	PRTPflow pflow;
	PRTPCall pCall;
	PSockEntry pSocket = NULL;
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct _thw_rtpflow *hw_flow;
	PRTCPStats egress_stats = NULL;
#endif

	// Check length
	if (Length != sizeof(RTPTakeoverCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RTPCmd, (U8*)p, sizeof(RTPTakeoverCommand));

	if ((pCall = RTP_find_call(RTPCmd.CallID)) == NULL)
		return ERR_RTP_UNKNOWN_CALL;

	if ((pSocket = SOCKET_find_entry_by_id(RTPCmd.Socket)) == NULL)
		return ERR_SOCKID_UNKNOWN;

	if (pCall->AtoB_flow->egress_socketID == RTPCmd.Socket)
		pflow = pCall->AtoB_flow;
	else if (pCall->BtoA_flow->egress_socketID == RTPCmd.Socket)
		pflow = pCall->BtoA_flow;
	else
		return ERR_WRONG_SOCKID;

#if !defined(COMCERTO_2000)
	pflow->SSRC = RTPCmd.SSRC;
	pflow->TimestampBase = RTPCmd.TimeStampBase;
	pflow->TimeStampIncr = RTPCmd.TimeStampIncr;
	pflow->takeover_mode = (RTPCmd.mode & RTP_TAKEOVER_MODE_TSINCR_FREQ);
	pflow->Seq = RTPCmd.SeqNumberBase;
	pflow->SSRC_takeover =  (((pflow->SSRC) == 0) ? RTP_TAKEOVER_SSRC_TRANSPARENT : RTP_TAKEOVER_SSRC_MANGLE);
	pflow->TS_takeover =  (((pflow->TimestampBase) == 0) ? 0 : 1);
	pflow->Seq_takeover =  (((pflow->Seq) == 0) ? 0 : 1);

	pflow->takeover_resync = TRUE;
	pflow->rtp_info.first_packet = TRUE;
#else
	/* reflect changes in hardware flow */
	hw_flow = pflow->hw_flow;

	/* We must stop UTIL since some of these flags are written to by UTIL */
	pe_sync_stop(ctrl, (1 << UTIL_ID));

	hw_flow->SSRC = cpu_to_be32(RTPCmd.SSRC);
	hw_flow->TimestampBase = cpu_to_be32(RTPCmd.TimeStampBase);
	hw_flow->TimeStampIncr = cpu_to_be32(RTPCmd.TimeStampIncr);
	hw_flow->takeover_mode = RTPCmd.mode;
	hw_flow->Seq = RTPCmd.SeqNumberBase;
	hw_flow->SSRC_takeover = (((hw_flow->SSRC) == 0) ? RTP_TAKEOVER_SSRC_TRANSPARENT : RTP_TAKEOVER_SSRC_MANGLE);
	if (hw_flow->takeover_mode & RTP_TAKEOVER_MODE_SSRC)
	{
		/* If automatic SSRC takeover is configured then let's the control plane generates the initial SSRC value. If any
		change in incoming SSRC occurs during the call, then the PFE will recomputes a new ssrc internally */
		hw_flow->SSRC_takeover = RTP_TAKEOVER_SSRC_AUTO;
		hw_flow->SSRC = cpu_to_be32((pflow->egress_socketID << 16) | (jiffies & 0xFFFF));

		/* Initial SSRC computed by the Host control code should be also reflected in the egress socket's statistics */
		egress_stats = (PRTCPStats)pSocket->hw_sock->SocketStats;
		egress_stats->ssrc_overwrite_value = hw_flow->SSRC;
	}
	hw_flow->TS_takeover = (((hw_flow->TimestampBase) == 0) ? 0 : 1);
	hw_flow->Seq_takeover = (((hw_flow->Seq) == 0) ? 0 : 1);

	hw_flow->takeover_resync = TRUE;
	hw_flow->rtp_info.first_packet = TRUE;

	pe_start(ctrl, (1 << UTIL_ID));
#endif

	return NO_ERR;
}


static U16 RTP_Call_SpecialTx_Payload (U16 *p, U16 Length)
{
	RTPSpecTxPayloadCommand RTPCmd;
	PRTPCall pCall;
	U8*	payload;

	// Check length
	if (Length < sizeof(RTPSpecTxPayloadCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RTPCmd, (U8*)p, sizeof(RTPSpecTxPayloadCommand));

	if ((pCall = RTP_find_call(RTPCmd.CallID)) == NULL)
		return ERR_RTP_UNKNOWN_CALL;

	if (RTPCmd.payloadID)
		payload = pCall->Next_Special_payload2;
	else
		payload = pCall->Next_Special_payload1;

	memset(payload, 0, RTP_SPECIAL_PAYLOAD_LEN);
	SFL_memcpy(payload, RTPCmd.payload, RTPCmd.payloadLength);

	return NO_ERR;
}


static U16 RTP_Call_SpecialTx_Control (U16 *p, U16 Length)
{
	RTPSpecTxCtrlCommand RTPCmd;
	PRTPCall pCall;
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
#endif

	// Check length
	if (Length < sizeof(RTPSpecTxCtrlCommand))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&RTPCmd, (U8*)p, sizeof(RTPSpecTxCtrlCommand));

	if ((pCall = RTP_find_call(RTPCmd.CallID)) == NULL)
		return ERR_RTP_UNKNOWN_CALL;

#if !defined(COMCERTO_2000)
	if (RTPCmd.Type == RTP_SPEC_TX_STOP)
	{
		pCall->Special_tx_active = 0;
		pCall->Special_tx_type = RTPCmd.Type; //this line is needed to stop sending special packet to ACP if TX_STOP is received
	}
	else
	{
		SFL_memcpy(pCall->Special_payload1, pCall->Next_Special_payload1, RTP_SPECIAL_PAYLOAD_LEN);
		SFL_memcpy(pCall->Special_payload2, pCall->Next_Special_payload2, RTP_SPECIAL_PAYLOAD_LEN);

		pCall->Special_tx_active = 1;
		pCall->Special_tx_type = RTPCmd.Type;
	}
#else
	/* reflect changes in hardware flow */

	/* We must stop UTIL since some of these flags are written to by UTIL */
	pe_sync_stop(ctrl, (1 << UTIL_ID));

	if (RTPCmd.Type == RTP_SPEC_TX_STOP)
	{
		pCall->AtoB_flow->hw_flow->Special_tx_active = 0;
		pCall->BtoA_flow->hw_flow->Special_tx_active = 0;
	}
	else
	{
		pCall->AtoB_flow->hw_flow->Special_tx_active = 1;
		pCall->BtoA_flow->hw_flow->Special_tx_active = 1;

		memcpy(pCall->AtoB_flow->hw_flow->Special_payload1, pCall->Next_Special_payload1, RTP_SPECIAL_PAYLOAD_LEN);
		memcpy(pCall->AtoB_flow->hw_flow->Special_payload2, pCall->Next_Special_payload2, RTP_SPECIAL_PAYLOAD_LEN);

		memcpy(pCall->BtoA_flow->hw_flow->Special_payload1, pCall->Next_Special_payload1, RTP_SPECIAL_PAYLOAD_LEN);
		memcpy(pCall->BtoA_flow->hw_flow->Special_payload2, pCall->Next_Special_payload2, RTP_SPECIAL_PAYLOAD_LEN);
	}

	pCall->AtoB_flow->hw_flow->Special_tx_type = RTPCmd.Type;
	pCall->BtoA_flow->hw_flow->Special_tx_type = RTPCmd.Type;

	pe_start(ctrl, (1 << UTIL_ID));
#endif

	return NO_ERR;
}


/* This function is used by both RTP Relay Stats and RTP FF Stats feature */
static int RTP_query_stats_common(PRTCPQueryResponse pRTPRep, PRTCPStats pStats)
{
	U8 first_packet = 0;
	U32 num_rx_valid = 0;

	if((pStats == NULL) || (pRTPRep == NULL))
		return 1;

	if(pStats->prev_reception_period >= 1000)
		pRTPRep->prev_reception_period = pStats->prev_reception_period / 1000; // expressed in msec
	if(pStats->last_reception_period >= 1000)
		pRTPRep->last_reception_period = pStats->last_reception_period / 1000; //expressed in msec
	pRTPRep->num_tx_pkts = pStats->num_tx_pkts;
	pRTPRep->num_rx_pkts = pStats->num_rx_pkts;
	pRTPRep->last_rx_Seq = pStats->last_rx_Seq;
	pRTPRep->last_TimeStamp = pStats->last_TimeStamp;
	SFL_memcpy(pRTPRep->RTP_header, pStats->first_received_RTP_header, RTP_HDR_SIZE);	
	pRTPRep->num_rx_dup = pStats->packets_duplicated;
	pRTPRep->num_rx_since_RTCP = pStats->num_rx_since_RTCP;
	pRTPRep->num_tx_bytes = pStats->num_tx_bytes;
	pRTPRep->num_malformed_pkts = pStats->num_malformed_pkts;
	pRTPRep->num_expected_pkts = pStats->num_expected_pkts;
	pRTPRep->num_late_pkts = pStats->num_late_pkts;
	pRTPRep->ssrc_overwrite_value = pStats->ssrc_overwrite_value;

	if (pStats->num_expected_pkts > pStats->num_rx_pkts_in_seq)
		pRTPRep->num_rx_lost_pkts = pStats->num_expected_pkts - pStats->num_rx_pkts_in_seq;

    pRTPRep->num_cumulative_rx_lost_pkts = pStats->num_previous_rx_lost_pkts + pRTPRep->num_rx_lost_pkts;

	if(pStats->num_rx_pkts > 1) 
	{
		//jitter statistics
		pRTPRep->min_jitter = (pStats->min_jitter != 0xffffffff)? (pStats->min_jitter >> 4): 0; //if min value has never been computed just return 0
		pRTPRep->max_jitter = pStats->max_jitter >> 4; //expressed in us
		pRTPRep->mean_jitter = pStats->mean_jitter >> 4; //expressed in us

		//interarrival statistics
		pRTPRep->min_reception_period = (pStats->min_reception_period != 0xffffffff)? pStats->min_reception_period : 0;//expressed in us	
		pRTPRep->max_reception_period = pStats->max_reception_period; //expressed in us

		//first rtp packet of the session is not include in the average_reception_period variable, as we need at least 2 packets to compute an interval
		//that's why we substract one packet when computing average
		if(pStats->state == RTP_STATS_FIRST_PACKET) 
			first_packet = 1;

		//FIXME: do_div should be implemented in hal for non-linux code
		num_rx_valid = pStats->num_rx_pkts - first_packet - pStats->num_late_pkts - pStats->packets_duplicated - pStats->num_big_jumps;
		if((pStats->average_reception_period >= num_rx_valid) && (num_rx_valid))
		{
#if defined(COMCERTO_2000_CONTROL)
			pRTPRep->average_reception_period = pStats->average_reception_period;
			do_div(pRTPRep->average_reception_period, num_rx_valid); 		//expressed in us
#else
			pRTPRep->average_reception_period = pStats->average_reception_period / num_rx_valid; //expressed in us
#endif
		}
	}
	else
	{
		//make sure clean values are reported even if no packets received
		pRTPRep->min_jitter = 0;
		pRTPRep->max_jitter = 0;
		pRTPRep->mean_jitter = 0;
		pRTPRep->min_reception_period = 0;
		pRTPRep->max_reception_period = 0;
		pRTPRep->average_reception_period = 0;
	}

	pStats->num_rx_since_RTCP = 0;

#if defined (COMCERTO_2000)
	pRTPRep->sport = ntohs(pStats->sport);
	pRTPRep->dport = ntohs(pStats->dport);
#else
	pRTPRep->sport = pStats->sport;
	pRTPRep->dport = pStats->dport;
#endif

	return NO_ERR;
}

#if defined(COMCERTO_2000)
static PRTCPStats RTCP_get_stats(PRTCPStats sw_stats, PRTCPStats hw_stats, U32 stats_size)
{
	PRTCPStats pStats = NULL;
	U32 msb32, lsb32 = 0;

	if(hw_stats)
	{
		/* set back statistics from util to host endianess */
		memcpy(sw_stats, hw_stats, stats_size);
		pStats = sw_stats;

		pStats->prev_reception_period = be32_to_cpu(pStats->prev_reception_period);
		pStats->last_reception_period = be32_to_cpu(pStats->last_reception_period);
		pStats->num_tx_pkts = be32_to_cpu(pStats->num_tx_pkts);
		pStats->num_rx_pkts = be32_to_cpu(pStats->num_rx_pkts);
        pStats->num_rx_pkts_in_seq = be32_to_cpu(pStats->num_rx_pkts_in_seq);
		pStats->last_rx_Seq = be16_to_cpu(pStats->last_rx_Seq);
		pStats->last_TimeStamp = be32_to_cpu(pStats->last_TimeStamp);
		pStats->packets_duplicated = be32_to_cpu(pStats->packets_duplicated);
		pStats->num_rx_since_RTCP = be32_to_cpu(pStats->num_rx_since_RTCP);
		pStats->num_tx_bytes = be32_to_cpu(pStats->num_tx_bytes);
		pStats->min_jitter = be32_to_cpu(pStats->min_jitter);
		pStats->max_jitter = be32_to_cpu(pStats->max_jitter);
		pStats->mean_jitter = be32_to_cpu(pStats->mean_jitter);
		pStats->num_rx_lost_pkts = be32_to_cpu(pStats->num_rx_lost_pkts);
		pStats->min_reception_period = be32_to_cpu(pStats->min_reception_period);
		pStats->max_reception_period = be32_to_cpu(pStats->max_reception_period);
		msb32 = be32_to_cpu(pStats->average_reception_period << 32); lsb32 = be32_to_cpu(pStats->average_reception_period >> 32);
		pStats->average_reception_period = msb32 | lsb32;
		pStats->num_malformed_pkts = be32_to_cpu(pStats->num_malformed_pkts);
		pStats->num_expected_pkts = be32_to_cpu(pStats->num_expected_pkts);
		pStats->num_late_pkts = be32_to_cpu(pStats->num_late_pkts);
		pStats->sport = be16_to_cpu(pStats->sport);
		pStats->dport = be16_to_cpu(pStats->dport);
		pStats->num_big_jumps = be32_to_cpu(pStats->num_big_jumps);
		pStats->num_previous_rx_lost_pkts = be32_to_cpu(pStats->num_previous_rx_lost_pkts);
		pStats->ssrc_overwrite_value = be32_to_cpu(pStats->ssrc_overwrite_value);
		pStats->state = be16_to_cpu(pStats->state);
	}

	return pStats;
}

static void RTP_reset_stats(PRTCPStats pStats, U8 type)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	pe_sync_stop(ctrl, (1 << UTIL_ID));

	RTP_clear_stats(pStats, type);

	pStats->state = cpu_to_be16(type);

	pe_start(ctrl, (1 << UTIL_ID));
}
#else
static PRTCPStats RTCP_get_stats(PSockEntry pSock)
{
	return (PRTCPStats)pSock->SocketStats;
}

static void RTP_reset_stats(PRTCPStats pStats, U8 type)
{
	RTP_clear_stats(pStats, type);

	pStats->state = type;
}
#endif

static U16 RTCP_Query (U16 *p, U16 Length)
{
	RTCPQueryCommand *pRTPCmd = (RTCPQueryCommand *)p;
	RTCPQueryResponse RTPRep;
	PSockEntry pSock = NULL;
	PRTCPStats pStats = NULL;

	if (Length < sizeof(RTCPQueryCommand))
		return ERR_WRONG_COMMAND_SIZE;

	pSock = SOCKET_find_entry_by_id(pRTPCmd->SocketID);

	if (!pSock)
		return ERR_SOCKID_UNKNOWN;

	if (pSock->SocketType == SOCKET_TYPE_ACP)
		return ERR_WRONG_SOCK_TYPE;

	pStats = RTCP_get_stats((PRTCPStats)pSock->SocketStats, (PRTCPStats)pSock->hw_sock->SocketStats, SOCKET_STATS_SIZE);
	if(pStats == NULL)
		return ERR_RTP_STATS_NOT_AVAILABLE;

	memset((U8*)&RTPRep, 0, sizeof(RTCPQueryResponse));
	if (RTP_query_stats_common(&RTPRep, pStats))
		return ERR_RTP_STATS_STREAMID_UNKNOWN;

#if defined (COMCERTO_2000)
	pStats = (PRTCPStats)pSock->hw_sock->SocketStats;
	pStats->num_rx_since_RTCP = 0;
#endif
	
	if(pRTPCmd->flags)
		RTP_reset_stats((PRTCPStats)pSock->hw_sock->SocketStats, pRTPCmd->flags);

	SFL_memcpy((U8*)(p + 1), (U8*)&RTPRep, sizeof(RTCPQueryResponse));
	return NO_ERR;
}

void RTP_clear_stats(PRTCPStats pStats, U8 type)
{
	if(type == RTP_STATS_FULL_RESET) //full reset, all stats are cleared
	{
		pStats->prev_reception_period = 0;
		pStats->last_reception_period = 0;
		pStats->seq_base = 0;
		pStats->last_rx_Seq = 0;
		pStats->last_TimeStamp= 0;
		pStats->num_rx_pkts_in_seq = 0;
		pStats->num_expected_pkts = 0;
		pStats->num_previous_rx_lost_pkts = 0;
	}
	if(type == RTP_STATS_FULL_RESET || type == RTP_STATS_PARTIAL_RESET) //partial reset used for session restart
	{
		pStats->num_tx_pkts = 0;
		pStats->num_tx_bytes = 0;
	}
	if(type == RTP_STATS_FULL_RESET || type == RTP_STATS_PARTIAL_RESET || type == RTP_STATS_RX_RESET)
	{
		pStats->max_jitter = pStats->mean_jitter = 0;
		pStats->min_jitter = 0xffffffff;
		pStats->max_reception_period = pStats->average_reception_period = 0;
		pStats->min_reception_period = 0xffffffff;
		pStats->num_rx_pkts = 0;
		pStats->packets_duplicated = 0;
		pStats->num_malformed_pkts = 0;
		pStats->num_late_pkts = 0;
		pStats->num_big_jumps = 0;
	}
}


/*************************** RTP Stats for QoS Measurement ****************************
Notes:
-----
The goal of this feature is to add RTP QoS MEasurement support for both fast forwarded and Relayed connections in C1000
FPP code. This feature is different from the RTCP Query for RTP Relay feature, but provides similar service.

MSPD has implements similar API and common code in FPP to collect both RTP statistics for
Relay and Fast Forwarded connections (only CMM CLI usage differs). So in terms of RTP
Statistics only (processing, statistics format, etc) 

Same control plane is used for both RTP FF Stats and RTP Relay Stats in order to minimize FPP modules
usage
************************************************************************************/


static void rtpqos_free_entry(PRTPQOS_ENTRY pEntry)
{
	if(pEntry->slot < MAX_RTP_STATS_ENTRY)
	{
		memset((U8*)&pEntry->rtp_info, 0, sizeof(RTPinfo));
		memset((U8*)&pEntry->stats, 0, sizeof(RTCPStats));
		pEntry->stream_id = RTP_STATS_FREE;
		pEntry->stream_type = 0;
	}
}

static PRTPQOS_ENTRY rtpqos_alloc_entry(U16 stream_id)
{
	int i;
	PRTPQOS_ENTRY pStat = NULL;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++) {
		if (rtpqos_cache[i].stream_id == RTP_STATS_FREE) {
			pStat = &rtpqos_cache[i];
			pStat->stream_id = stream_id;
			break;
		}
	}
	return pStat;
}


#if !defined(COMCERTO_2000)

static void rtpqos_remove_entry(PRTPQOS_ENTRY pEntry)
{
	rtpqos_free_entry(pEntry);
}

static void rtpqos_update(PSockEntry pSocket)
{
}

#else

static void rtpqos_update_pe_cache(U32 host_dma_addr, U32 slot)
{
	U32	*dmem_addr = (U32*)&util_rtpqos_cache[slot];

	pe_dmem_write(UTIL_ID, host_dma_addr, virt_to_util_dmem(dmem_addr), 4);
}

static void rtpqos_update_entry(PRTPQOS_ENTRY pEntry)
{
	struct _thw_rtpqos_entry *hw_rtpqos = pEntry->hw_rtpqos;

	if(!pEntry->hw_rtpqos)
		return;

	hw_entry_set_flags(&hw_rtpqos->flags, RTPQOS_UPDATING | RTPQOS_VALID);

	SFL_memcpy(hw_rtpqos->saddr, pEntry->saddr, 4*sizeof(U32)); SFL_memcpy(hw_rtpqos->daddr, pEntry->daddr, 4*sizeof(U32));
	hw_rtpqos->sport = pEntry->sport;  hw_rtpqos->dport = pEntry->dport;
	hw_rtpqos->proto = pEntry->proto;

	hw_rtpqos->stream_type = cpu_to_be16(pEntry->stream_type);
	hw_rtpqos->stream_id = pEntry->stream_id;

	hw_rtpqos->rtp_info.first_packet = pEntry->rtp_info.first_packet;
	hw_rtpqos->rtp_info.probation = pEntry->rtp_info.probation;
	hw_rtpqos->rtp_info.mode = cpu_to_be16(pEntry->rtp_info.mode);
	hw_rtpqos->slot = pEntry->slot;

	//in multicast unset ports if auto mode is enabled
	if((pEntry->rtp_info.mode == 1) && ((pEntry->stream_type == MC4) || (pEntry->stream_type == MC6)))
	{
		hw_rtpqos->stats.sport = 0xFFFF;
		hw_rtpqos->stats.dport = 0xFFFF;
	}
	else
	{
		hw_rtpqos->stats.sport = pEntry->sport;
		hw_rtpqos->stats.dport = pEntry->dport;
	}

	hw_entry_set_flags(&hw_rtpqos->flags, RTPQOS_VALID);
}


int rtpqos_add_entry(PRTPQOS_ENTRY pEntry)
{
	struct _thw_rtpqos_entry *hw_entry;
	int rc;
	dma_addr_t dma_addr;

	/* Allocate hardware entry */
	hw_entry = dma_pool_alloc(rtp_dma_pool, GFP_ATOMIC, &dma_addr);
	if (!hw_entry)
	{
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err;
	}

	hw_entry->dma_addr = cpu_to_be32(dma_addr);
	memset((U8*)&hw_entry->rtp_info, 0, sizeof(RTPinfo));
	memset((U8*)&hw_entry->stats, 0, sizeof(RTCPStats));

	/* Link software conntrack to hardware conntrack */
	pEntry->hw_rtpqos = hw_entry;
	hw_entry->sw_rtpqos = pEntry;

	/* fill in the hw entry */
	rtpqos_update_entry(pEntry);

	/* Update PE's internal memory cache tables with the HW entry's DDR address */
	rtpqos_update_pe_cache(hw_entry->dma_addr, pEntry->slot);

	return NO_ERR;

err:
	memset((U8*)&pEntry->rtp_info, 0, sizeof(RTPinfo));
	memset((U8*)&pEntry->stats, 0, sizeof(RTCPStats));
	pEntry->stream_id = RTP_STATS_FREE;
	pEntry->stream_type = 0;

	return rc;
}


static void hw_rtpqos_schedule_remove(struct _thw_rtpqos_entry *hw_rtpqos)
{
	hw_rtpqos->removal_time = jiffies + 2;

	dlist_add(&hw_rtpqos_removal_list, &hw_rtpqos->list);
}


static void hw_rtpqos_delayed_remove(void)
{
	struct _thw_rtpqos_entry *hw_rtpqos;
	struct dlist_head *entry;

	dlist_for_each_safe(hw_rtpqos, entry, &hw_rtpqos_removal_list, list)
	{
		if (!time_after(jiffies, hw_rtpqos->removal_time))
			continue;

		dlist_remove(&hw_rtpqos->list);

		dma_pool_free(rtp_dma_pool, hw_rtpqos, be32_to_cpu(hw_rtpqos->dma_addr));
	}
}

void rtpqos_remove_entry(PRTPQOS_ENTRY pEntry)
{
	struct _thw_rtpqos_entry *hw_entry;

	/* Check if there is a hardware socket */
	if ((hw_entry = pEntry->hw_rtpqos))
	{
		/* detach from software socket */
		pEntry->hw_rtpqos = NULL;

		/* mark this slot as  empty in PE dmem */
		rtpqos_update_pe_cache(hw_entry_get_field(&hw_entry->next), pEntry->slot);

		/* now switching hw entry from active to delayed removal list */
		hw_rtpqos_schedule_remove(hw_entry);
	}

	rtpqos_free_entry(pEntry);
}

#endif

static int rtpqos_check_entry(U16 stream_id, U32 *saddr, U32 *daddr, U16 sport, U16 dport, U8 family)
{
	PRTPQOS_ENTRY pEntry = NULL;
	int i;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		pEntry = &rtpqos_cache[i];

		if(pEntry)
		{
			if(stream_id == pEntry->stream_id)
				return ERR_RTP_STATS_STREAMID_ALREADY_USED;

			if(family == IP4) {
				if((pEntry->stream_id != RTP_STATS_FREE) && (saddr[0] == pEntry->saddr[0]) && (daddr[0] == pEntry->daddr[0]) && (sport == pEntry->sport) && (dport == pEntry->dport))
					return ERR_RTP_STATS_DUPLICATED;

			} else {
				if((pEntry->stream_id != RTP_STATS_FREE) && !IPV6_CMP(saddr, pEntry->saddr) && !IPV6_CMP(daddr, pEntry->daddr) && (sport == pEntry->sport) && (dport == pEntry->dport))
					return ERR_RTP_STATS_DUPLICATED;
			}
		}
	}

	return ERR_RTP_STATS_STREAMID_UNKNOWN;
}


static PRTPQOS_ENTRY rtpqos_get_entry_by_id(U16 stream_id)
{
	int i;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		if(stream_id == rtpqos_cache[i].stream_id)
			return &rtpqos_cache[i];
	}
	return NULL;
}


PRTPflow RTP_find_flow(U16 id)
{
	PRTPflow flow_entry;
	struct slist_entry *entry;
	U32 hash_key;

	hash_key = HASH_RTP(id);
	slist_for_each(flow_entry, entry, &rtpflow_cache[hash_key], list)
	{
		if (id && (flow_entry->ingress_socketID == id))
			return flow_entry;
	}

	return NULL;
}


static int RTPQOS_enable_stats(U16 *p, U16 Length)
{
	RTP_ENABLE_STATS_COMMAND cmd;
	PCtEntry pCT_entry = NULL;
	PCtEntryIPv6 pCT6_entry = NULL;
	PMC4Entry pMC_entry = NULL;
	PMC6Entry pMC6_entry = NULL;
	PSockEntry pSocket = NULL;
	PSock6Entry pSocket6 = NULL;
	PRTPQOS_ENTRY pStatEntry = NULL;
	int check_status = NO_ERR;
	U8 ip_family;

	//check length
	if (Length != sizeof(RTP_ENABLE_STATS_COMMAND))
		return ERR_WRONG_COMMAND_SIZE;

	memset((U8*)&cmd, 0, sizeof(RTP_ENABLE_STATS_COMMAND));

	// Ensure alignment
	SFL_memcpy((U8*)&cmd, (U8*)p, sizeof(RTP_ENABLE_STATS_COMMAND));

	//0xFFFF is a reserved value and connot be used as a stream ID
	if(cmd.stream_id == RTP_STATS_FREE)
		return ERR_WRONG_COMMAND_PARAM;

	//check all possible error cases before marking the connection

	if((cmd.stream_type == IP4) || (cmd.stream_type == MC4) ||(cmd.stream_type == RLY))
		ip_family = IP4;
	else if((cmd.stream_type == IP6) || (cmd.stream_type == MC6) ||(cmd.stream_type == RLY6))
		ip_family = IP6;
	else
		return ERR_RTP_STATS_WRONG_TYPE;

	if((check_status = rtpqos_check_entry(cmd.stream_id, cmd.saddr, cmd.daddr, cmd.sport, cmd.dport, ip_family)) != ERR_RTP_STATS_STREAMID_UNKNOWN)
		return check_status;

	pStatEntry = rtpqos_alloc_entry(cmd.stream_id);
	if(pStatEntry == NULL)
		return ERR_RTP_STATS_MAX_ENTRIES;

	//find corresponding CT or MC entry, if exists
	switch(cmd.stream_type)
	{
		case IP4:
			//auto mode not supported for ipv4
			cmd.mode = 0;
			if((pCT_entry = IPv4_get_ctentry(cmd.saddr[0], cmd.daddr[0], cmd.sport, cmd.dport, cmd.proto)) != NULL)
			{
				pCT_entry->status |= CONNTRACK_RTP_STATS;
				pCT_entry->rtpqos_slot = pStatEntry->slot;
			#if defined (COMCERTO_2000)
				pCT_entry->ct->status = cpu_to_be16(pCT_entry->status);
				pCT_entry->ct->rtpqos_slot = pCT_entry->rtpqos_slot;
			#endif
			}
			break;

		case IP6:
			//auto mode not supported for ipv6
			cmd.mode = 0;
			if((pCT6_entry = IPv6_get_ctentry(cmd.saddr, cmd.daddr, cmd.sport, cmd.dport, cmd.proto)) != NULL)
			{
				pCT6_entry->status |= CONNTRACK_RTP_STATS;
				pCT6_entry->rtpqos_slot = pStatEntry->slot;
			#if defined (COMCERTO_2000)
				pCT6_entry->ct->status = cpu_to_be16(pCT6_entry->status);
				pCT6_entry->ct->rtpqos_slot = pCT6_entry->rtpqos_slot;
			#endif
			}
			break;

		case MC4:
			if((pMC_entry = MC4_rule_search(cmd.saddr[0], cmd.daddr[0])) != NULL)
			{
				pMC_entry->status |= CONNTRACK_RTP_STATS;
				pMC_entry->rtpqos_slot = pStatEntry->slot;
				pMC_entry->rtpqos_ref_count++;
			#if defined (COMCERTO_2000)
				MC4_entry_update(pMC_entry);
			#endif
			}
			break;

		case MC6:
			if((pMC6_entry = MC6_rule_search(cmd.saddr, cmd.daddr)) != NULL)
			{
				pMC6_entry->status |= CONNTRACK_RTP_STATS;
				pMC6_entry->rtpqos_slot = pStatEntry->slot;
				pMC6_entry->rtpqos_ref_count++;
			#if defined (COMCERTO_2000)
				MC6_entry_update(pMC6_entry);
			#endif
			}
			break;

		case RLY:
			pSocket = SOCKET4_find_entry(cmd.saddr[0], cmd.sport, cmd.daddr[0], cmd.dport, cmd.proto);
			if(pSocket != NULL)
			{
				pSocket->qos_enable = TRUE;
				pSocket->rtpqos_slot = pStatEntry->slot;
			#if defined (COMCERTO_2000)
				socket4_update(pSocket, SOCKET_UPDATE);
			#endif
			}
			break;

		case RLY6:
			pSocket6 = SOCKET6_find_entry(cmd.saddr, cmd.sport, cmd.daddr, cmd.dport, cmd.proto);
			if(pSocket6 != NULL)
			{
				pSocket6->qos_enable = TRUE;
				pSocket6->rtpqos_slot = pStatEntry->slot;
			#if defined (COMCERTO_2000)
				socket6_update(pSocket6, SOCKET_UPDATE);
			#endif
			}
			break;

		default:
			return ERR_RTP_STATS_WRONG_TYPE;
	}

	SFL_memcpy(pStatEntry->saddr, cmd.saddr, 4*sizeof(U32)); SFL_memcpy(pStatEntry->daddr, cmd.daddr, 4*sizeof(U32));
	pStatEntry->sport = cmd.sport;  pStatEntry->dport = cmd.dport;
	pStatEntry->proto = cmd.proto;

	pStatEntry->stream_type = cmd.stream_type;
	pStatEntry->rtp_info.first_packet = TRUE;
	pStatEntry->rtp_info.probation = RTP_MIN_SEQUENTIAL;
	pStatEntry->rtp_info.mode = cmd.mode;

	//in multicast unset ports if auto mode is enabled
	if((cmd.mode == 1) && ((cmd.stream_type == MC4) || (cmd.stream_type == MC6)))
	{
		pStatEntry->stats.sport = 0xFFFF;
		pStatEntry->stats.dport = 0xFFFF;
	}
	else
	{
		pStatEntry->stats.sport = cmd.sport;
		pStatEntry->stats.dport = cmd.dport;
	}

	/* Now adding hardware rtp qos stats entry to packet engine's cache */
	if(rtpqos_add_entry(pStatEntry) != NO_ERR)
		return ERR_NOT_ENOUGH_MEMORY;

	return NO_ERR;
}


static int RTPQOS_disable_stats(U16 *p, U16 Length)
{
	PRTPQOS_ENTRY pRTPQos_entry = NULL;
	PCtEntry pCT_entry = NULL;
	PCtEntryIPv6 pCT6_entry = NULL;
	PMC4Entry pMC_entry = NULL;
	PMC6Entry pMC6_entry = NULL;
	PSockEntry pSocket = NULL;
	PSock6Entry pSocket6 = NULL;
	U32 saddr[4];
	U32 daddr[4];
	U16 sport, dport, proto, stream_id;

	//check length
	if (Length != sizeof(RTP_DISABLE_STATS_COMMAND))
		return ERR_WRONG_COMMAND_SIZE;

	stream_id = p[0];

	if((pRTPQos_entry = rtpqos_get_entry_by_id(stream_id)) == NULL)
		return ERR_RTP_STATS_STREAMID_UNKNOWN;

	SFL_memcpy(saddr, pRTPQos_entry->saddr, 4*sizeof(U32)); SFL_memcpy(daddr, pRTPQos_entry->daddr, 4*sizeof(U32));
	sport = pRTPQos_entry->sport; dport = pRTPQos_entry->dport;
	proto = pRTPQos_entry->proto;

	switch(pRTPQos_entry->stream_type)
	{
		case IP4:
			//find corresponding CT or MC entry, if exists
			if((pCT_entry = IPv4_get_ctentry(saddr[0], daddr[0], sport, dport, proto)) == NULL)
				goto reset_slot;
			//set  CT or MC marker for per packet first level processing
			pCT_entry->status &= ~ CONNTRACK_RTP_STATS;
		#if defined (COMCERTO_2000)
			pCT_entry->ct->status = cpu_to_be16(pCT_entry->status);
		#endif
			break;

		case IP6:
			if((pCT6_entry = IPv6_get_ctentry(saddr, daddr, sport, dport, proto)) == NULL)
				goto reset_slot;
			pCT6_entry->status &= ~ CONNTRACK_RTP_STATS;
		#if defined (COMCERTO_2000)
			pCT6_entry->ct->status =  cpu_to_be16(pCT6_entry->status);
		#endif	
			break;

		case MC4:
			if((pMC_entry = MC4_rule_search(saddr[0], daddr[0])) == NULL)
				goto reset_slot;
			if(pMC_entry->rtpqos_ref_count)
			{
				if(--pMC_entry->rtpqos_ref_count == 0)
					pMC_entry->status &= ~ CONNTRACK_RTP_STATS;
			}
		#if defined (COMCERTO_2000)
			pMC_entry->hw_entry->status = cpu_to_be16(pMC_entry->status);
		#endif
			break;

		case MC6:
			if((pMC6_entry = MC6_rule_search(saddr, daddr))== NULL)
				goto reset_slot;
			if(pMC6_entry->rtpqos_ref_count)
			{
				if(--pMC6_entry->rtpqos_ref_count == 0)
					pMC6_entry->status &= ~ CONNTRACK_RTP_STATS;
			}
		#if defined (COMCERTO_2000)
			pMC6_entry->hw_entry->status = cpu_to_be16(pMC6_entry->status);
		#endif
			break;

		case RLY:
			pSocket = SOCKET4_find_entry(saddr[0], sport, daddr[0], dport, proto);
			if(pSocket == NULL)
				goto reset_slot;
			pSocket->qos_enable = FALSE;
		#if defined (COMCERTO_2000)
			pSocket->hw_sock->qos_enable = pSocket->qos_enable;
		#endif
			break;

		case RLY6:
			pSocket6 = SOCKET6_find_entry(saddr, sport, daddr, dport, proto);
			if(pSocket6 == NULL)
				goto reset_slot;
			pSocket6->qos_enable = FALSE;
		#if defined (COMCERTO_2000)
			pSocket6->hw_sock->qos_enable = pSocket6->qos_enable;
		#endif
			break;
	}

reset_slot:
	rtpqos_remove_entry(pRTPQos_entry);

	return NO_ERR;
}


static U16 RTPQOS_query_stats (U16 *p, U16 Length)
{
	RTCPQueryCommand *pRTPCmd = (RTCPQueryCommand *)p;
	PRTPQOS_ENTRY pEntry = NULL;
	RTCPQueryResponse RTPRep;
	PRTCPStats pStats;
	U16 stream_id;

	// Check length
	if (Length != sizeof(RTP_QUERY_STATS_COMMAND))
		return ERR_WRONG_COMMAND_SIZE;

	stream_id = p[0];

	memset((U8*)&RTPRep, 0, sizeof(RTCPQueryResponse));

	if((pEntry = rtpqos_get_entry_by_id(stream_id)) == NULL)
		return ERR_RTP_STATS_STREAMID_UNKNOWN;

	pStats = RTCP_get_stats(&pEntry->stats, &pEntry->hw_rtpqos->stats, sizeof(RTCPStats));
	if(pStats == NULL)
		return ERR_RTP_STATS_NOT_AVAILABLE;

	/* check against null pStats pointer is done in the RTP_query_stats_common function */
	if(RTP_query_stats_common(&RTPRep, pStats))
		return ERR_RTP_STATS_STREAMID_UNKNOWN;

#if defined (COMCERTO_2000)
	pEntry->hw_rtpqos->stats.num_rx_since_RTCP = 0;
#endif

	if(pRTPCmd->flags)
		RTP_reset_stats((PRTCPStats)&pEntry->hw_rtpqos->stats, pRTPCmd->flags);

	SFL_memcpy((U8*)(p + 1), (U8*)&RTPRep, sizeof(RTCPQueryResponse));
	return NO_ERR;
}


static int rtp_set_dtmf_pt(U16 *p, U16 Length)
{
	U8 pt1, pt2;

	if (Length != sizeof(RTP_DTMF_PT_COMMAND))
		return ERR_WRONG_COMMAND_SIZE;

	pt1 = p[0] & 0x00FF;
	pt2 = (p[0] & 0xFF00) >> 8;

	if(pt2 == 0)
		pt2 = pt1;

	gDTMF_PT[0] = pt1; gDTMF_PT[1] = pt2;

#if defined(COMCERTO_2000)
	rtp_pt_add_to_pe(pt1, pt2);
#endif

	return NO_ERR;
}

static U16 M_rtp_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 retlen = 2;

	switch (cmd_code)
	{
	case CMD_RTP_OPEN:
		rc = RTP_Call_Open(pcmd, cmd_len);
		break;

	case CMD_RTP_UPDATE:
		rc = RTP_Call_Update(pcmd, cmd_len);
		break;

	case CMD_RTP_TAKEOVER:
		rc = RTP_Call_TakeOver(pcmd, cmd_len);
		break;

	case CMD_RTP_CONTROL:
		rc = RTP_Call_Control(pcmd, cmd_len);
		break;

	case CMD_RTP_CLOSE:
		rc = RTP_Call_Close(pcmd, cmd_len);
		break;

	case CMD_RTP_SPECTX_PLD:
		rc = RTP_Call_SpecialTx_Payload(pcmd, cmd_len);
		break;

	case CMD_RTP_SPECTX_CTRL:
		rc = RTP_Call_SpecialTx_Control(pcmd, cmd_len);
		break;

	case CMD_RTCP_QUERY:
		rc = RTCP_Query(pcmd, cmd_len);
		if (rc == NO_ERR)
			retlen += sizeof(RTCPQueryResponse);
		break;

	case CMD_RTP_STATS_ENABLE:
		rc = RTPQOS_enable_stats(pcmd, cmd_len);
		break;	

	case CMD_RTP_STATS_DISABLE:
		rc = RTPQOS_disable_stats(pcmd, cmd_len);
		break;	

	case CMD_RTP_STATS_QUERY:
		rc = RTPQOS_query_stats(pcmd, cmd_len);
		if (rc == NO_ERR)
			retlen += sizeof(RTCPQueryResponse);
		break;	

	case CMD_RTP_STATS_DTMF_PT:
		rc = rtp_set_dtmf_pt(pcmd, cmd_len);
		break;	

	case CMD_VOICE_BUFFER_LOAD:
		rc = voice_buffer_command_load(pcmd, cmd_len);
		break;

	case CMD_VOICE_BUFFER_UNLOAD:
		rc = voice_buffer_command_unload(pcmd, cmd_len);
		break;

	case CMD_VOICE_BUFFER_START:
		rc = voice_buffer_command_start(pcmd, cmd_len);
		break;

	case CMD_VOICE_BUFFER_STOP:
		rc = voice_buffer_command_stop(pcmd, cmd_len);
		break;

	case CMD_VOICE_BUFFER_RESET:
		rc = voice_buffer_command_reset(pcmd, cmd_len);
		break;
	default:
		rc = ERR_UNKNOWN_COMMAND;
		break;
	}

	*pcmd = rc;
	return retlen;
}


/* link a CT4 entry to a RTP statistics slot */
int rtpqos_ipv4_link_stats_entry_by_tuple(PCtEntry pCT, U32 saddr, U32 daddr, U16 sport, U16 dport)
{
	int i;
	PRTPQOS_ENTRY pEntry;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		pEntry = &rtpqos_cache[i];

		if(pEntry->stream_id != RTP_STATS_FREE)
		{
			if((saddr == pEntry->saddr[0]) && (daddr == pEntry->daddr[0]) && (sport == pEntry->sport) && (dport == pEntry->dport))
			{
				pCT->status |= CONNTRACK_RTP_STATS;
				pCT->rtpqos_slot = pEntry->slot;
			#if defined (COMCERTO_2000)
				pCT->ct->status = cpu_to_be16(pCT->status);
				pCT->ct->rtpqos_slot = pCT->rtpqos_slot;
			#endif

				return ERR_RTP_STATS_DUPLICATED;
			}
		}
	}

	return ERR_RTP_STATS_STREAMID_UNKNOWN;
}


/* link a CT6 entry to a RTP statistics slot */
int rtpqos_ipv6_link_stats_entry_by_tuple(PCtEntryIPv6 pCT, U32 *saddr, U32 *daddr, U16 sport, U16 dport)
{
	int i;
	PRTPQOS_ENTRY pEntry;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		pEntry = &rtpqos_cache[i];

		if(pEntry->stream_id != RTP_STATS_FREE)
		{
			if(!IPV6_CMP(saddr, pEntry->saddr) && !IPV6_CMP(daddr, pEntry->daddr) && (sport == pEntry->sport) && (dport == pEntry->dport))
			{
				pCT->status |= CONNTRACK_RTP_STATS;
				pCT->rtpqos_slot = pEntry->slot;
			#if defined (COMCERTO_2000)
				pCT->ct->status = cpu_to_be16(pCT->status);
				pCT->ct->rtpqos_slot = pCT->rtpqos_slot;
			#endif

				return ERR_RTP_STATS_DUPLICATED;
			}
		}
	}
	return ERR_RTP_STATS_STREAMID_UNKNOWN;
}


/* link a MC4 entry to a RTP statistics slot */
int rtpqos_mc4_link_stats_entry_by_tuple(PMC4Entry pMC, U32 saddr, U32 daddr)
{
	int i;
	PRTPQOS_ENTRY pEntry;
	
	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		pEntry = &rtpqos_cache[i];

		if(pEntry->stream_id != RTP_STATS_FREE)
		{
			if((saddr == pEntry->saddr[0]) && (daddr == pEntry->daddr[0]))
			{
				pMC->status |= CONNTRACK_RTP_STATS;
				pMC->rtpqos_slot = pEntry->slot;
				pMC->rtpqos_ref_count++;

				return ERR_RTP_STATS_DUPLICATED;
			}
		}
	}

	return ERR_RTP_STATS_STREAMID_UNKNOWN;
}


/* link a MC6 entry to a RTP statistics slot */
int rtpqos_mc6_link_stats_entry_by_tuple(PMC6Entry pMC, U32 *saddr, U32 *daddr)
{
	int i;
	PRTPQOS_ENTRY pEntry;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		pEntry = &rtpqos_cache[i];

		if(pEntry->stream_id != RTP_STATS_FREE)
		{
			if (!IPV6_CMP(saddr, pEntry->saddr) && !IPV6_CMP(daddr, pEntry->daddr)) 
			{
				pMC->status |= CONNTRACK_RTP_STATS;
				pMC->rtpqos_slot = pEntry->slot;
				pMC->rtpqos_ref_count++;

				return ERR_RTP_STATS_DUPLICATED;
			}
		}
	}

	return ERR_RTP_STATS_STREAMID_UNKNOWN;
}

/* link a socket entry to a RTP statistics slot */
int rtpqos_relay_link_stats_entry_by_tuple(PSockEntry pSocket, U32 saddr, U32 daddr, U16 sport, U16 dport)
{
	int i;
	PRTPQOS_ENTRY pEntry = NULL;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		pEntry = &rtpqos_cache[i];

		if(pEntry->stream_id != RTP_STATS_FREE)
		{
			if((saddr == pEntry->saddr[0]) && (daddr == pEntry->daddr[0]) && (sport == pEntry->sport) && (dport == pEntry->dport))
			{
				pSocket->qos_enable = TRUE;
				pSocket->rtpqos_slot = pEntry->slot;

				return ERR_RTP_STATS_DUPLICATED;
			}
		}
	}

	return ERR_RTP_STATS_STREAMID_UNKNOWN;
}


/* link a socket6 entry to a RTP statistics slot */
int rtpqos_relay6_link_stats_entry_by_tuple(PSock6Entry pSocket, U32 *saddr, U32 *daddr, U16 sport, U16 dport)
{
	int i;
	PRTPQOS_ENTRY pEntry = NULL;

	for (i = 0; i < MAX_RTP_STATS_ENTRY; i++)
	{
		pEntry = &rtpqos_cache[i];

		if(pEntry->stream_id != RTP_STATS_FREE)
		{
			if(!IPV6_CMP(saddr, pEntry->saddr) && !IPV6_CMP(daddr, pEntry->daddr) && (sport == pEntry->sport) && (dport == pEntry->dport))
			{
				pSocket->qos_enable = TRUE;
				pSocket->rtpqos_slot = pEntry->slot;

				return ERR_RTP_STATS_DUPLICATED;
			}
		}
	}
	return ERR_RTP_STATS_STREAMID_UNKNOWN;
}

BOOL rtp_relay_init(void)
{
	int i;
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	rtp_dma_pool = ctrl->dma_pool_512;
#endif

#if !defined(COMCERTO_2000)
	set_event_handler(EVENT_RTP_RELAY, M_rtp_entry);
#endif

	set_cmd_handler(EVENT_RTP_RELAY, M_rtp_cmdproc);

	gDTMF_PT[0] = 96;
	gDTMF_PT[1] = 97;
#if defined(COMCERTO_2000)
	rtp_pt_add_to_pe(gDTMF_PT[0], gDTMF_PT[1]);
#endif

	for (i = 0; i < NUM_RTPFLOW_ENTRIES; i++)
		slist_head_init(&rtpflow_cache[i]);

	slist_head_init(&rtpcall_list);

#if defined(COMCERTO_2000)
	for (i = 0; i < NUM_RTPFLOW_ENTRIES; i++)
		dlist_head_init(&hw_flow_active_list[i]);

	dlist_head_init(&hw_flow_removal_list);

	timer_init(&rtpflow_timer, hw_rtp_flow_delayed_remove);
	timer_add(&rtpflow_timer, CT_TIMER_INTERVAL);

#endif
	voice_buffer_init();

	/* RTP QOS Measurement */

#if defined(COMCERTO_2000)
	dlist_head_init(&hw_rtpqos_removal_list);

	timer_init(&rtpqos_timer, hw_rtpqos_delayed_remove);
	timer_add(&rtpqos_timer, CT_TIMER_INTERVAL);
#endif

	/* mark all rtp stats entry as unused */
	for(i = 0; i < MAX_RTP_STATS_ENTRY; i++) {
		rtpqos_cache[i].stream_id = RTP_STATS_FREE;
		rtpqos_cache[i].slot = i;
	}

    return 0;
}


void rtp_relay_exit(void)
{
#if defined(COMCERTO_2000)
	struct dlist_head *entry;
	struct _thw_rtpflow *hw_flow;
	struct _thw_rtpqos_entry *hw_rtpqos;

	voice_buffer_exit();

	timer_del(&rtpflow_timer);

	rtp_flow_reset();
	rtp_call_reset();

	dlist_for_each_safe(hw_flow, entry, &hw_flow_removal_list, list)
	{
		dlist_remove(&hw_flow->list);
		dma_pool_free(rtp_dma_pool, hw_flow, be32_to_cpu(hw_flow->dma_addr));
	}

	/* RTP QOS Measurement */
	dlist_for_each_safe(hw_rtpqos, entry, &hw_rtpqos_removal_list, list)
	{
		dlist_remove(&hw_rtpqos->list);
		dma_pool_free(rtp_dma_pool, hw_rtpqos, be32_to_cpu(hw_rtpqos->dma_addr));
	}
#endif
}

