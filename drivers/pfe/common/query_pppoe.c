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
#include "layer2.h"
#include "module_pppoe.h"


/* This function returns total PPPoE configured in FPP */

static int PPPoE_Get_Hash_Sessions(int pppoe_hash_index)
{
	int tot_sessions=0;
	struct slist_entry *entry;
	
	slist_for_each_entry(entry, &pppoe_cache[pppoe_hash_index])
		tot_sessions++;

	return tot_sessions;

}

/* This function fills in the snapshot of all PPPoE Sessions 
   in a Session Table */

static int PPPoE_Get_Session_Snapshot(int pppoe_hash_index , int pppoe_tot_entries, pPPPoECommand pPPPoESnapshot)
{
	int tot_sessions=0;
	pPPPoE_Info pPPPoEEntry;
	struct slist_entry *entry;

	slist_for_each(pPPPoEEntry, entry, &pppoe_cache[pppoe_hash_index], list)
	{
		pPPPoESnapshot->sessionID   = ntohs(pPPPoEEntry->sessionID);
		COPY_MACADDR(pPPPoESnapshot->macAddr, pPPPoEEntry->DstMAC);
				
		if (!pPPPoEEntry->relay)
		{
			strcpy((char *)pPPPoESnapshot->phy_intf, get_onif_name(pPPPoEEntry->itf.phys->index));
			strcpy((char *)pPPPoESnapshot->log_intf, get_onif_name(pPPPoEEntry->itf.index));
		}
		else
		{
			strcpy((char *)pPPPoESnapshot->phy_intf, get_onif_name(pPPPoEEntry->itf.phys->index));
			strcpy((char *)pPPPoESnapshot->log_intf, "relay");
		}

		pPPPoESnapshot++;
		tot_sessions++;

		if (--pppoe_tot_entries <= 0)
			break;
	}
	
	return tot_sessions;
}

/* This function creates the snapshot memory and returns the 
   next PPPoE session entry from the PPPoE Session snapshot 
   to the caller  */
int PPPoE_Get_Next_SessionEntry(pPPPoECommand pSessionCmd, int reset_action)
{
	int pppoe_hash_entries;
	pPPPoECommand pSession;
	static pPPPoECommand pPPPoESnapshot = NULL;
	static int pppoe_session_hash_index =0, pppoe_snapshot_entries = 0, pppoe_snapshot_index = 0, pppoe_snapshot_buf_entries = 0;
	
	if(reset_action)
	{
		pppoe_session_hash_index =0;
		pppoe_snapshot_entries = 0;
		pppoe_snapshot_index = 0;
		if(pPPPoESnapshot)
		{
			Heap_Free(pPPPoESnapshot);
			pPPPoESnapshot = NULL;
		}
		pppoe_snapshot_buf_entries = 0;
	}
	    
    if (pppoe_snapshot_index == 0)
    {
       
       while( pppoe_session_hash_index <  NUM_PPPOE_ENTRIES)
       {
       	  pppoe_hash_entries = PPPoE_Get_Hash_Sessions(pppoe_session_hash_index);
       	  if (pppoe_hash_entries == 0)
       	  {
       	  	pppoe_session_hash_index++;
       	  	continue;
       	  }
       	  
       	  if(pppoe_hash_entries > pppoe_snapshot_buf_entries)
       	  {
       	  	if(pPPPoESnapshot)
       	  		Heap_Free(pPPPoESnapshot);
       	  	
       	  	pPPPoESnapshot = Heap_Alloc(pppoe_hash_entries * sizeof(PPPoECommand));
       	  	if (!pPPPoESnapshot)
       	  	{
       	  		pppoe_session_hash_index =0;
       	  		pppoe_snapshot_buf_entries = 0;
       	  		return ERR_NOT_ENOUGH_MEMORY;
       	  	}
       	  	pppoe_snapshot_buf_entries = pppoe_hash_entries;
       	  }
			
       	  pppoe_snapshot_entries = PPPoE_Get_Session_Snapshot(pppoe_session_hash_index,pppoe_hash_entries,pPPPoESnapshot);
       	  break;
        }
        if (pppoe_session_hash_index >= NUM_PPPOE_ENTRIES)
		{
			pppoe_session_hash_index = 0;
			if(pPPPoESnapshot)
			{
				Heap_Free(pPPPoESnapshot);
				pPPPoESnapshot = NULL;	
			}
			pppoe_snapshot_buf_entries = 0;
			return ERR_PPPOE_ENTRY_NOT_FOUND;
		}
 	
    }
    
   	pSession = &pPPPoESnapshot[pppoe_snapshot_index++];
   	
   	SFL_memcpy(pSessionCmd, pSession, sizeof(PPPoECommand));
	if (pppoe_snapshot_index == pppoe_snapshot_entries)
	{
	    pppoe_snapshot_index = 0;
	    pppoe_session_hash_index++;

	}
	
	return NO_ERR;
}

#ifdef CFG_STATS

#if defined(COMCERTO_2000)

void stat_pppoe_reset(pPPPoE_Info pEntry)
{
	U64 temp;
	class_statistics_get(&pEntry->total_packets_received, &temp, sizeof(pEntry->total_packets_received), TRUE);
	class_statistics_get(&pEntry->total_packets_transmitted, &temp, sizeof(pEntry->total_packets_transmitted), TRUE);
}

static void stat_pppoe_get(pPPPoE_Info pEntry, PStatPPPoEEntryResponse snapshot, U32 do_reset)
{
	class_statistics_get(&pEntry->total_packets_received, &snapshot->total_packets_received, sizeof(pEntry->total_packets_received), do_reset);
	class_statistics_get(&pEntry->total_packets_transmitted, &snapshot->total_packets_transmitted, sizeof(pEntry->total_packets_transmitted), do_reset);
}

#else

void stat_pppoe_reset(pPPPoE_Info pEntry)
{
	pEntry->total_packets_received = 0;
	pEntry->total_packets_transmitted = 0;
}

static void stat_pppoe_get(pPPPoE_Info pEntry, PStatPPPoEEntryResponse snapshot)
{
	snapshot->total_packets_received = pEntry->total_packets_received;
	snapshot->total_packets_transmitted = pEntry->total_packets_transmitted;
}

#endif /* !defined(COMCERTO_2000) */

/* This function fills in the snapshot of all PPPoE Sessions 
   in a Session Table */

static int stat_PPPoE_Get_Session_Snapshot(int stat_pppoe_hash_index, int stat_tot_entries, PStatPPPoEEntryResponse pStatPPPoESnapshot)
{
	int stat_tot_sessions=0;
	pPPPoE_Info pStatPPPoEEntry;
	struct slist_entry *entry;

	slist_for_each(pStatPPPoEEntry, entry, &pppoe_cache[stat_pppoe_hash_index], list)
	{
		pStatPPPoESnapshot->eof = 0;
		pStatPPPoESnapshot->sessionID = htons(pStatPPPoEEntry->sessionID);
		pStatPPPoESnapshot->interface_no = itf_get_phys_port(&pStatPPPoEEntry->itf);

		stat_pppoe_get(pStatPPPoEEntry, pStatPPPoESnapshot, gStatPPPoEQueryStatus & STAT_PPPOE_QUERY_RESET);

		pStatPPPoESnapshot++;
		stat_tot_sessions++;

		if (--stat_tot_entries <= 0)
			break;
	}

	return stat_tot_sessions;
}

/* This function creates the snapshot memory and returns the 
   next PPPoE session entry from the PPPoE Session snapshot 
   to the caller  */
int stat_PPPoE_Get_Next_SessionEntry(PStatPPPoEEntryResponse pStatSessionCmd, int reset_action)
{
    int stat_pppoe_hash_entries;
	PStatPPPoEEntryResponse pStatSession;
	static PStatPPPoEEntryResponse pStatPPPoESnapshot = NULL;
	static int stat_pppoe_session_hash_index=0, stat_pppoe_snapshot_entries = 0, stat_pppoe_snapshot_index = 0, stat_pppoe_snapshot_buf_entries = 0;

	if(reset_action)
	{
		stat_pppoe_session_hash_index = 0;
		stat_pppoe_snapshot_entries = 0;
		stat_pppoe_snapshot_index = 0;
		if(pStatPPPoESnapshot)
		{
			Heap_Free(pStatPPPoESnapshot);
			pStatPPPoESnapshot = NULL;
		}
		stat_pppoe_snapshot_buf_entries = 0;
		return NO_ERR;
	}

	
	if (stat_pppoe_snapshot_index == 0)
    {
       
       while( stat_pppoe_session_hash_index <  NUM_PPPOE_ENTRIES)
       {
       	  stat_pppoe_hash_entries = PPPoE_Get_Hash_Sessions(stat_pppoe_session_hash_index);
       	  if (stat_pppoe_hash_entries == 0)
       	  {
       	  	stat_pppoe_session_hash_index++;
       	  	continue;
       	  }
       	  
       	  if(stat_pppoe_hash_entries > stat_pppoe_snapshot_buf_entries)
       	  {
       	  	if(pStatPPPoESnapshot)
       	  		Heap_Free(pStatPPPoESnapshot);	
       	  	pStatPPPoESnapshot = Heap_Alloc(stat_pppoe_hash_entries * sizeof(StatPPPoEEntryResponse));
       	  	if (!pStatPPPoESnapshot)
       	  	{
       	  		stat_pppoe_session_hash_index = 0;
       	  		stat_pppoe_snapshot_buf_entries = 0;
       	  		return ERR_NOT_ENOUGH_MEMORY;
       	  	}
       	  	stat_pppoe_snapshot_buf_entries = stat_pppoe_hash_entries;
       	  }
       	  stat_pppoe_snapshot_entries = stat_PPPoE_Get_Session_Snapshot(stat_pppoe_session_hash_index,stat_pppoe_hash_entries,pStatPPPoESnapshot);
       	  break;
        }
        if (stat_pppoe_session_hash_index >= NUM_PPPOE_ENTRIES)
		{
			stat_pppoe_session_hash_index = 0;
			if(pStatPPPoESnapshot)
			{
				Heap_Free(pStatPPPoESnapshot);
				pStatPPPoESnapshot = NULL;
			}
			stat_pppoe_snapshot_buf_entries = 0;
			return ERR_PPPOE_ENTRY_NOT_FOUND;
		}
 	
    }
    
   	pStatSession = &pStatPPPoESnapshot[stat_pppoe_snapshot_index++];
   	
   	SFL_memcpy(pStatSessionCmd, pStatSession, sizeof(StatPPPoEEntryResponse));
	if (stat_pppoe_snapshot_index == stat_pppoe_snapshot_entries)
	{
	    stat_pppoe_snapshot_index = 0;
	    stat_pppoe_session_hash_index++;
	}
	   
    
	return NO_ERR;
}

#endif /* CFG_STATS */
