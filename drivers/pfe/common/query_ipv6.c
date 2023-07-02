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
#include "fe.h"
#include "module_ipv6.h"
#include "module_ipsec.h"


/* This function returns total ipv6 conntrack entries 
   configured in a given hash index */
static int IPv6_Get_Hash_CtEntries(int ct6_hash_index)
{
	int tot_ipv6_ct_entries = 0;
	PCtEntryIPv6 pCtEntryIPv6;
	struct slist_entry *entry;
	
	slist_for_each(pCtEntryIPv6, entry, &ct_cache[ct6_hash_index], list)
	{
	    	if (IS_IPV6(pCtEntryIPv6) && (pCtEntryIPv6->status & CONNTRACK_ORIG) == CONNTRACK_ORIG)
	    	    	tot_ipv6_ct_entries++;
	}
		
	return tot_ipv6_ct_entries;

}

/* This function fills the snapshot of ipv6 CT entries in a given hash index */
static int IPv6_CT_Get_Hash_Snapshot(int ct6_hash_index,int v6_ct_total_entries, PCtExCommandIPv6 pSnapshot)
{
	
	int tot_ipv6_ct_entries=0;
	PCtEntryIPv6 pCtEntryIPv6;
	PCtEntryIPv6 twin_entry;
	struct slist_entry *entry;
	
	slist_for_each(pCtEntryIPv6, entry, &ct_cache[ct6_hash_index], list)
	{
	     	if (IS_IPV6(pCtEntryIPv6) && (pCtEntryIPv6->status & CONNTRACK_ORIG) == CONNTRACK_ORIG)
	     	{
				twin_entry = CT6_TWIN(pCtEntryIPv6);

				SFL_memcpy(pSnapshot->Daddr, pCtEntryIPv6->Daddr_v6, IPV6_ADDRESS_LENGTH);
				SFL_memcpy(pSnapshot->Saddr, pCtEntryIPv6->Saddr_v6, IPV6_ADDRESS_LENGTH);
				pSnapshot->Sport = 	pCtEntryIPv6->Sport;
				pSnapshot->Dport = 	pCtEntryIPv6->Dport;
			
				SFL_memcpy(pSnapshot->DaddrReply, twin_entry->Daddr_v6, IPV6_ADDRESS_LENGTH);
				SFL_memcpy(pSnapshot->SaddrReply, twin_entry->Saddr_v6, IPV6_ADDRESS_LENGTH);
				pSnapshot->SportReply = 	twin_entry->Sport;
				pSnapshot->DportReply = 	twin_entry->Dport;
				pSnapshot->protocol   =  GET_PROTOCOL(pCtEntryIPv6); 
				pSnapshot->fwmark     = IP_get_fwmark((PCtEntry)pCtEntryIPv6, (PCtEntry)twin_entry);
				pSnapshot->SA_nr      =	0;
				pSnapshot->SAReply_nr	= 	0;
				pSnapshot->format = 0;
				
				if ((pCtEntryIPv6->status & CONNTRACK_SEC) == CONNTRACK_SEC)
				{
					int i;
					pSnapshot->format |= CT_SECURE;
					for (i= 0; i < SA_MAX_OP; i++)
					{
						if (pCtEntryIPv6->hSAEntry[i])
						{
							pSnapshot->SA_nr++;
							pSnapshot->SA_handle[i] = pCtEntryIPv6->hSAEntry[i];
						}
					}
					
					for (i= 0; i < SA_MAX_OP; i++)
					{
						if (twin_entry->hSAEntry[i])
						{
							pSnapshot->SAReply_nr++;
							pSnapshot->SAReply_handle[i] = twin_entry->hSAEntry[i];
						}
						
					}
							
				}
				
				pSnapshot++;
				tot_ipv6_ct_entries++;
				
				if (--v6_ct_total_entries <= 0)
					break;
	     	}
	}
		
	return tot_ipv6_ct_entries;

}


/* This function creates the snapshot memory and returns the 
   next ipv6 CT entry from the snapshop of the ipv6 conntrack entries of a
   single hash to the caller  */
   
int IPv6_Get_Next_Hash_CTEntry(PCtExCommandIPv6 pV6CtCmd, int reset_action)
{
	int ipv6_ct_hash_entries;
	PCtExCommandIPv6 pV6Ct;
	static PCtExCommandIPv6 pV6CtSnapshot = NULL;
	static int v6_ct_hash_index = 0, v6_ct_snapshot_entries =0, v6_ct_snapshot_index=0, v6_ct_snapshot_buf_entries = 0;
	
	if(reset_action)
	{
		v6_ct_hash_index = 0;
		v6_ct_snapshot_entries =0;
		v6_ct_snapshot_index=0;
		if(pV6CtSnapshot)
		{
			Heap_Free(pV6CtSnapshot);
			pV6CtSnapshot = NULL;	
		}
		v6_ct_snapshot_buf_entries = 0;
	}
	
	if (v6_ct_snapshot_index == 0)
	{
		while( v6_ct_hash_index < NUM_CT_ENTRIES)
		{
		
			ipv6_ct_hash_entries = IPv6_Get_Hash_CtEntries(v6_ct_hash_index);
			if(ipv6_ct_hash_entries == 0)
			{
				v6_ct_hash_index++;
				continue;
			}
		   	
		   	if(ipv6_ct_hash_entries > v6_ct_snapshot_buf_entries)
		   	{
		   		
		   		if(pV6CtSnapshot)
		   			Heap_Free(pV6CtSnapshot);
				pV6CtSnapshot = Heap_Alloc(ipv6_ct_hash_entries * sizeof(CtExCommandIPv6));
			
				if (!pV6CtSnapshot)
				{
			    	v6_ct_hash_index = 0;
			    	v6_ct_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				v6_ct_snapshot_buf_entries = ipv6_ct_hash_entries;
				
		   	}
			v6_ct_snapshot_entries = IPv6_CT_Get_Hash_Snapshot(v6_ct_hash_index , ipv6_ct_hash_entries,pV6CtSnapshot);
		
					
			break;
		}
		
		if (v6_ct_hash_index >= NUM_CT_ENTRIES)
		{
			v6_ct_hash_index = 0;
			if(pV6CtSnapshot)
			{
				Heap_Free(pV6CtSnapshot);
				pV6CtSnapshot = NULL;	
			}
			v6_ct_snapshot_buf_entries = 0;
			return ERR_CT_ENTRY_NOT_FOUND;
		}
		   
	}
	
	pV6Ct = &pV6CtSnapshot[v6_ct_snapshot_index++];
	SFL_memcpy(pV6CtCmd, pV6Ct, sizeof(CtExCommandIPv6));
	if (v6_ct_snapshot_index == v6_ct_snapshot_entries)
	{
		v6_ct_snapshot_index = 0;
		v6_ct_hash_index ++;
	}
		
	
	return NO_ERR;	
		
}
