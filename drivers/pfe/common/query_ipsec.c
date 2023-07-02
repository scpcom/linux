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

#include "module_ipsec.h"
#include "module_tunnel.h"
#include "fe.h"


/* This function returns total SA entries configured with a given handle */
static int IPsec_Get_Hash_SAEntries(int sa_handle_index)
{
	
	int tot_sa_entries = 0;
	PSAEntry  pSAEntry;
	struct slist_entry *entry;
	
	slist_for_each(pSAEntry, entry, &sa_cache_by_h[sa_handle_index], list_h)
	{
		tot_sa_entries++;
	}
		

	return tot_sa_entries;

}

/* This function fills the snapshot of SA entries in a given handle hash index */
static int IPsec_SA_Get_Handle_Snapshot(int sa_handle_index, int sa_entries, PSAQueryCommand pSAHandleSnapshot)
{
	
	int tot_sa_entries=0;
	PSAEntry pSAEntry;
	struct slist_entry *entry;
			
	slist_for_each(pSAEntry, entry, &sa_cache_by_h[sa_handle_index], list_h)
	{

	    pSAHandleSnapshot->src_ip[0] = pSAEntry->id.saddr[0];
	    pSAHandleSnapshot->src_ip[1] = pSAEntry->id.saddr[1];
	    pSAHandleSnapshot->src_ip[2] = pSAEntry->id.saddr[2];
	    pSAHandleSnapshot->src_ip[3] = pSAEntry->id.saddr[3];
	    pSAHandleSnapshot->dst_ip[0] = pSAEntry->id.daddr.a6[0];
	    pSAHandleSnapshot->dst_ip[1] = pSAEntry->id.daddr.a6[1];
	    pSAHandleSnapshot->dst_ip[2] = pSAEntry->id.daddr.a6[2];
	    pSAHandleSnapshot->dst_ip[3] = pSAEntry->id.daddr.a6[3];
	    pSAHandleSnapshot->spi       = pSAEntry->id.spi;
	    pSAHandleSnapshot->sa_type 	 = pSAEntry->id.proto;
	    pSAHandleSnapshot->family 	 = pSAEntry->family;
	    pSAHandleSnapshot->handle 	 = pSAEntry->handle;
	    pSAHandleSnapshot->mtu	 	 = pSAEntry->mtu;
	    pSAHandleSnapshot->state 	 = pSAEntry->state;
	    	
	    if (pSAEntry->elp_sa)
	    {
	    	pSAHandleSnapshot->flags = pSAEntry->elp_sa->flags;
	    	if (pSAEntry->elp_sa->flags & ESPAH_ANTI_REPLAY_ENABLE)
	       		pSAHandleSnapshot->replay_window = 0x1;
	    	else
	    		pSAHandleSnapshot->replay_window = 0x0;
	    	
	   
	    	pSAHandleSnapshot->key_alg   = pSAEntry->elp_sa->algo;
	    	if (pSAEntry->elp_sa->algo & 0x7)
	    	{
   		
	    		
#if defined(COMCERTO_1000) || defined(COMCERTO_2000)
				*((U32*)&pSAHandleSnapshot->auth_key[0]) = htonl(*(U32*)&pSAEntry->elp_sa->auth_key0);
				*((U32*)&pSAHandleSnapshot->auth_key[4]) = htonl(*(U32*)&pSAEntry->elp_sa->auth_key1);
				*((U32*)&pSAHandleSnapshot->auth_key[8]) = htonl(*(U32*)&pSAEntry->elp_sa->auth_key2);
				*((U32*)&pSAHandleSnapshot->auth_key[12]) = htonl(*(U32*)&pSAEntry->elp_sa->auth_key3);
				*((U32*)&pSAHandleSnapshot->auth_key[16]) = htonl(*(U32*)&pSAEntry->elp_sa->auth_key4);
				
				*((U32*)&pSAHandleSnapshot->ext_auth_key[0]) = htonl(*(U32*)&pSAEntry->elp_sa->ext_auth_key0);
				*((U32*)&pSAHandleSnapshot->ext_auth_key[4]) = htonl(*(U32*)&pSAEntry->elp_sa->ext_auth_key1);
				*((U32*)&pSAHandleSnapshot->ext_auth_key[8]) = htonl(*(U32*)&pSAEntry->elp_sa->ext_auth_key2);
			
#else
				int i;
			   for (i =0; i < 5; i++)
					*((U32*)&pSAHandleSnapshot->auth_key[i*4]) = htonl(*(U32*)&pSAEntry->elp_sa->auth_key[i*4]);
				
				for (i =0; i < 3; i++)
					*((U32*)&pSAHandleSnapshot->ext_auth_key[i*4]) = htonl(*(U32*)&pSAEntry->elp_sa->ext_auth_key[i*4]);
				
#endif
	    	}
	    	if ((pSAEntry->elp_sa->algo >> 3) & 0x1F)
	    	{
#if defined(COMCERTO_1000) || defined(COMCERTO_2000)
	    		*((U32*)&pSAHandleSnapshot->cipher_key[0]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher0);
				*((U32*)&pSAHandleSnapshot->cipher_key[4]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher1);
				*((U32*)&pSAHandleSnapshot->cipher_key[8]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher2);
				*((U32*)&pSAHandleSnapshot->cipher_key[12]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher3);
				*((U32*)&pSAHandleSnapshot->cipher_key[16]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher4);
				*((U32*)&pSAHandleSnapshot->cipher_key[20]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher5);
				*((U32*)&pSAHandleSnapshot->cipher_key[24]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher6);
				*((U32*)&pSAHandleSnapshot->cipher_key[28]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher7);
#else	
				int i;
				for (i =0; i < 7; i++)
					*((U32*)&pSAHandleSnapshot->cipher_key[i*4]) = htonl(*(U32*)&pSAEntry->elp_sa->cipher_key[i*4]);
				
#endif
	    	}
	    	
	    
	    }
	   

	    if (pSAEntry->mode == SA_MODE_TUNNEL)
	    {
	    	pSAHandleSnapshot->mode = SA_MODE_TUNNEL;
	    	if (pSAEntry->header_len == IPV4_HDR_SIZE)
	    	{
			pSAHandleSnapshot->tunnel_proto_family = PROTO_FAMILY_IPV4;
			pSAHandleSnapshot->tnl.ipv4.DestinationAddress = pSAEntry->tunnel.ip4.DestinationAddress;	    		
			pSAHandleSnapshot->tnl.ipv4.SourceAddress = pSAEntry->tunnel.ip4.SourceAddress;
			pSAHandleSnapshot->tnl.ipv4.TypeOfService   = pSAEntry->tunnel.ip4.TypeOfService;
			pSAHandleSnapshot->tnl.ipv4.Protocol = pSAEntry->tunnel.ip4.Protocol;
			pSAHandleSnapshot->tnl.ipv4.TotalLength = pSAEntry->tunnel.ip4.TotalLength;	
	    	}
	    	else
	    	{
			pSAHandleSnapshot->tunnel_proto_family = PROTO_FAMILY_IPV6;
			pSAHandleSnapshot->tnl.ipv6.DestinationAddress[0] = pSAEntry->tunnel.ip6.DestinationAddress[0];
			pSAHandleSnapshot->tnl.ipv6.DestinationAddress[1] = pSAEntry->tunnel.ip6.DestinationAddress[1];
			pSAHandleSnapshot->tnl.ipv6.DestinationAddress[2] = pSAEntry->tunnel.ip6.DestinationAddress[2];
			pSAHandleSnapshot->tnl.ipv6.DestinationAddress[3] = pSAEntry->tunnel.ip6.DestinationAddress[3];
				
			pSAHandleSnapshot->tnl.ipv6.SourceAddress[0] = pSAEntry->tunnel.ip6.SourceAddress[0];
			pSAHandleSnapshot->tnl.ipv6.SourceAddress[1] = pSAEntry->tunnel.ip6.SourceAddress[1];
			pSAHandleSnapshot->tnl.ipv6.SourceAddress[2] = pSAEntry->tunnel.ip6.SourceAddress[2];
			pSAHandleSnapshot->tnl.ipv6.SourceAddress[3] = pSAEntry->tunnel.ip6.SourceAddress[3];
			IPV6_SET_TRAFFIC_CLASS(&pSAHandleSnapshot->tnl.ipv6, IPV6_GET_TRAFFIC_CLASS(&pSAEntry->tunnel.ip6));
			IPV6_SET_VERSION(&pSAHandleSnapshot->tnl.ipv6, IPV6_GET_VERSION(&pSAEntry->tunnel.ip6));
			IPV6_COPY_FLOW_LABEL(&pSAHandleSnapshot->tnl.ipv6, &pSAEntry->tunnel.ip6);
	    	}
	    }
	    
	   
	    pSAHandleSnapshot->soft_byte_limit = pSAEntry->lft_conf.soft_byte_limit;
	    pSAHandleSnapshot->hard_byte_limit = pSAEntry->lft_conf.hard_byte_limit;
	    pSAHandleSnapshot->soft_packet_limit = pSAEntry->lft_conf.soft_packet_limit;
	    pSAHandleSnapshot->hard_packet_limit = pSAEntry->lft_conf.hard_packet_limit;

	    	
	    
	    pSAHandleSnapshot++;
	    tot_sa_entries++;
	    if(--sa_entries <= 0)
		break;
	}
		
	
	return tot_sa_entries;

}


/* This function creates the snapshot memory  for SAs and returns the 
   next SA entry from the snapshot of the SA entries of a
   single handle to the caller  */
   
int IPsec_Get_Next_SAEntry(PSAQueryCommand  pSAQueryCmd, int reset_action)
{
	int ipsec_sa_hash_entries;
	PSAQueryCommand pSACmd;
	static PSAQueryCommand pSASnapshot = NULL;
	static int sa_hash_index = 0, sa_snapshot_entries =0, sa_snapshot_index=0 , sa_snapshot_buf_entries = 0;
	
	if(reset_action)
	{
		sa_hash_index = 0;
		sa_snapshot_entries =0;
		sa_snapshot_index=0;
		if(pSASnapshot)
		{
			Heap_Free(pSASnapshot);
			pSASnapshot = NULL;
		}
		sa_snapshot_buf_entries = 0;
	}
	
	if (sa_snapshot_index == 0)
	{
		while( sa_hash_index < NUM_SA_ENTRIES)
		{
		
			ipsec_sa_hash_entries = IPsec_Get_Hash_SAEntries(sa_hash_index);
			if(ipsec_sa_hash_entries == 0)
			{
				sa_hash_index++;
				continue;
			}
	   	
	   		if(ipsec_sa_hash_entries > sa_snapshot_buf_entries)
	   		{
	   			if(pSASnapshot)
	   				Heap_Free(pSASnapshot);
	   			   		
				pSASnapshot = Heap_Alloc(ipsec_sa_hash_entries * sizeof(SAQueryCommand));
			
				if (!pSASnapshot)
				{
			    	sa_hash_index = 0;
			    	sa_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				sa_snapshot_buf_entries = ipsec_sa_hash_entries;
	   		}
			sa_snapshot_entries = IPsec_SA_Get_Handle_Snapshot(sa_hash_index , ipsec_sa_hash_entries,pSASnapshot);
			
			break;
		}
		
		if (sa_hash_index >= NUM_SA_ENTRIES)
		{
			sa_hash_index = 0;
			if (pSASnapshot)
			{
				Heap_Free(pSASnapshot);
				pSASnapshot = NULL;	
			}
			sa_snapshot_buf_entries = 0;
			return ERR_SA_ENTRY_NOT_FOUND;
		}
		   
	}


	pSACmd = &pSASnapshot[sa_snapshot_index++];
	SFL_memcpy(pSAQueryCmd, pSACmd, sizeof(SAQueryCommand));
	if (sa_snapshot_index == sa_snapshot_entries)
	{
		sa_snapshot_index = 0;
		sa_hash_index ++;
	}

	
	return NO_ERR;	
		
}




#ifdef CFG_STATS

/* This function fills the snapshot of SA entries 
   in a given hash index, statReset = 1 resets the stats of SA. */

static int stat_Get_SA_Hash_Snapshot(int sa_hash_index, int sa_entries,
                                     PStatIpsecEntryResponse pStatSASnapshot)
{
	
	int tot_sa_entries=0;
	PSAEntry pSAEntry;
	struct slist_entry *entry;
	u32 last_bytes, last_bytes_up32;
		
	slist_for_each(pSAEntry, entry, &sa_cache_by_h[sa_hash_index], list_h)
	{
		pStatSASnapshot->spi  		= ntohl(pSAEntry->id.spi);
		pStatSASnapshot->proto		= pSAEntry->id.proto;
		pStatSASnapshot->family		= pSAEntry->family;
		pStatSASnapshot->dstIP[0] 	= pSAEntry->id.daddr.a6[0];
		pStatSASnapshot->dstIP[1] 	= pSAEntry->id.daddr.a6[1];
		pStatSASnapshot->dstIP[2] 	= pSAEntry->id.daddr.a6[2];
		pStatSASnapshot->dstIP[3] 	= pSAEntry->id.daddr.a6[3];
		
#if defined(COMCERTO_2000)
		pStatSASnapshot->total_pkts_processed 		= 
		                    be32_to_cpu(pSAEntry->hw_sa->stats.last_pkts_processed);
		last_bytes = pSAEntry->hw_sa->stats.last_bytes_processed & 0xffffffff;
		last_bytes_up32=  (pSAEntry->hw_sa->stats.last_bytes_processed >> 32)& 0xffffffff;

		pStatSASnapshot->total_bytes_processed[0] = be32_to_cpu(last_bytes_up32);
		pStatSASnapshot->total_bytes_processed[1] = be32_to_cpu(last_bytes);
#else
              pStatSASnapshot->total_pkts_processed           =
                                    pSAEntry->total_pkts_processed;
                pStatSASnapshot->total_bytes_processed[0]       =
                                    pSAEntry->total_bytes_processed & 0xffffffff;

                pStatSASnapshot->total_bytes_processed[1]       =
                                    (pSAEntry->total_bytes_processed >> 32) & 0xffffffff;

#endif
		pStatSASnapshot->eof = 0;
		
#if !defined(COMCERTO_2000)
		if(gStatIpsecQueryStatus & STAT_IPSEC_QUERY_RESET)
		{
			pSAEntry->total_pkts_processed =  pSAEntry->total_bytes_processed = 0;	
		}
#endif
	
		pStatSASnapshot->sagd = pSAEntry->handle;
			
		pStatSASnapshot++;
		tot_sa_entries++;

		if(--sa_entries <= 0)
			break;

	}
		
	printk (KERN_ERR "total_sa_entries:%x\n", tot_sa_entries);
	return tot_sa_entries;

}

/* This function creates the snapshot memory and returns the 
   next SA entry from the snapshop of the SA entries of a
   single hash to the caller (statReset = 1 resets the stats) */

int stat_Get_Next_SAEntry(PStatIpsecEntryResponse pSACmd, int reset_action)
{
	int stat_sa_hash_entries;
	PStatIpsecEntryResponse pSA;
	static PStatIpsecEntryResponse pStatSASnapshot = NULL;
	static int stat_sa_hash_index = 0, stat_sa_snapshot_entries =0, stat_sa_snapshot_index=0, stat_sa_snapshot_buf_entries = 0;
	
	if(reset_action)
	{
		stat_sa_hash_index = 0;
		stat_sa_snapshot_entries =0;
		stat_sa_snapshot_index=0;
		if(pStatSASnapshot)
		{
			Heap_Free(pStatSASnapshot);
			pStatSASnapshot = NULL;	
		}
		stat_sa_snapshot_buf_entries = 0;
		return NO_ERR;
	}
	
	if (stat_sa_snapshot_index == 0)
	{
		while( stat_sa_hash_index < NUM_SA_ENTRIES)
		{
		
			stat_sa_hash_entries = IPsec_Get_Hash_SAEntries(stat_sa_hash_index);
			if(stat_sa_hash_entries == 0)
			{
				stat_sa_hash_index++;
				continue;
			}
		   	
		   	if (stat_sa_hash_entries > stat_sa_snapshot_buf_entries)
		   	{
		   		if(pStatSASnapshot)
					Heap_Free(pStatSASnapshot);	   	
				pStatSASnapshot = Heap_Alloc(stat_sa_hash_entries * sizeof(StatIpsecEntryResponse));
			
				if (!pStatSASnapshot)
				{
			    	stat_sa_hash_index = 0;
			    	stat_sa_snapshot_buf_entries = 0;
					return ERR_NOT_ENOUGH_MEMORY;
				}
				stat_sa_snapshot_buf_entries = 	stat_sa_hash_entries;
		   	}
			stat_sa_snapshot_entries = stat_Get_SA_Hash_Snapshot(stat_sa_hash_index , stat_sa_hash_entries, pStatSASnapshot);
			break;
		}
		
		if (stat_sa_hash_index >= NUM_SA_ENTRIES)
		{
			stat_sa_hash_index = 0;
			if(pStatSASnapshot)
			{
				Heap_Free(pStatSASnapshot);
				pStatSASnapshot = NULL;
			}
			stat_sa_snapshot_buf_entries = 0;
			return ERR_SA_ENTRY_NOT_FOUND;
		}
		   
	}
	
	pSA = &pStatSASnapshot[stat_sa_snapshot_index++];
	SFL_memcpy(pSACmd, pSA, sizeof(StatIpsecEntryResponse));
	if (stat_sa_snapshot_index == stat_sa_snapshot_entries)
	{
		stat_sa_snapshot_index = 0;
		stat_sa_hash_index ++;
	}
		
	
	return NO_ERR;	
}

#endif /* CFG_STATS */
