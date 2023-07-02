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
#include "module_vlan.h"
#include "layer2.h"



/* This function returns total vlan interfaces configured in FPP */
static int Vlan_Get_Hash_Entries(int vlan_hash_index)
{
	int tot_vlans=0;
	struct slist_entry *entry;

	slist_for_each_entry(entry, &vlan_cache[vlan_hash_index])
		tot_vlans++;

	return tot_vlans;
}


/* This function fills in the snapshot of all Vlan entries of a VLAN cache */

static int Vlan_Get_Hash_Snapshot(int vlan_hash_index, int vlan_entries, PVlanCommand pVlanSnapshot)
{
	int tot_vlans=0;
	PVlanEntry pVlanEntry;
	struct slist_entry *entry;

	slist_for_each(pVlanEntry, entry, &vlan_cache[vlan_hash_index], list)
	{
		pVlanSnapshot->vlanID = ntohs(pVlanEntry->VlanId);
		strcpy((char *)pVlanSnapshot->vlanifname, get_onif_name(pVlanEntry->itf.index));
		strcpy((char *)pVlanSnapshot->phyifname, get_onif_name(pVlanEntry->itf.phys->index));

		pVlanSnapshot++;
		tot_vlans++;

		if (--vlan_entries <= 0)
			break;
	}

	return tot_vlans;
	
}


   
int Vlan_Get_Next_Hash_Entry(PVlanCommand pVlanCmd, int reset_action)
{
    int total_vlan_entries;
	PVlanCommand pVlan;
	static PVlanCommand pVlanSnapshot = NULL;
	static int vlan_hash_index = 0, vlan_snapshot_entries =0, vlan_snapshot_index=0, vlan_snapshot_buf_entries = 0;

	if(reset_action)
	{
		vlan_hash_index = 0;
		vlan_snapshot_entries =0;
		vlan_snapshot_index=0;
		if(pVlanSnapshot)
		{
			Heap_Free(pVlanSnapshot);
			pVlanSnapshot = NULL;
		}
		vlan_snapshot_buf_entries = 0;
	}
	
    if (vlan_snapshot_index == 0)
    {
    	while( vlan_hash_index < NUM_VLAN_ENTRIES)
		{
        	total_vlan_entries = Vlan_Get_Hash_Entries(vlan_hash_index);
        	if (total_vlan_entries == 0)
        	{
        		vlan_hash_index++;
        		continue;
        	}
        
        	if(total_vlan_entries > vlan_snapshot_buf_entries)
        	{
        		if(pVlanSnapshot)
        			Heap_Free(pVlanSnapshot);
        		
        		pVlanSnapshot = Heap_Alloc(total_vlan_entries * sizeof(VlanCommand));
			
				if (!pVlanSnapshot)
				{
					vlan_hash_index = 0;
					vlan_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;	
				}
				vlan_snapshot_buf_entries = total_vlan_entries;
        	}
	
		
			vlan_snapshot_entries = Vlan_Get_Hash_Snapshot(vlan_hash_index,total_vlan_entries,pVlanSnapshot);
			break;
		
		}
		if (vlan_hash_index >= NUM_VLAN_ENTRIES)
		{
			vlan_hash_index = 0;
			if(pVlanSnapshot)
			{
				Heap_Free(pVlanSnapshot);
				pVlanSnapshot = NULL;
			}
			vlan_snapshot_buf_entries = 0;
			return ERR_VLAN_ENTRY_NOT_FOUND;
		}
    }
    
   	pVlan = &pVlanSnapshot[vlan_snapshot_index++];
   	
   	SFL_memcpy(pVlanCmd, pVlan, sizeof(VlanCommand));
	if (vlan_snapshot_index == vlan_snapshot_entries)
	{
	    vlan_snapshot_index = 0;
	    vlan_hash_index++;
	}
	
	return NO_ERR;
}


#ifdef CFG_STATS

#if defined(COMCERTO_2000)
void stat_vlan_reset(PVlanEntry pEntry)
{
	U64 temp;
	class_statistics_get(&pEntry->total_packets_received, &temp, sizeof(pEntry->total_packets_received), TRUE);
	class_statistics_get(&pEntry->total_packets_transmitted, &temp, sizeof(pEntry->total_packets_transmitted), TRUE);
	class_statistics_get(&pEntry->total_bytes_received, &temp, sizeof(pEntry->total_bytes_received), TRUE);
	class_statistics_get(&pEntry->total_bytes_transmitted, &temp, sizeof(pEntry->total_bytes_transmitted), TRUE);
}

static void stat_vlan_get(PVlanEntry pEntry, PStatVlanEntryResponse snapshot, U32 do_reset)
{
	class_statistics_get(&pEntry->total_packets_received, &snapshot->total_packets_received, sizeof(pEntry->total_packets_received), do_reset);
	class_statistics_get(&pEntry->total_packets_transmitted, &snapshot->total_packets_transmitted, sizeof(pEntry->total_packets_transmitted), do_reset);
	class_statistics_get(&pEntry->total_bytes_received, &snapshot->total_bytes_received, sizeof(pEntry->total_bytes_received), do_reset);
	class_statistics_get(&pEntry->total_bytes_transmitted, &snapshot->total_bytes_transmitted, sizeof(pEntry->total_bytes_transmitted), do_reset);
}

#else
void stat_vlan_reset(PVlanEntry pEntry)
{
	pEntry->total_packets_received = 0;
	pEntry->total_packets_transmitted = 0;
	pEntry->total_bytes_received = 0; 
	pEntry->total_bytes_transmitted = 0;
}

static void stat_vlan_get(PVlanEntry pEntry, PStatVlanEntryResponse snapshot, U32 do_reset)
{
	snapshot->total_packets_received = pEntry->total_packets_received;
	snapshot->total_packets_transmitted = pEntry->total_packets_transmitted;
      	GET_LSB(pEntry->total_bytes_received, snapshot->total_bytes_received[0], U32);
	GET_MSB(pEntry->total_bytes_received, snapshot->total_bytes_received[1], U32);
      	GET_LSB(pEntry->total_bytes_transmitted, snapshot->total_bytes_transmitted[0], U32);
	GET_MSB(pEntry->total_bytes_transmitted, snapshot->total_bytes_transmitted[1], U32);
	if (do_reset)
		stat_vlan_reset(pEntry);
}

#endif /* !defined(COMCERTO_2000) */

/* This function fills in the snapshot of all Vlan entries of a VLAN cache along with statistics information*/

static int stat_VLAN_Get_Session_Snapshot(int stat_vlan_hash_index, int stat_vlan_entries, PStatVlanEntryResponse pStatVLANSnapshot)
{
	int stat_tot_vlans=0;
	PVlanEntry pStatVlanEntry;
	struct slist_entry *entry;

	slist_for_each(pStatVlanEntry, entry, &vlan_cache[stat_vlan_hash_index], list)
	{
		pStatVLANSnapshot->eof = 0;
		pStatVLANSnapshot->vlanID = ntohs(pStatVlanEntry->VlanId);
		strcpy((char *)pStatVLANSnapshot->vlanifname, get_onif_name(pStatVlanEntry->itf.index));
		strcpy((char *)pStatVLANSnapshot->phyifname, get_onif_name(pStatVlanEntry->itf.phys->index));

		stat_vlan_get(pStatVlanEntry, pStatVLANSnapshot, gStatVlanQueryStatus & STAT_VLAN_QUERY_RESET);
		
		pStatVLANSnapshot++;
		stat_tot_vlans++;

		if (--stat_vlan_entries <= 0)
			break;
	}	

	return stat_tot_vlans;
	
}


   
int stat_VLAN_Get_Next_SessionEntry(PStatVlanEntryResponse pStatVlanCmd, int reset_action)
{
    int stat_total_vlan_entries;
	PStatVlanEntryResponse pStatVlan;
	static PStatVlanEntryResponse pStatVLANSnapshot = NULL;
	static int stat_vlan_hash_index = 0, stat_vlan_snapshot_entries =0, stat_vlan_snapshot_index=0, stat_vlan_snapshot_buf_entries = 0;

	if(reset_action)
	{
		stat_vlan_hash_index = 0;
		stat_vlan_snapshot_entries =0;
		stat_vlan_snapshot_index=0;
		if(pStatVLANSnapshot)
		{
			Heap_Free(pStatVLANSnapshot);
			pStatVLANSnapshot = NULL;
		}
		stat_vlan_snapshot_buf_entries = 0;
		return NO_ERR;
	}
	
    	if (stat_vlan_snapshot_index == 0)
    	{
    		while(stat_vlan_hash_index < NUM_VLAN_ENTRIES)
		{
        		stat_total_vlan_entries = Vlan_Get_Hash_Entries(stat_vlan_hash_index);
        		if (stat_total_vlan_entries == 0)
        		{
        			stat_vlan_hash_index++;
        			continue;
        		}
        
        		if(stat_total_vlan_entries > stat_vlan_snapshot_buf_entries)
        		{
        			if(pStatVLANSnapshot)
        				Heap_Free(pStatVLANSnapshot);
        		
        			pStatVLANSnapshot = Heap_Alloc(stat_total_vlan_entries * sizeof(StatVlanEntryResponse));
			
				if (!pStatVLANSnapshot)
				{
					stat_vlan_hash_index = 0;
					stat_vlan_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;	
				}
				stat_vlan_snapshot_buf_entries = stat_total_vlan_entries;
        		}
	
		
			stat_vlan_snapshot_entries = stat_VLAN_Get_Session_Snapshot(stat_vlan_hash_index,stat_total_vlan_entries,pStatVLANSnapshot);
			break;		
		}
			
		if (stat_vlan_hash_index >= NUM_VLAN_ENTRIES)
		{
			stat_vlan_hash_index = 0;
			if(pStatVLANSnapshot)
			{
				Heap_Free(pStatVLANSnapshot);
				pStatVLANSnapshot = NULL;
			}
			stat_vlan_snapshot_buf_entries = 0;
			return ERR_VLAN_ENTRY_NOT_FOUND;
		}
    	}
    
   	pStatVlan = &pStatVLANSnapshot[stat_vlan_snapshot_index++];
   	
   	SFL_memcpy(pStatVlanCmd, pStatVlan, sizeof(StatVlanEntryResponse));
	if (stat_vlan_snapshot_index == stat_vlan_snapshot_entries)
	{
	    stat_vlan_snapshot_index = 0;
	    stat_vlan_hash_index++;
	}
	
	return NO_ERR;
}

#endif /* CFG_STATS */
