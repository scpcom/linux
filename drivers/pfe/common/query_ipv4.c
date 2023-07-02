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
#include "module_ipv4.h"
#include "module_ipsec.h"


/* This function returns total arp entries configured in a given hash index */
static int IPV4_Get_Hash_CTEntries(int ct_hash_index)
{
	
	int tot_ct_entries = 0;
	PCtEntry pCtEntry;
	struct slist_entry *entry;

	slist_for_each(pCtEntry, entry, &ct_cache[ct_hash_index], list)
	{
	    	if (IS_IPV4(pCtEntry) && (pCtEntry->status & CONNTRACK_ORIG) == CONNTRACK_ORIG)
	    	    	tot_ct_entries++;
	}

	return tot_ct_entries;
}

/* This function fills the snapshot of ipv4 CT entries in a given hash index */
static int IPv4_CT_Get_Hash_Snapshot(int ct_hash_index, int ct_total_entries, PCtExCommand pSnapshot)
{
	
	int tot_ct_entries=0;
	PCtEntry pCtEntry;
	PCtEntry pReplyEntry = NULL;
	struct slist_entry *entry;

	slist_for_each(pCtEntry, entry, &ct_cache[ct_hash_index], list)
	{
	     	if (IS_IPV4(pCtEntry) &&(pCtEntry->status & CONNTRACK_ORIG) == CONNTRACK_ORIG)
	     	{
	     		
				pReplyEntry = CT_TWIN(pCtEntry);

				pSnapshot->Daddr = pCtEntry->Daddr_v4;
				pSnapshot->Saddr = pCtEntry->Saddr_v4;
				pSnapshot->Sport = pCtEntry->Sport;
				pSnapshot->Dport = pCtEntry->Dport;
			
				pSnapshot->DaddrReply =  	pCtEntry->twin_Daddr;
				pSnapshot->SaddrReply =   pCtEntry->twin_Saddr;
				pSnapshot->SportReply = 	pCtEntry->twin_Sport;
				pSnapshot->DportReply = 	pCtEntry->twin_Dport;
				pSnapshot->protocol   =  GET_PROTOCOL(pCtEntry); 
				pSnapshot->fwmark     = IP_get_fwmark(pCtEntry, pReplyEntry);
				pSnapshot->SA_nr      =	0;
				pSnapshot->SAReply_nr	= 	0;
				pSnapshot->format = 0;
				
				if ((pCtEntry->status & CONNTRACK_SEC) == CONNTRACK_SEC)
				{
					int i;
					pSnapshot->format |= CT_SECURE;
					for (i= 0; i < SA_MAX_OP; i++)
					{
						if (pCtEntry->hSAEntry[i])
						{
							pSnapshot->SA_nr++;
							pSnapshot->SA_handle[i] = pCtEntry->hSAEntry[i];
						}
					}
					
					for (i= 0; i < SA_MAX_OP; i++)
					{
						if (pReplyEntry->hSAEntry[i])
						{
							pSnapshot->SAReply_nr++;
							pSnapshot->SAReply_handle[i] = pReplyEntry->hSAEntry[i];
						}
						
					}
							
				}
				
				
				pSnapshot++;
				tot_ct_entries++;

				if (--ct_total_entries <= 0)
					break;
	     	}
	}
	
	return tot_ct_entries;

}


/* This function creates the snapshot memory and returns the 
   next ipv4 CT entry from the snapshop of the ipv4 conntrack entries of a
   single hash to the caller  */
   
int IPv4_Get_Next_Hash_CTEntry(PCtExCommand pCtCmd, int reset_action)
{
	int ct_hash_entries;
	static PCtExCommand pCtSnapshot = NULL;
	static int ct_hash_index = 0,ct_snapshot_entries =0, ct_snapshot_index = 0, ct_snapshot_buf_entries = 0;
	PCtExCommand pCt;
	
	if(reset_action)
	{
		ct_hash_index = 0;
		ct_snapshot_entries =0;
		ct_snapshot_index = 0;
		if(pCtSnapshot)
		{
			Heap_Free(pCtSnapshot);
			pCtSnapshot = NULL;	
		}
		ct_snapshot_buf_entries = 0;
	}
	
	if (ct_snapshot_index == 0)
	{
	
		while( ct_hash_index < NUM_CT_ENTRIES)
		{
		
			ct_hash_entries = IPV4_Get_Hash_CTEntries(ct_hash_index);
			if(ct_hash_entries == 0)
			{
				ct_hash_index++;
				continue;
			}
			if (ct_hash_entries > ct_snapshot_buf_entries)
			{
				if(pCtSnapshot)
					Heap_Free(pCtSnapshot);
			
				pCtSnapshot = Heap_Alloc(ct_hash_entries * sizeof(CtExCommand));
			
				if (!pCtSnapshot)
				{
			    	ct_hash_index = 0;
			    	ct_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				ct_snapshot_buf_entries = ct_hash_entries;
			}
			
			ct_snapshot_entries = IPv4_CT_Get_Hash_Snapshot(ct_hash_index ,ct_hash_entries, pCtSnapshot);
						
			break;
		}
		
		if (ct_hash_index >= NUM_CT_ENTRIES)
		{
			ct_hash_index = 0;
			if(pCtSnapshot)
			{
				Heap_Free(pCtSnapshot);
				pCtSnapshot = NULL;
			}
			
			ct_snapshot_buf_entries = 0;
			return ERR_CT_ENTRY_NOT_FOUND;
		}
	   
	}
	
	pCt = &pCtSnapshot[ct_snapshot_index++];
	SFL_memcpy(pCtCmd, pCt, sizeof(CtExCommand));
	if (ct_snapshot_index == ct_snapshot_entries)
	{
		ct_snapshot_index = 0;
		ct_hash_index ++;
	}

	return NO_ERR;	
		
}



/* This function returns total routes configured in a given hash index */
static int IPV4_Get_Hash_Routes(int rt_hash_index)
{
	int tot_routes = 0;
	struct slist_entry *entry;

	slist_for_each_entry(entry, &rt_cache[rt_hash_index])
	{
		tot_routes++;
	}

	return tot_routes;
}

/* This function fills the snapshot of route entries in a given hash index */
static int IPV4_RT_Get_Hash_Snapshot(int rt_hash_index,int rt_total_entries, PRtCommand pSnapshot)
{
	
	int tot_routes=0;
	PRouteEntry pRtEntry;
	struct slist_entry *entry;
	POnifDesc onif_desc;

	slist_for_each(pRtEntry, entry, &rt_cache[rt_hash_index], list)
	{
		COPY_MACADDR(pSnapshot->macAddr, pRtEntry->dstmac);

		onif_desc = get_onif_by_index(pRtEntry->itf->index);
		if (onif_desc)
			strcpy((char *)pSnapshot->outputDevice, (char *)onif_desc->name);
		else
			pSnapshot->outputDevice[0] = '\0';

		pSnapshot->mtu = pRtEntry->mtu;
		pSnapshot->id = pRtEntry->id;
#if !defined(COMCERTO_2000)
		if (pRtEntry->flags & EXTRA_INFO)
#endif
		SFL_memcpy( pSnapshot->daddr, ROUTE_EXTRA_INFO(pRtEntry), IPV6_ADDRESS_LENGTH);

		pSnapshot++;
		tot_routes++;

		if (--rt_total_entries <= 0)
			break;
	}

	return tot_routes;
}



/* This function creates the snapshot memory and returns the 
   next route entry from the snapshop of the route entries of a
   single hash to the caller  */

int IPV4_Get_Next_Hash_RtEntry(PRtCommand pRtCmd, int reset_action)
{
	int rt_hash_entries;
	PRtCommand pRt;
	static PRtCommand pRtSnapshot = NULL;
	static int rt_hash_index = 0, rt_snapshot_entries =0, rt_snapshot_index=0, rt_snapshot_buf_entries = 0;
	
	if(reset_action)
	{
		rt_hash_index = 0;
		rt_snapshot_entries =0;
		rt_snapshot_index=0;
		if(pRtSnapshot)
		{
			Heap_Free(pRtSnapshot);
			pRtSnapshot = NULL;
		}
		rt_snapshot_buf_entries = 0;
	}
	
	if (rt_snapshot_index == 0)
	{
		while( rt_hash_index < NUM_ROUTE_ENTRIES)
		{
		
			rt_hash_entries = IPV4_Get_Hash_Routes(rt_hash_index);
			if(rt_hash_entries == 0)
			{
				rt_hash_index++;
				continue;
			}
		   	
		   	if(rt_hash_entries > rt_snapshot_buf_entries)
		   	{
		   		if(pRtSnapshot)
		   			Heap_Free(pRtSnapshot);
		   		pRtSnapshot = Heap_Alloc(rt_hash_entries * sizeof(RtCommand));
			
				if (!pRtSnapshot)
				{
					rt_hash_index = 0;
					rt_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				rt_snapshot_buf_entries = rt_hash_entries;
		   	}
		
			rt_snapshot_entries = IPV4_RT_Get_Hash_Snapshot(rt_hash_index ,rt_hash_entries, pRtSnapshot);
								
			break;
		}
		
		if (rt_hash_index >= NUM_ROUTE_ENTRIES)
		{
			rt_hash_index = 0;
			if(pRtSnapshot)
			{
				Heap_Free(pRtSnapshot);
				pRtSnapshot = NULL;
			}
			rt_snapshot_buf_entries = 0;
			return ERR_RT_ENTRY_NOT_FOUND;
		}
		   
	}
	
	pRt = &pRtSnapshot[rt_snapshot_index++];
	SFL_memcpy(pRtCmd, pRt, sizeof(RtCommand));
	if (rt_snapshot_index == rt_snapshot_entries)
	{
		rt_snapshot_index = 0;
		rt_hash_index ++;
	}
		
	
	return NO_ERR;	
		
}
