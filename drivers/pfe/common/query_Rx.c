/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
*/
#include "module_ethernet.h"
#include "fe.h"
#include "module_Rx.h"
#include "module_stat.h"
#include "layer2.h"
#include "module_timer.h"
#include "module_bridge.h"

#if !defined (COMCERTO_2000)
PVOID bridge_snapshot_alloc(U32 size)
{
	return Heap_Alloc(size);
}

void bridge_snapshot_free(PVOID p)
{
	Heap_Free(p);
}

#else

extern U32 L2Bridge_timeout;

PVOID bridge_snapshot_alloc(U32 size)
{
	return pfe_kzalloc(size, GFP_KERNEL);
}

void bridge_snapshot_free(PVOID p)
{
	pfe_kfree(p);
}
#endif


/* This function returns total bridge entries configured 
   in a given hash index */

static int rx_Get_Hash_BridgeEntries(int bridge_hash_index)
{
	
	int tot_bridge_entries = 0;
	PL2Bridge_entry pEntry;
	struct slist_entry *entry;

	slist_for_each(pEntry, entry, &bridge_cache[bridge_hash_index], list)
	{
		tot_bridge_entries++;
	}
		
	return tot_bridge_entries;
}

/* This function fills the snapshot of bridge entries 
   in a given hash index */

static int rx_Bridge_Get_Hash_Snapshot(int bt_hash_index, int br_tot_entries, PL2BridgeQueryEntryResponse pBridgeSnapshot)
{
	int tot_bridge_entries=0;
	PL2Bridge_entry pBridgeEntry;
	struct slist_entry *entry;

	slist_for_each(pBridgeEntry, entry, &bridge_cache[bt_hash_index], list) 
	{
		if(br_tot_entries > 0)
		{
			pBridgeSnapshot->input_interface  = pBridgeEntry->input_interface;
			pBridgeSnapshot->input_svlan       = htons(pBridgeEntry->input_svlan);
			pBridgeSnapshot->input_cvlan       = htons(pBridgeEntry->input_cvlan);
			if(pBridgeEntry->output_itf)
				pBridgeSnapshot->output_interface  = pBridgeEntry->output_itf->index;
			else
				pBridgeSnapshot->output_interface = 0xFF;
			pBridgeSnapshot->output_svlan       = htons(pBridgeEntry->output_svlan);
			pBridgeSnapshot->output_cvlan       = htons(pBridgeEntry->output_cvlan);
			SFL_memcpy(pBridgeSnapshot->destaddr, pBridgeEntry->da, 2 * ETHER_ADDR_LEN);
			pBridgeSnapshot->ethertype       = htons(pBridgeEntry->ethertype);
			pBridgeSnapshot->pkt_priority    = pBridgeEntry->pkt_priority == 0xFF ? 0x8000 : pBridgeEntry->pkt_priority;
			pBridgeSnapshot->svlan_priority = pBridgeEntry->svlan_priority_mark ? 0x8000 : pBridgeEntry->svlan_priority;
			pBridgeSnapshot->cvlan_priority = pBridgeEntry->cvlan_priority_mark ? 0x8000 : pBridgeEntry->cvlan_priority;
			pBridgeSnapshot->session_id = htons(pBridgeEntry->session_id);
			pBridgeSnapshot->qmod = pBridgeEntry->qmod;
			SFL_memcpy(pBridgeSnapshot->input_name, get_onif_name(phy_port[pBridgeEntry->input_interface].itf.index), 16);
			if(pBridgeEntry->output_itf)
				SFL_memcpy(pBridgeSnapshot->output_name, get_onif_name(pBridgeEntry->output_itf->index), 16);
			else
				pBridgeSnapshot->output_name[0] = '\0';
		
			pBridgeSnapshot++;
			tot_bridge_entries++;
			br_tot_entries--;
		}
	}
		
	return tot_bridge_entries;

}

/* This function creates the snapshot memory and returns the 
   next bridge entry from the snapshop of the bridge entries of a
   single hash to the caller  */

int rx_Get_Next_Hash_BridgeEntry(PL2BridgeQueryEntryResponse pBridgeCmd, int reset_action)
{
	int bridge_hash_entries;
	PL2BridgeQueryEntryResponse pBridge;
	static PL2BridgeQueryEntryResponse pBridgeSnapshot = NULL;
	static int bridge_hash_index = 0, bridge_snapshot_entries =0, bridge_snapshot_index=0, bridge_snapshot_buf_entries = 0;
	
	if(reset_action)
	{
		bridge_hash_index = 0;
		bridge_snapshot_entries = 0;
		bridge_snapshot_index = 0;
		if(pBridgeSnapshot)
		{
			bridge_snapshot_free(pBridgeSnapshot);
			pBridgeSnapshot = NULL;
		}
		bridge_snapshot_buf_entries = 0;
		return NO_ERR;
	}
	
	if (bridge_snapshot_index == 0)
	{
		while( bridge_hash_index < NUM_BT_ENTRIES)
		{
		
			bridge_hash_entries = rx_Get_Hash_BridgeEntries(bridge_hash_index);
			if(bridge_hash_entries == 0)
			{
				bridge_hash_index++;
				continue;
			}
		   	
		   	if(bridge_hash_entries > bridge_snapshot_buf_entries)
		   	{
		   		if(pBridgeSnapshot)	
		   			bridge_snapshot_free(pBridgeSnapshot);
		   
				pBridgeSnapshot = bridge_snapshot_alloc(bridge_hash_entries * sizeof(L2BridgeQueryEntryResponse));
			
				if (!pBridgeSnapshot)
				{
			    	bridge_hash_index = 0;
			    	bridge_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				bridge_snapshot_buf_entries = bridge_hash_entries;
		   	}
		
			bridge_snapshot_entries = rx_Bridge_Get_Hash_Snapshot(bridge_hash_index , bridge_hash_entries, pBridgeSnapshot);
	
			break;
		}
		
		if (bridge_hash_index >= NUM_BT_ENTRIES)
		{
			bridge_hash_index = 0;
			if(pBridgeSnapshot)
			{
				bridge_snapshot_free(pBridgeSnapshot);
				pBridgeSnapshot = NULL;
			}
			bridge_snapshot_buf_entries = 0;
			return ERR_BRIDGE_ENTRY_NOT_FOUND;
		}
		   
	}
	
	pBridge = &pBridgeSnapshot[bridge_snapshot_index++];
	SFL_memcpy(pBridgeCmd, pBridge, sizeof(L2BridgeQueryEntryResponse));
	if (bridge_snapshot_index == bridge_snapshot_entries)
	{
		bridge_snapshot_index = 0;
		bridge_hash_index ++;
	}
		
	
	return NO_ERR;	
		
}



#ifdef CFG_STATS
/* This function fills the snapshot of bridge entries 
   in a given hash index */

static int stat_Bridge_Get_Hash_Snapshot(int stat_bt_hash_index, int stat_tot_br_entries, PStatBridgeEntryResponse pStatBridgeSnapshot)
{
	int stat_bridge_entries=0;
	PL2Bridge_entry pStatBridgeEntry;
	struct slist_entry *entry;
	
	slist_for_each(pStatBridgeEntry, entry, &bridge_cache[stat_bt_hash_index], list)
	{
		if(stat_tot_br_entries > 0)
		{
			pStatBridgeSnapshot->eof = 0;
			pStatBridgeSnapshot->input_interface = pStatBridgeEntry->input_interface;
			pStatBridgeSnapshot->input_svlan = htons(pStatBridgeEntry->input_svlan);
			pStatBridgeSnapshot->input_cvlan = htons(pStatBridgeEntry->input_cvlan);
			if(pStatBridgeEntry->output_itf)
				pStatBridgeSnapshot->output_interface = pStatBridgeEntry->output_itf->index;
			else
				pStatBridgeSnapshot->output_interface = 0xFF;
			pStatBridgeSnapshot->output_svlan = htons(pStatBridgeEntry->output_svlan);
			pStatBridgeSnapshot->output_cvlan = htons(pStatBridgeEntry->output_cvlan);
			SFL_memcpy(pStatBridgeSnapshot->dst_mac, pStatBridgeEntry->da, 2 * ETHER_ADDR_LEN);
			pStatBridgeSnapshot->etherType = htons(pStatBridgeEntry->ethertype);
			pStatBridgeSnapshot->session_id = htons(pStatBridgeEntry->session_id);
#if !defined (COMCERTO_2000)
			pStatBridgeSnapshot->total_packets_transmitted = pStatBridgeEntry->total_packets_transmitted;
#else
			{
				struct _thw_L2Bridge_entry *hw_l2bridge = pStatBridgeEntry->hw_l2bridge;
				class_statistics_get_ddr(hw_l2bridge->total_packets_transmitted,
						&pStatBridgeSnapshot->total_packets_transmitted,
						sizeof(pStatBridgeSnapshot->total_packets_transmitted),
						gStatBridgeQueryStatus & STAT_BRIDGE_QUERY_RESET);
			}
#endif

			SFL_memcpy(pStatBridgeSnapshot->input_name, get_onif_name(phy_port[pStatBridgeEntry->input_interface].itf.index), 16);
			if(pStatBridgeEntry->output_itf)
				SFL_memcpy(pStatBridgeSnapshot->output_name, get_onif_name(pStatBridgeEntry->output_itf->index), 16);
			else
				pStatBridgeSnapshot->output_name[0] = '\0';
		
#if !defined (COMCERTO_2000)
			if(gStatBridgeQueryStatus & STAT_BRIDGE_QUERY_RESET)
				pStatBridgeEntry->total_packets_transmitted = 0;
#endif

			pStatBridgeSnapshot++;
			stat_bridge_entries++;
			stat_tot_br_entries--;
		}
	}
		
	return stat_bridge_entries;

}

/* This function creates the snapshot memory and returns the 
   next bridge entry from the snapshop of the bridge entries of a
   single hash to the caller  */

int rx_Get_Next_Hash_Stat_BridgeEntry(PStatBridgeEntryResponse pStatBridgeCmd, int reset_action)
{
	int stat_bridge_hash_entries;
	PStatBridgeEntryResponse pStatBridge;
	static PStatBridgeEntryResponse pStatBridgeSnapshot = NULL;
	static int stat_bridge_hash_index = 0, stat_bridge_snapshot_entries =0, stat_bridge_snapshot_index=0, stat_bridge_snapshot_buf_entries = 0;
	
	if(reset_action)
	{
		stat_bridge_hash_index = 0;
		stat_bridge_snapshot_entries = 0;
		stat_bridge_snapshot_index	=	0;
		if(pStatBridgeSnapshot)
		{
			bridge_snapshot_free(pStatBridgeSnapshot);
			pStatBridgeSnapshot = NULL;
		}
		stat_bridge_snapshot_buf_entries = 0;
		return NO_ERR;
	}
	
	if (stat_bridge_snapshot_index == 0)
	{
		while( stat_bridge_hash_index < NUM_BT_ENTRIES)
		{
		
			stat_bridge_hash_entries = rx_Get_Hash_BridgeEntries(stat_bridge_hash_index);
			if(stat_bridge_hash_entries == 0)
			{
				stat_bridge_hash_index++;
				continue;
			}
		   	
		   	if(stat_bridge_hash_entries > stat_bridge_snapshot_buf_entries)
		   	{
		   		if(pStatBridgeSnapshot)
		   			bridge_snapshot_free(pStatBridgeSnapshot);
		   		pStatBridgeSnapshot = bridge_snapshot_alloc(stat_bridge_hash_entries * sizeof(StatBridgeEntryResponse));
			
				if (!pStatBridgeSnapshot)
				{
			    	stat_bridge_hash_index = 0;
			    	stat_bridge_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				stat_bridge_snapshot_buf_entries = 	stat_bridge_hash_entries;
		   	}
			stat_bridge_snapshot_entries = stat_Bridge_Get_Hash_Snapshot(stat_bridge_hash_index ,stat_bridge_hash_entries, pStatBridgeSnapshot);
		
			break;
		}
		
		if (stat_bridge_hash_index >= NUM_BT_ENTRIES)
		{
			stat_bridge_hash_index = 0;
			if(pStatBridgeSnapshot)
			{
				bridge_snapshot_free(pStatBridgeSnapshot);
				pStatBridgeSnapshot = NULL;
			}
			stat_bridge_snapshot_buf_entries = 0;
			return ERR_BRIDGE_ENTRY_NOT_FOUND;
		}
		   
	}
	
	pStatBridge = &pStatBridgeSnapshot[stat_bridge_snapshot_index++];
	SFL_memcpy(pStatBridgeCmd, pStatBridge, sizeof(StatBridgeEntryResponse));
	if (stat_bridge_snapshot_index == stat_bridge_snapshot_entries)
	{
		stat_bridge_snapshot_index = 0;
		stat_bridge_hash_index ++;
	}
		
	
	return NO_ERR;	
		
}
#endif /* CFG_STATS */

/* This function returns total L2Flow entries configured 
   in a given hash index */
static int rx_Get_Hash_L2FlowEntries(int L2Flow_hash_index)
{	
	int tot_bridge_entries = 0;
	PL2Flow_entry pL2FlowEntry;
	PL3Flow_entry pL3FlowEntry;
	struct slist_entry *entry;

	/* Hash index matches L2 Flow table*/
	if(L2Flow_hash_index < NUM_BT_ENTRIES)
	{
		/* Only process L2 only entries */
		slist_for_each(pL2FlowEntry, entry, &bridge_cache[L2Flow_hash_index], list)
		{
			if(!pL2FlowEntry->nb_l3_ref)
				tot_bridge_entries++;
		}
	}
	else
	{
		/* Index matches L3 table, process L3 entries */
		slist_for_each(pL3FlowEntry, entry, &bridge_l3_cache[L2Flow_hash_index -NUM_BT_ENTRIES], list)
		{
			tot_bridge_entries++;
		}
	}
	
	return tot_bridge_entries;
}

/* This function fills the snapshot of bridge entries 
   in a given hash index */
static int rx_Get_Hash_Snapshot_L2FlowEntries(int L2Flow_hash_index, int L2Flow_tot_entries, PL2BridgeL2FlowEntryCommand pL2FlowSnapshot)
{
	int tot_bridge_entries = 0;
	PL2Flow_entry pL2FlowEntry;
	PL3Flow_entry pL3FlowEntry;

	if(L2Flow_hash_index < NUM_BT_ENTRIES){
		pL2FlowEntry = container_of(slist_first(&bridge_cache[L2Flow_hash_index]), typeof(L2Flow_entry), list);
		pL3FlowEntry = NULL;
	}
	else{
		pL3FlowEntry = container_of(slist_first(&bridge_l3_cache[L2Flow_hash_index - NUM_BT_ENTRIES]), typeof(L3Flow_entry), list);
		pL2FlowEntry = pL3FlowEntry->l2flow_entry;
	}
		
	while ((pL2FlowEntry) && (L2Flow_tot_entries > 0)) {
		
		memset(pL2FlowSnapshot, 0 , sizeof(L2BridgeL2FlowEntryCommand));
		
		/* L2 info */
		SFL_memcpy(pL2FlowSnapshot->destaddr, pL2FlowEntry->l2flow.da, 2 * ETHER_ADDR_LEN);
		pL2FlowSnapshot->ethertype = pL2FlowEntry->l2flow.ethertype;
		pL2FlowSnapshot->svlan_tag = pL2FlowEntry->l2flow.svlan_tag;
		pL2FlowSnapshot->cvlan_tag = pL2FlowEntry->l2flow.cvlan_tag;
		pL2FlowSnapshot->session_id = pL2FlowEntry->l2flow.session_id;
		SFL_memcpy(pL2FlowSnapshot->input_name, get_onif_name(pL2FlowEntry->input_ifindex), INTERFACE_NAME_LENGTH);
		SFL_memcpy(pL2FlowSnapshot->output_name, get_onif_name(pL2FlowEntry->output_if->index), INTERFACE_NAME_LENGTH);
		
		/* L3-4 info */
		if(pL3FlowEntry){
			SFL_memcpy(pL2FlowSnapshot->saddr, pL3FlowEntry->l3flow.saddr, IPV6_ADDRESS_LENGTH);
			SFL_memcpy(pL2FlowSnapshot->daddr, pL3FlowEntry->l3flow.daddr, IPV6_ADDRESS_LENGTH);
			pL2FlowSnapshot->proto = pL3FlowEntry->l3flow.proto;
			pL2FlowSnapshot->sport = pL3FlowEntry->l3flow.sport;
			pL2FlowSnapshot->dport = pL3FlowEntry->l3flow.dport;
			pL2FlowSnapshot->timeout = (L2Bridge_timeout - (ct_timer - pL3FlowEntry->last_l3flow_timer)) / CT_TICKS_PER_SECOND;
		}
		else
			pL2FlowSnapshot->timeout = (L2Bridge_timeout - (ct_timer - pL2FlowEntry->last_l2flow_timer)) / CT_TICKS_PER_SECOND;
		
		/* Get next entry */
		if(pL3FlowEntry){
			pL3FlowEntry = container_of(slist_next(&pL3FlowEntry->list), typeof(L3Flow_entry), list);
			if(pL3FlowEntry)
				pL2FlowEntry = pL3FlowEntry->l2flow_entry;
			else
				pL2FlowEntry = NULL;
		}
		else
			pL2FlowEntry = container_of(slist_next(&pL2FlowEntry->list), typeof(L2Flow_entry), list);
		
		pL2FlowSnapshot++;
		tot_bridge_entries++;
		L2Flow_tot_entries--;
	}
		
	return tot_bridge_entries;

}

/* This function creates the snapshot memory and returns the 
   next bridge entry from the snapshop of the bridge entries of a
   single hash to the caller  
   Both L2 and L3 tables are linearly parsed */
int rx_Get_Next_Hash_L2FlowEntry(PL2BridgeL2FlowEntryCommand pL2FlowCmd, int reset_action)
{
	int L2Flow_hash_entries;
	static PL2BridgeL2FlowEntryCommand pL2FlowSnapshot = NULL;
	static int L2Flow_hash_index = 0, L2Flow_snapshot_entries = 0, 
		L2Flow_snapshot_index = 0, L2Flow_snapshot_buf_entries = 0;

	/* Reset */
	if(reset_action){
		L2Flow_hash_index = 0;
		L2Flow_snapshot_entries = 0;
		L2Flow_snapshot_index = 0;

		if(pL2FlowSnapshot){
			bridge_snapshot_free(pL2FlowSnapshot);
			pL2FlowSnapshot = NULL;
		}
		L2Flow_snapshot_buf_entries = 0;
	}
	
	if (L2Flow_snapshot_index == 0){
		while( L2Flow_hash_index < (NUM_BT_ENTRIES + NUM_BT_L3_ENTRIES)){

			/* Get next non-null bucket */
			L2Flow_hash_entries = rx_Get_Hash_L2FlowEntries(L2Flow_hash_index);
			if(L2Flow_hash_entries == 0){
				L2Flow_hash_index++;
				continue;
			}
			
		   	/* Alloc snapshot buffer if needed */
		   	if(L2Flow_hash_entries > L2Flow_snapshot_buf_entries){
		   		if(pL2FlowSnapshot)	
		   			bridge_snapshot_free(pL2FlowSnapshot);
		   
				pL2FlowSnapshot = bridge_snapshot_alloc(L2Flow_hash_entries * sizeof(L2BridgeL2FlowEntryCommand));
			
				if (!pL2FlowSnapshot){
					L2Flow_hash_index = 0;
					L2Flow_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				L2Flow_snapshot_buf_entries = L2Flow_hash_entries;
		   	}
		
			L2Flow_snapshot_entries = rx_Get_Hash_Snapshot_L2FlowEntries(L2Flow_hash_index , L2Flow_hash_entries, pL2FlowSnapshot);
	
			break;
		}
		
		/* No more entries */
		if (L2Flow_hash_index >= (NUM_BT_ENTRIES + NUM_BT_L3_ENTRIES)){
			L2Flow_hash_index = 0;
			if(pL2FlowSnapshot){
				bridge_snapshot_free(pL2FlowSnapshot);
				pL2FlowSnapshot = NULL;
			}
			L2Flow_snapshot_buf_entries = 0;
			return ERR_BRIDGE_ENTRY_NOT_FOUND;
		}
		   
	}
	
	SFL_memcpy(pL2FlowCmd, &pL2FlowSnapshot[L2Flow_snapshot_index++], sizeof(L2BridgeL2FlowEntryCommand));
	if (L2Flow_snapshot_index == L2Flow_snapshot_entries){
		L2Flow_snapshot_index = 0;
		L2Flow_hash_index ++;
	}
	return NO_ERR;		
}
