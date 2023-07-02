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
#include "module_tunnel.h"

/* This function returns total bridge entries configured
   in a given hash index */

static int Tnl_Get_Hash_Entries(int hash_index)
{
	int tot_tunnels = 0;
	if (hash_index < 0)
	{
		int i;
		PTnlEntry pTnlEntry;
		for (i = 0; i < TNL_MAX_TUNNEL_DMEM; i++)
		{
			pTnlEntry = &gTNLCtx.tunnel_table[i];
			if (pTnlEntry->state != TNL_STATE_FREE)
				tot_tunnels++;
		}
	}
	else
	{
		PTnlEntry pStatTunnelEntry;
		struct slist_entry *entry;
		slist_for_each(pStatTunnelEntry, entry, &gre_tunnel_cache[hash_index], list)
		{
			tot_tunnels++;
		}
	}
	return tot_tunnels;
}


/* This function fills in the snapshot of all tunnel entries of a tunnel cache */

static void fill_snapshot(PTNLCommand_query pTnlSnapshot, PTnlEntry pTnlEntry)
{
	memset(pTnlSnapshot , 0, sizeof(TNLCommand_query));
	pTnlSnapshot->mode = pTnlEntry->mode;
	pTnlSnapshot->secure = pTnlEntry->secure;
	SFL_memcpy(pTnlSnapshot->name, get_onif_name(pTnlEntry->itf.index), 16);
	SFL_memcpy(pTnlSnapshot->local, pTnlEntry->local, IPV6_ADDRESS_LENGTH);
	SFL_memcpy(pTnlSnapshot->remote, pTnlEntry->remote, IPV6_ADDRESS_LENGTH);
	pTnlSnapshot->fl=pTnlEntry->fl;
	pTnlSnapshot->frag_off = pTnlEntry->frag_off;
	pTnlSnapshot->enabled = pTnlEntry->state;
	pTnlSnapshot->elim = pTnlEntry->elim;
	pTnlSnapshot->hlim = pTnlEntry->hlim;
	pTnlSnapshot->mtu = pTnlEntry->tnl_mtu;
}

static int Tnl_Get_Hash_Snapshot(int hash_index, int tnl_entries, PTNLCommand_query pTnlSnapshot)
{
        int tot_tnls = 0;
        PTnlEntry pTnlEntry;

	if (hash_index < 0)
	{
		int i;
		for (i = 0; i < TNL_MAX_TUNNEL_DMEM; i++)
		{
			pTnlEntry = &gTNLCtx.tunnel_table[i];
			if (pTnlEntry->state == TNL_STATE_FREE)
				continue;
			fill_snapshot(pTnlSnapshot, pTnlEntry);
			pTnlSnapshot++;
			tot_tnls++;
			tnl_entries--;
			if (tnl_entries == 0)
				break;
		}
	}
	else
	{
		struct slist_entry *entry;
		slist_for_each(pTnlEntry, entry, &gre_tunnel_cache[hash_index], list)
		{
			fill_snapshot(pTnlSnapshot, pTnlEntry);
			pTnlSnapshot++;
			tot_tnls++;
			tnl_entries--;
			if (tnl_entries == 0)
				break;
		}
	}
        return tot_tnls;
}

U16 Tnl_Get_Next_Hash_Entry(PTNLCommand_query pTnlCmd, int reset_action)
{
	int total_tnl_entries;
	PTNLCommand_query pTnl;
	static PTNLCommand_query pTnlSnapshot = NULL;
	static int tnl_hash_index = -1, tnl_snapshot_entries = 0, tnl_snapshot_index = 0;

	if(reset_action)
	{
		tnl_hash_index = -1;
		tnl_snapshot_entries = 0;
		tnl_snapshot_index = 0;
		if (pTnlSnapshot)
		{
			Heap_Free(pTnlSnapshot);
			pTnlSnapshot = NULL;
		}
	}

	if (tnl_snapshot_index == 0)
	{
		while (tnl_hash_index < NUM_GRE_TUNNEL_ENTRIES)
		{
			total_tnl_entries = Tnl_Get_Hash_Entries(tnl_hash_index);
			if (total_tnl_entries == 0)
			{
				tnl_hash_index++;
				continue;
			}
			if (pTnlSnapshot)
				Heap_Free(pTnlSnapshot);
			pTnlSnapshot = Heap_Alloc(total_tnl_entries * sizeof(TNLCommand_query));
			if (!pTnlSnapshot)
				return ERR_NOT_ENOUGH_MEMORY;
			tnl_snapshot_entries = Tnl_Get_Hash_Snapshot(tnl_hash_index, total_tnl_entries, pTnlSnapshot);
			break;
		}
		if (tnl_hash_index >= NUM_GRE_TUNNEL_ENTRIES)
		{
			tnl_hash_index = -1;
			if (pTnlSnapshot)
			{
				Heap_Free(pTnlSnapshot);
				pTnlSnapshot = NULL;
			}
			return ERR_TNL_ENTRY_NOT_FOUND;
		}
	}

	pTnl = &pTnlSnapshot[tnl_snapshot_index++];
        SFL_memcpy(pTnlCmd, pTnl, sizeof(TNLCommand_query));
	if (tnl_snapshot_index == tnl_snapshot_entries)
	{
		tnl_snapshot_index = 0;
		tnl_hash_index ++;
	}

	return NO_ERR;
}


#ifdef CFG_STATS
static void stat_tunnel_get(PTnlEntry pEntry, PStatTunnelEntryResponse snapshot, U32 do_reset)
{
	PHw_TnlEntry_gre hw_tnl_entry = pEntry->hw_tnl_entry;
	class_statistics_get_ddr(hw_tnl_entry->total_packets_received, &snapshot->total_packets_received,
						sizeof(snapshot->total_packets_received), do_reset);
	class_statistics_get_ddr(hw_tnl_entry->total_packets_transmitted, &snapshot->total_packets_transmitted,
						sizeof(snapshot->total_packets_transmitted), do_reset);
	class_statistics_get_ddr(hw_tnl_entry->total_bytes_received, &snapshot->total_bytes_received,
						sizeof(snapshot->total_bytes_received), do_reset);
	class_statistics_get_ddr(hw_tnl_entry->total_bytes_transmitted, &snapshot->total_bytes_transmitted,
						sizeof(snapshot->total_bytes_transmitted), do_reset);
}

void stat_tunnel_reset(PTnlEntry pEntry)
{
	StatTunnelEntryResponse temp_snapshot;
	stat_tunnel_get(pEntry, &temp_snapshot, TRUE);
}

/* This function fills in the snapshot of all tunnel entries of a tunnel cache along with statistics information*/

static int stat_tunnel_Get_Session_Snapshot(int hash_index, int stat_tunnel_entries, PStatTunnelEntryResponse pStatTunnelSnapshot)
{
	int stat_tot_tunnel = 0;
	PTnlEntry pStatTunnelEntry;
	struct slist_entry *entry;

	slist_for_each(pStatTunnelEntry, entry, &gre_tunnel_cache[hash_index], list)
	{
		memset(pStatTunnelSnapshot, 0, sizeof(StatTunnelEntryResponse));
		strcpy((char *)pStatTunnelSnapshot->ifname, get_onif_name(pStatTunnelEntry->itf.index));
		stat_tunnel_get(pStatTunnelEntry, pStatTunnelSnapshot, gStatTunnelQueryStatus & STAT_TUNNEL_QUERY_RESET);

		pStatTunnelSnapshot++;
		stat_tot_tunnel++;

		if (--stat_tunnel_entries <= 0)
			break;
	}

	return stat_tot_tunnel;
}


int stat_tunnel_Get_Next_SessionEntry(PStatTunnelEntryResponse pResponse, int reset_action)
{
	int stat_total_tunnel_entries;
	PStatTunnelStatusCmd pCommand = (PStatTunnelStatusCmd)pResponse;
	PStatTunnelEntryResponse pStatTunnel;
	static PStatTunnelEntryResponse pStatTunnelSnapshot = NULL;
	static int stat_tunnel_hash_index = 0, stat_tunnel_snapshot_entries = 0, stat_tunnel_snapshot_index = 0;
	static char stat_tunnel_name[IF_NAME_SIZE];

	if(reset_action)
	{
		stat_tunnel_hash_index = 0;
		stat_tunnel_snapshot_entries = 0;
		stat_tunnel_snapshot_index = 0;
		if (pStatTunnelSnapshot)
		{
			Heap_Free(pStatTunnelSnapshot);
			pStatTunnelSnapshot = NULL;
		}
		memcpy(stat_tunnel_name, pCommand->ifname, IF_NAME_SIZE - 1);
		return NO_ERR;
	}

top:
	if (stat_tunnel_snapshot_index == 0)
	{
		while(stat_tunnel_hash_index < NUM_GRE_TUNNEL_ENTRIES)
		{
			stat_total_tunnel_entries = Tnl_Get_Hash_Entries(stat_tunnel_hash_index);
			if (stat_total_tunnel_entries == 0)
			{
				stat_tunnel_hash_index++;
				continue;
			}

			if(pStatTunnelSnapshot)
				Heap_Free(pStatTunnelSnapshot);
			pStatTunnelSnapshot = Heap_Alloc(stat_total_tunnel_entries * sizeof(StatTunnelEntryResponse));
			if (!pStatTunnelSnapshot)
			{
				stat_tunnel_hash_index = 0;
				return ERR_NOT_ENOUGH_MEMORY;
			}

			stat_tunnel_snapshot_entries = stat_tunnel_Get_Session_Snapshot(stat_tunnel_hash_index,stat_total_tunnel_entries,pStatTunnelSnapshot);
			break;
		}

		if (stat_tunnel_hash_index >= NUM_GRE_TUNNEL_ENTRIES)
		{
			stat_tunnel_hash_index = 0;
			if(pStatTunnelSnapshot)
			{
				Heap_Free(pStatTunnelSnapshot);
				pStatTunnelSnapshot = NULL;
			}
			return ERR_TNL_ENTRY_NOT_FOUND;
		}
	}

	pStatTunnel = &pStatTunnelSnapshot[stat_tunnel_snapshot_index++];

	SFL_memcpy(pResponse, pStatTunnel, sizeof(StatTunnelEntryResponse));
	if (stat_tunnel_snapshot_index == stat_tunnel_snapshot_entries)
	{
		stat_tunnel_snapshot_index = 0;
		stat_tunnel_hash_index++;
	}

	if (stat_tunnel_name[0])
	{
		// If name is specified, and no match, keep looking
		if (strcmp(stat_tunnel_name, pResponse->ifname) != 0)
			goto top;
		// If name matches, force EOF on next call
		stat_tunnel_hash_index = NUM_GRE_TUNNEL_ENTRIES;
		stat_tunnel_snapshot_index = 0;
	}

	return NO_ERR;
}

#endif	// CFG_STATS

