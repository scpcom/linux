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


#ifdef CFG_MACVLAN

#include "layer2.h"
#include "module_macvlan.h"
#include "gemac.h"
#include "system.h"
#include "fe.h"
#include "module_hidrv.h"
#include "fpp.h"
#include "module_ethernet.h"


/* This function returns total macvlan entries configured in a given hash index */
static int Macvlan_Get_Hash_Entries(int macvlan_hash_index)
{
	
	int tot_macvlan_entries = 0;
	PMacvlanEntry pMacvlanEntry, pFirstEntry;
	
	pMacvlanEntry = pFirstEntry = macvlan_cache[macvlan_hash_index];

	if (pMacvlanEntry)
	{
		do {
			tot_macvlan_entries++;
			pMacvlanEntry = pMacvlanEntry->next;
		}while(pMacvlanEntry != pFirstEntry);
	}
		
	return tot_macvlan_entries;

}


/* This function fills the snapshot of Macvlan entries in a given hash index */
static int Macvlan_Get_Hash_Snapshot( int macvlan_hash_index, int macvlan_tot_entries, PMacvlanCmd pMacvlanSnapshot)
{
	
	int tot_macvlan_entries = 0;
	PMacvlanEntry  pMacvlanEntry, pFirstEntry;

	pMacvlanEntry = pFirstEntry = macvlan_cache[macvlan_hash_index];
			
	if((pMacvlanEntry) && (macvlan_tot_entries > 0))
	{
		do
		{
			SFL_memcpy(pMacvlanSnapshot->macaddr, pMacvlanEntry->MACaddr, ETHER_ADDR_LEN);
		 	SFL_memcpy(pMacvlanSnapshot->macvlan_ifname, get_onif_name(pMacvlanEntry->itf.index), IF_NAME_SIZE);
			SFL_memcpy(pMacvlanSnapshot->phys_ifname, get_onif_name(pMacvlanEntry->itf.phys->index), IF_NAME_SIZE);
			     	
			pMacvlanSnapshot++;
			tot_macvlan_entries++;
			pMacvlanEntry = pMacvlanEntry->next;
			macvlan_tot_entries--;
		}while(pMacvlanEntry != pFirstEntry);
	}
		
	return tot_macvlan_entries;

}


/* 	This function creates the snapshot memory and returns the next macvlan entry 
 	from the snapshot of the Macvlan entries of a  single hash to the caller  */
   
static int Macvlan_Get_Next_Hash_Entry(PMacvlanCmd pMacvlanCmd, int reset_action)
{
	PMacvlanCmd        pMacvlan;
	int                macvlan_hash_entries;
	static PMacvlanCmd pMacvlanSnapshot = NULL;
	static int         macvlan_hash_index = 0, 
                           macvlan_snapshot_entries = 0, 
                           macvlan_snapshot_index = 0, 
                           macvlan_snapshot_buf_entries = 0;
	
	
	if(reset_action)
	{
		macvlan_hash_index = 0;
		macvlan_snapshot_entries =0;
		macvlan_snapshot_index=0;
		if(pMacvlanSnapshot)
		{
			Heap_Free(pMacvlanSnapshot);
			pMacvlanSnapshot = NULL;
		}
		macvlan_snapshot_buf_entries = 0;
		
	}
	
		
	if (macvlan_snapshot_index == 0)
	{
		
		while(macvlan_hash_index <  NUM_MACVLAN_HASH_ENTRIES)
		{
		
			macvlan_hash_entries = Macvlan_Get_Hash_Entries(macvlan_hash_index);
			if(macvlan_hash_entries == 0)
			{
				macvlan_hash_index++;
				continue;
			}
		   	
		   	if(macvlan_hash_entries > macvlan_snapshot_buf_entries)
		   	{
		   		if(pMacvlanSnapshot)
		   			Heap_Free(pMacvlanSnapshot);
				pMacvlanSnapshot = Heap_Alloc(macvlan_hash_entries * sizeof(MacvlanCmd));
			
				if (!pMacvlanSnapshot)
				{
				    	macvlan_hash_index = 0;
			    		macvlan_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				macvlan_snapshot_buf_entries = macvlan_hash_entries;
		   	}
				
			macvlan_snapshot_entries = Macvlan_Get_Hash_Snapshot(macvlan_hash_index, macvlan_hash_entries, pMacvlanSnapshot);
			
			break;
		}
		
		if (macvlan_hash_index >= NUM_MACVLAN_HASH_ENTRIES)
		{
			macvlan_hash_index = 0;
			if(pMacvlanSnapshot)
			{
				Heap_Free(pMacvlanSnapshot);
				pMacvlanSnapshot = NULL;
			}
			macvlan_snapshot_buf_entries = 0;
			return ERR_MACVLAN_ENTRY_NOT_FOUND;
		}
		   
	}
	
	pMacvlan = &pMacvlanSnapshot[macvlan_snapshot_index++];
	SFL_memcpy(pMacvlanCmd, pMacvlan, sizeof(MacvlanCmd));
	if (macvlan_snapshot_index == macvlan_snapshot_entries)
	{
		macvlan_snapshot_index = 0;
		macvlan_hash_index ++;
	}
		
	
	return NO_ERR;	
}


static int Macvlan_rule_find( U8* mac_addr, struct itf *itf_phys)
{
	PMacvlanEntry this_entry, first_entry;
	U8 hash_key;

	hash_key = HASH_MACVLAN(mac_addr);
	
	first_entry = macvlan_cache[hash_key];
	if (first_entry == NULL)
		return 0;
	this_entry = first_entry;
	while (1) {
		if (TESTEQ_MACADDR(mac_addr, this_entry->MACaddr) && (this_entry->itf.phys->index == itf_phys->index))
		{
			break;
		}
		this_entry = this_entry->next;
		if(this_entry == first_entry)
			return 0;
	}
  	return 1;
}


/**
 * Macvlan_add_entry()
 *
 */
 
static int Macvlan_add_entry(PMacvlanCmd macvlan_cmd)
{
	POnifDesc phys_onif, macvlan_onif;
	PMacvlanEntry this_entry, first_entry;
	U8 hash_key;

	phys_onif = get_onif_by_name(macvlan_cmd->phys_ifname);
	if (!phys_onif)
		return ERR_UNKNOWN_INTERFACE;

	if ((phys_onif->itf->type != IF_TYPE_ETHERNET) && (phys_onif->itf->type != IF_TYPE_VLAN))
        {
                return ERR_UNKNOWN_INTERFACE;
        }

	macvlan_onif = get_onif_by_name(macvlan_cmd->macvlan_ifname);
	if (macvlan_onif)
		return ERR_MACVLAN_ALREADY_REGISTERED;

	
		
	hash_key = HASH_MACVLAN(macvlan_cmd->macaddr);
	
	if (Macvlan_rule_find(macvlan_cmd->macaddr, phys_onif->itf))
		return ERR_MACVLAN_ALREADY_REGISTERED;

	

	/* If mac doesn't exist, add it to the macvlan cache */
	if ((this_entry = (struct _tMacvlanEntry*)Heap_Alloc_ARAM(sizeof(MacvlanEntry))) == NULL)
		  	return  ERR_CREATION_FAILED;

	memset(this_entry, 0, sizeof(MacvlanEntry));
	SFL_memcpy(this_entry->MACaddr, macvlan_cmd->macaddr, ETHER_ADDR_LEN);

	/* Create the macvlan interface */
	if(!add_onif(macvlan_cmd->macvlan_ifname, &this_entry->itf, phys_onif->itf, IF_TYPE_MACVLAN))
	{
		Heap_Free((PVOID)this_entry);
		return ERR_CREATION_FAILED;
	}	

	first_entry = macvlan_cache[hash_key];
	if (first_entry == NULL)
	{
		macvlan_cache[hash_key] = this_entry;
		this_entry->next = this_entry;
		this_entry->prev = this_entry;
	}
	else
	{
		this_entry->next = first_entry;
		this_entry->prev = first_entry->prev;
		first_entry->prev->next = this_entry;
		first_entry->prev = this_entry;
	}
			
	return NO_ERR;
}


static inline void  Macvlan_free_entry(U8 hash_key , PMacvlanEntry pEntry)
{
	
	if (pEntry->next == pEntry)
		macvlan_cache[hash_key] = NULL;
	else
	{
		pEntry->next->prev = pEntry->prev;
		pEntry->prev->next = pEntry->next;
		if (macvlan_cache[hash_key] == pEntry)
			macvlan_cache[hash_key] = pEntry->next;
	}	
		
	Heap_Free((PVOID)pEntry);
	return;
}


/**
 * Macvlan_remove_entry()
 *
 */
static int Macvlan_remove_entry(PMacvlanCmd macvlan_cmd)
{
	PMacvlanEntry this_entry;
	POnifDesc macvlan_if;
	U8 hash_key;

	macvlan_if = get_onif_by_name(macvlan_cmd->macvlan_ifname);

	if(!macvlan_if)
		return ERR_MACVLAN_ENTRY_NOT_FOUND;

	this_entry = (PMacvlanEntry)macvlan_if->itf;
	hash_key = HASH_MACVLAN(this_entry->MACaddr);

	remove_onif(macvlan_if);

	Macvlan_free_entry(hash_key, this_entry);

	return NO_ERR;
}


static int Macvlan_handle_entry(U16 *p, U16 Length)
{
	MacvlanCmd	cmd;
	U16 rc = NO_ERR;
	int reset_action = 0;
	
	/* Check length */
	if (Length != sizeof(MacvlanCmd) )
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(MacvlanCmd));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);
		
	switch(cmd.action)
	{
		case ACTION_DEREGISTER: 
			rc =  Macvlan_remove_entry(&cmd);
			break;
	
		case ACTION_REGISTER:
			rc =  Macvlan_add_entry(&cmd);
			break;

		case ACTION_QUERY:
			reset_action = 1;
		case ACTION_QUERY_CONT:
			rc = Macvlan_Get_Next_Hash_Entry((MacvlanCmd*)p, reset_action);
			break;

		default :
			rc =  ERR_UNKNOWN_ACTION;
			break;
	} 

	return rc;
}


static int Macvlan_handle_reset(void)
{
	PMacvlanEntry pEntry = NULL;
	int rc = NO_ERR;
	U8 i;

	/* free MACVLAN entries */
	for(i = 0; i < NUM_MACVLAN_HASH_ENTRIES; i++)
	{
		while(macvlan_cache[i] != NULL)
		{
			pEntry = macvlan_cache[i];
			Macvlan_free_entry(i, pEntry);
		}
	}

	return rc;
}


static U16 M_macvlan_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 retlen = 2;
	U16 action;

	switch (cmd_code)
	{
		case CMD_MACVLAN_ENTRY:
			action = *pcmd;
			rc = Macvlan_handle_entry(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				retlen = sizeof(MacvlanCmd);
			break;

		case CMD_MACVLAN_ENTRY_RESET:
			rc = Macvlan_handle_reset();
			break;

		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;
	return retlen;
}


BOOL M_macvlan_init(PModuleDesc pModule)
{
	/* Entry point and Channel registration */
    pModule->entry = &M_macvlan_entry;
    pModule->cmdproc = &M_macvlan_cmdproc;

    return 0;
}

#endif /* CFG_MACVLAN */
