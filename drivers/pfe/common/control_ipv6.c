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

#include "module_ipv6.h"
#include "module_timer.h"
#include "module_hidrv.h"
#include "module_ipsec.h"
#include "module_rtp_relay.h"
#include "Module_ipv6frag.h"
#include "fpp.h"
#include "module_socket.h"
#include "control_socket.h"



int IPv6_Get_Next_Hash_CTEntry(PCtExCommandIPv6 pV6CtCmd, int reset_action);


/**
 * IPv6_delete_CTpair()
 *
 *
 */
int IPv6_delete_CTpair(PCtEntryIPv6 ctEntry)
{
	PCtEntryIPv6 twin_entry;
	struct _tCtCommandIPv6 *message;
	HostMessage *pmsg;

	twin_entry = CT6_TWIN(ctEntry);
	if ((twin_entry->status & CONNTRACK_ORIG) == CONNTRACK_ORIG)
	{
		ctEntry = twin_entry;
		twin_entry = CT6_TWIN(ctEntry);
	}

	// Send indication message
	pmsg = msg_alloc();
	if (!pmsg)
		goto err;

	message = (struct _tCtCommandIPv6 *)pmsg->data;

	// Prepare indication message
	message->action = (ctEntry->status & CONNTRACK_TCP_FIN) ? ACTION_TCP_FIN : ACTION_REMOVED;
	SFL_memcpy(message->Saddr, ctEntry->Saddr_v6, IPV6_ADDRESS_LENGTH);
	SFL_memcpy(message->Daddr, ctEntry->Daddr_v6, IPV6_ADDRESS_LENGTH);
	message->Sport= ctEntry->Sport;
	message->Dport= ctEntry->Dport;
	SFL_memcpy(message->SaddrReply, twin_entry->Saddr_v6, IPV6_ADDRESS_LENGTH);
	SFL_memcpy(message->DaddrReply, twin_entry->Daddr_v6, IPV6_ADDRESS_LENGTH);
	message->SportReply= twin_entry->Sport;
	message->DportReply= twin_entry->Dport;
	message->protocol= GET_PROTOCOL(ctEntry); 
	message->fwmark = 0;

	pmsg->code = CMD_IPV6_CONNTRACK_CHANGE;
	pmsg->length = sizeof(*message);

	if (msg_send(pmsg) < 0)
		goto err;

	//Remove conntrack from list
	ct_remove((PCtEntry)ctEntry, (PCtEntry)twin_entry, Get_Ctentry_Hash(ctEntry), Get_Ctentry_Hash(twin_entry));

	return 0;

err:
	/* Can't send indication, try later from timeout routine */
	IP_expire((PCtEntry)ctEntry);
	IP_expire((PCtEntry)twin_entry);

	return 1;
}




/**
 * IPv6_handle_RESET
 *
 *	-- called from IPv4 reset handler
 *
 *
 */
int IPv6_handle_RESET(void)
{
	int rc = NO_ERR;

	/* free IPv4 sockets entries */
	SOCKET6_free_entries();

	return rc;
}


static PCtEntryIPv6 IPv6_find_ctentry(U32 hash, U32 *saddr, U32 *daddr, U16 sport, U16 dport)
{
	PCtEntryIPv6 pEntry;
	struct slist_entry *entry;

	slist_for_each(pEntry, entry, &ct_cache[hash], list)
	{
		// Note: we don't need to check for protocol match, since if all of the other fields match then the
		//	protocol will always match.
		if (IS_IPV6(pEntry) && !IPV6_CMP(pEntry->Saddr_v6, saddr) && !IPV6_CMP(pEntry->Daddr_v6, daddr) && pEntry->Sport == sport && pEntry->Dport == dport)
			return pEntry;
	}

	return NULL;
}


PCtEntryIPv6 IPv6_get_ctentry(U32 *saddr, U32 *daddr, U16 sport, U16 dport, U16 proto)
{
	U32 hash;
	hash = HASH_CT6(saddr, daddr, sport, dport, proto);
	return IPv6_find_ctentry(hash, saddr, daddr, sport, dport);
}


static int IPv6_HandleIP_Get_Timeout(U16 * p, U16 Length)
{
	int rc = NO_ERR;
	PTimeoutCommand TimeoutCmd;
	CtCommandIPv6 	Ctcmd;
	U32 			hash_key_ct_orig;
	PCtEntryIPv6	pEntry, twin_entry;
	// Check length
	if (Length != sizeof(CtCommandIPv6))
		return ERR_WRONG_COMMAND_SIZE;

	// Ensure alignment
	SFL_memcpy((U8*)&Ctcmd, (U8*)p,  Length);

	hash_key_ct_orig = HASH_CT6(Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport, Ctcmd.protocol);

	if ((pEntry = IPv6_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport)) != NULL)
	{
		BOOL bidir_flag;
		int timeout_value;
		U32 current_timer, twin_timer;
		memset(p, 0, 256);
		TimeoutCmd = (PTimeoutCommand)(p+1); // first word is for rc
		TimeoutCmd->protocol = GET_PROTOCOL(pEntry); 
		twin_entry = CT6_TWIN(pEntry);
		current_timer = ct_timer - pEntry->last_ct_timer;
		bidir_flag = twin_entry->last_ct_timer != UDP_REPLY_TIMER_INIT;
		if (bidir_flag)
		{
			twin_timer = ct_timer - twin_entry->last_ct_timer;
			if (twin_timer < current_timer)
				current_timer = twin_timer;
		}
		timeout_value = GET_TIMEOUT_VALUE(pEntry, bidir_flag) - current_timer;
		if (timeout_value < 0)
			timeout_value = 0;
		TimeoutCmd->timeout_value1 = (U32)timeout_value / CT_TICKS_PER_SECOND;
	}
	else
	{
		return CMD_ERR;
	}					
	
	
	return rc;
}





/**
 * IPv6_handle_CONNTRACK
 *
 *
 */
int IPv6_handle_CONNTRACK(U16 *p, U16 Length)
{
	PCtEntryIPv6 pEntry_orig = NULL, pEntry_rep = NULL;
	CtExCommandIPv6 Ctcmd;
	U32 hash_key_ct_orig, hash_key_ct_rep;
	int i, reset_action = 0;
	U8 sa_flow_orig_2dir = 0, sa_flow_rep_2dir = 0;

	/* Check length */
	/* Legacy support TO BE REMOVED */	
	if ((Length != sizeof(CtCommandIPv6)) && (Length != sizeof(CtExCommandIPv6)) && (Length != sizeof(CtExCommandIPv6_old)))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy((U8*)&Ctcmd, (U8*)p,  Length);

	hash_key_ct_orig = HASH_CT6(Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport, Ctcmd.protocol);
	hash_key_ct_rep = HASH_CT6(Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply, Ctcmd.protocol);
		
	switch(Ctcmd.action)
	{
		case ACTION_DEREGISTER: 

				pEntry_orig = IPv6_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport);
				pEntry_rep = IPv6_find_ctentry(hash_key_ct_rep, Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply);
				if (pEntry_orig == NULL || pEntry_rep == NULL ||
								CT6_TWIN(pEntry_orig) != pEntry_rep || CT6_TWIN(pEntry_rep) != pEntry_orig)
					return ERR_CT_ENTRY_NOT_FOUND;

				ct_remove((PCtEntry)pEntry_orig, (PCtEntry)pEntry_rep, hash_key_ct_orig, hash_key_ct_rep);
				break;
	
		case ACTION_REGISTER: //Add entry
		
				pEntry_orig = IPv6_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport);
				pEntry_rep = IPv6_find_ctentry(hash_key_ct_rep, Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply);
				if (pEntry_orig != NULL || pEntry_rep != NULL)
					    return ERR_CT_ENTRY_ALREADY_REGISTERED; //trying to add exactly the same conntrack

				if (Ctcmd.format & CT_SECURE) {
					if (Ctcmd.SA_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;
					for (i=0;i<Ctcmd.SA_nr;i++) {
						if (M_ipsec_sa_cache_lookup_by_h(Ctcmd.SA_handle[i]) == NULL)
							return ERR_CT_ENTRY_INVALID_SA; 
						}	

					if ((Ctcmd.SA_dir & (FLOW_DIR_FWD_BITVAL | FLOW_DIR_OUT_BITVAL)) == (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL))
						sa_flow_orig_2dir = 1;

					if (Ctcmd.SAReply_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;
					for (i=0;i<Ctcmd.SAReply_nr;i++) {
						if (M_ipsec_sa_cache_lookup_by_h(Ctcmd.SAReply_handle[i]) == NULL)
							return ERR_CT_ENTRY_INVALID_SA; 
					}	
					if ((Ctcmd.SAReply_dir & (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL)) == (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL))
						sa_flow_rep_2dir = 1;

				}

				/* originator -----------------------------*/					
				if ((pEntry_orig = (PCtEntryIPv6)ct_alloc()) == NULL)
				{
					return ERR_NOT_ENOUGH_MEMORY;
				}

				SFL_memcpy(pEntry_orig->Daddr_v6, Ctcmd.Daddr, IPV6_ADDRESS_LENGTH);
				SFL_memcpy(pEntry_orig->Saddr_v6, Ctcmd.Saddr, IPV6_ADDRESS_LENGTH);
	  			pEntry_orig->Sport = Ctcmd.Sport;
	  			pEntry_orig->Dport = Ctcmd.Dport;
				
				pEntry_orig->last_ct_timer = ct_timer;
				pEntry_orig->fwmark = Ctcmd.fwmark & 0xFFFF;
				pEntry_orig->status = CONNTRACK_ORIG | CONNTRACK_IPv6;

				if (Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED)
					pEntry_orig->status |= CONNTRACK_FF_DISABLED;

				pEntry_orig->route_id = Ctcmd.route_id;

	  			if (Ctcmd.format & CT_SECURE) {
	  				pEntry_orig->status |= CONNTRACK_SEC;
					for (i=0;i < SA_MAX_OP;i++) 
						pEntry_orig->hSAEntry[i] = 
						    (i<Ctcmd.SA_nr) ? Ctcmd.SA_handle[i] : 0;
					if (pEntry_orig->hSAEntry[0])
						pEntry_orig->status &= ~CONNTRACK_SEC_noSA;
					else
						pEntry_orig->status |= CONNTRACK_SEC_noSA;

					if (sa_flow_orig_2dir)
						pEntry_orig->status |= CONNTRACK_SEC_FLOW_BI_DIR;
					else
						pEntry_orig->status &= ~ CONNTRACK_SEC_FLOW_BI_DIR;
				}


				/* Replier ----------------------------------------*/ 	
				pEntry_rep = CT6_TWIN(pEntry_orig);
				SFL_memcpy(pEntry_rep->Daddr_v6, Ctcmd.DaddrReply, IPV6_ADDRESS_LENGTH);
				SFL_memcpy(pEntry_rep->Saddr_v6, Ctcmd.SaddrReply, IPV6_ADDRESS_LENGTH);
	  			pEntry_rep->Sport = Ctcmd.SportReply;
	  			pEntry_rep->Dport = Ctcmd.DportReply;
				
				
				if (Ctcmd.fwmark & 0x80000000)
					pEntry_rep->fwmark = ((Ctcmd.fwmark >> 16) & 0x7FFF) | (Ctcmd.fwmark & 0x8000);
				else
					pEntry_rep->fwmark = pEntry_orig->fwmark;
				pEntry_rep->status = CONNTRACK_IPv6;
				SET_PROTOCOL(pEntry_orig, pEntry_rep, Ctcmd.protocol);

	  			if(Ctcmd.protocol == IPPROTOCOL_UDP)
	  				pEntry_rep->last_ct_timer = UDP_REPLY_TIMER_INIT;
				else
					pEntry_rep->last_ct_timer = ct_timer;

				if (Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED)
					pEntry_rep->status |= CONNTRACK_FF_DISABLED;

				pEntry_rep->route_id = Ctcmd.route_id_reply;

	  			if (Ctcmd.format & CT_SECURE) {
	  				pEntry_rep->status |= CONNTRACK_SEC;
					for (i=0; i < SA_MAX_OP;i++) 
						pEntry_rep->hSAEntry[i]= 
						    (i<Ctcmd.SAReply_nr) ? Ctcmd.SAReply_handle[i] : 0;
					if (pEntry_rep->hSAEntry[0])
						pEntry_rep->status &= ~CONNTRACK_SEC_noSA;
					else
						pEntry_rep->status |= CONNTRACK_SEC_noSA;

					if (sa_flow_rep_2dir)
						pEntry_rep->status |= CONNTRACK_SEC_FLOW_BI_DIR;
					else
						pEntry_rep->status &= ~ CONNTRACK_SEC_FLOW_BI_DIR;

				}

				if ((Ctcmd.format & CT_ORIG_TUNNEL_SIT) && !(Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED))
				{
					pEntry_orig->tnl_route = L2_route_get(Ctcmd.tunnel_route_id);
					if (IS_NULL_ROUTE(pEntry_orig->tnl_route))
					{
						ct_free((PCtEntry)pEntry_orig);
						return ERR_RT_LINK_NOT_POSSIBLE;
					}
			  	}

				if ((Ctcmd.format & CT_REPL_TUNNEL_SIT) && !(Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED))
				{
					pEntry_rep->tnl_route = L2_route_get(Ctcmd.tunnel_route_id_reply);
					if (IS_NULL_ROUTE(pEntry_rep->tnl_route))
					{
						L2_route_put(pEntry_orig->tnl_route);
						ct_free((PCtEntry)pEntry_orig);
						return ERR_RT_LINK_NOT_POSSIBLE;
					}
			  	}

				// Check for NAT processing
				if (IPV6_CMP(Ctcmd.Saddr, Ctcmd.DaddrReply) || (Ctcmd.Sport != Ctcmd.DportReply))
				{
					pEntry_orig->status |= CONNTRACK_SNAT;
					pEntry_rep->status |= CONNTRACK_DNAT;
					if (Ctcmd.Sport != Ctcmd.DportReply)
					{
						pEntry_orig->status |= CONNTRACK_IPv6_PORTNAT;
						pEntry_rep->status |= CONNTRACK_IPv6_PORTNAT;
					}
				}
				if (IPV6_CMP(Ctcmd.Daddr, Ctcmd.SaddrReply) || (Ctcmd.Dport != Ctcmd.SportReply))
				{
					pEntry_orig->status |= CONNTRACK_DNAT;
					pEntry_rep->status |= CONNTRACK_SNAT;
					if (Ctcmd.Dport != Ctcmd.SportReply)
					{
						pEntry_orig->status |= CONNTRACK_IPv6_PORTNAT;
						pEntry_rep->status |= CONNTRACK_IPv6_PORTNAT;
					}
				}
				if (pEntry_orig->status & (CONNTRACK_SNAT | CONNTRACK_DNAT))
				{
					/* Check sum correction pre-computation RFC1624 */
					U32 sum = 0;
					for (i = 0; i < 4; i++)
					{
						sum += (Ctcmd.Daddr[i] & 0xffff) +
									(Ctcmd.Daddr[i] >> 16) +
									((Ctcmd.SaddrReply[i] & 0xffff) ^ 0xffff) +
									((Ctcmd.SaddrReply[i] >> 16) ^ 0xffff);
						sum += (Ctcmd.Saddr[i] & 0xffff) +
									(Ctcmd.Saddr[i] >> 16) +
									((Ctcmd.DaddrReply[i] & 0xffff) ^ 0xffff) +
									((Ctcmd.DaddrReply[i] >> 16) ^ 0xffff);
					}
					sum += Ctcmd.Dport + (Ctcmd.SportReply ^ 0xffff);
					sum += Ctcmd.Sport + (Ctcmd.DportReply ^ 0xffff);
					while (sum >> 16)
						sum = (sum & 0xffff) + (sum >> 16);
					if (sum == 0xffff)
						sum = 0;
					pEntry_orig->tcp_udp_chksm_corr = sum;
					pEntry_rep->tcp_udp_chksm_corr = sum == 0 ? 0 : sum ^ 0xffff;
				}

				/* Everything went Ok. We can safely put querier and replier entries in hash tables */

				return ct_add((PCtEntry)pEntry_orig, (PCtEntry)pEntry_rep, hash_key_ct_orig, hash_key_ct_rep);

		case ACTION_UPDATE: 

				pEntry_orig = IPv6_find_ctentry(hash_key_ct_orig, Ctcmd.Saddr, Ctcmd.Daddr, Ctcmd.Sport, Ctcmd.Dport);
				pEntry_rep = IPv6_find_ctentry(hash_key_ct_rep, Ctcmd.SaddrReply, Ctcmd.DaddrReply, Ctcmd.SportReply, Ctcmd.DportReply);
				// Check for errors before changing anything
				if (pEntry_orig == NULL || pEntry_rep == NULL ||
								CT6_TWIN(pEntry_orig) != pEntry_rep || CT6_TWIN(pEntry_rep) != pEntry_orig)
					return ERR_CT_ENTRY_NOT_FOUND;

				if ((Ctcmd.format & CT_ORIG_TUNNEL_SIT) && !(Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED))
				{
					PRouteEntry tnl_route;
					tnl_route = L2_route_get(Ctcmd.tunnel_route_id);  // do "dry run"
					if (IS_NULL_ROUTE(tnl_route))
						return ERR_RT_LINK_NOT_POSSIBLE;
					L2_route_put(tnl_route);			  // undo "dry run"
			  	}
				if ((Ctcmd.format & CT_REPL_TUNNEL_SIT) && !(Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED))
				{
					PRouteEntry tnl_route;
					tnl_route = L2_route_get(Ctcmd.tunnel_route_id_reply);  // do "dry run"
					if (IS_NULL_ROUTE(tnl_route))
						return ERR_RT_LINK_NOT_POSSIBLE;
					L2_route_put(tnl_route);			  // undo "dry run"
			  	}
				if (Ctcmd.format & CT_SECURE) {
					if (Ctcmd.SA_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;

					for (i = 0; i < Ctcmd.SA_nr; i++) {
						if (pEntry_orig->hSAEntry[i] != Ctcmd.SA_handle[i])
							if (M_ipsec_sa_cache_lookup_by_h(Ctcmd.SA_handle[i]) == NULL)
								return ERR_CT_ENTRY_INVALID_SA; 
					}
					if ((Ctcmd.SA_dir & (FLOW_DIR_FWD_BITVAL | FLOW_DIR_OUT_BITVAL)) == (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL))
						sa_flow_orig_2dir = 1;

					if (Ctcmd.SAReply_nr > SA_MAX_OP)
						return ERR_CT_ENTRY_TOO_MANY_SA_OP;

					for (i = 0; i < Ctcmd.SAReply_nr; i++) {
						if (pEntry_rep->hSAEntry[i] != Ctcmd.SAReply_handle[i])
							if (M_ipsec_sa_cache_lookup_by_h(Ctcmd.SAReply_handle[i]) == NULL)
								return ERR_CT_ENTRY_INVALID_SA;
					if ((Ctcmd.SAReply_dir & (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL)) == (FLOW_DIR_OUT_BITVAL | FLOW_DIR_FWD_BITVAL))
						sa_flow_rep_2dir = 1;
					}
				}

				pEntry_orig->fwmark = Ctcmd.fwmark & 0xFFFF;

				if (Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED) {
					pEntry_orig->status |= CONNTRACK_FF_DISABLED;
					IP_delete_CT_route((PCtEntry)pEntry_orig);
				} else
					pEntry_orig->status &= ~CONNTRACK_FF_DISABLED;

				if (Ctcmd.format & CT_SECURE) {
					pEntry_orig->status |= CONNTRACK_SEC;

					for (i = 0;i < SA_MAX_OP; i++)
						pEntry_orig->hSAEntry[i] = 
						    (i<Ctcmd.SA_nr) ? Ctcmd.SA_handle[i] : 0;

					if (pEntry_orig->hSAEntry[0])
						pEntry_orig->status &= ~ CONNTRACK_SEC_noSA;
					else 
						pEntry_orig->status |= CONNTRACK_SEC_noSA;

					if (sa_flow_orig_2dir)
						pEntry_orig->status |= CONNTRACK_SEC_FLOW_BI_DIR;
					else
						pEntry_orig->status &= ~ CONNTRACK_SEC_FLOW_BI_DIR;
				} else
					pEntry_orig->status &= ~CONNTRACK_SEC;

				if (Ctcmd.fwmark & 0x80000000)
					pEntry_rep->fwmark = ((Ctcmd.fwmark >> 16) & 0x7FFF) | (Ctcmd.fwmark & 0x8000);
				else
					pEntry_rep->fwmark = pEntry_orig->fwmark;

				if (Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED) {
					pEntry_rep->status |= CONNTRACK_FF_DISABLED;
					IP_delete_CT_route((PCtEntry)pEntry_rep);
				} else
					pEntry_rep->status &= ~CONNTRACK_FF_DISABLED;

				if (Ctcmd.format & CT_SECURE) {
					pEntry_rep->status |= CONNTRACK_SEC;

					for (i = 0; i < SA_MAX_OP; i++) 
						pEntry_rep->hSAEntry[i]= 
						    (i<Ctcmd.SAReply_nr) ? (Ctcmd.SAReply_handle[i]) : 0;

					if ( pEntry_rep->hSAEntry[0])
						pEntry_rep->status &= ~CONNTRACK_SEC_noSA;
					else 
						pEntry_rep->status |= CONNTRACK_SEC_noSA;

					if (sa_flow_rep_2dir)
						pEntry_rep->status |= CONNTRACK_SEC_FLOW_BI_DIR;
					else
						pEntry_rep->status &= ~ CONNTRACK_SEC_FLOW_BI_DIR;
				} else
					pEntry_rep->status &= ~CONNTRACK_SEC;


				/* Update route entries if needed */
				if (IS_NULL_ROUTE(pEntry_orig->pRtEntry))
				{
					pEntry_orig->route_id = Ctcmd.route_id;
				}
				else if (pEntry_orig->route_id != Ctcmd.route_id)
				{
					IP_delete_CT_route((PCtEntry)pEntry_orig);
					pEntry_orig->route_id = Ctcmd.route_id;
				}

				if (IS_NULL_ROUTE(pEntry_rep->pRtEntry))
				{
					pEntry_rep->route_id = Ctcmd.route_id_reply;
				}
				else if (pEntry_rep->route_id != Ctcmd.route_id_reply)
				{
					IP_delete_CT_route((PCtEntry)pEntry_rep);
					pEntry_rep->route_id = Ctcmd.route_id_reply;
				}

				if (pEntry_orig->tnl_route)
				{
					L2_route_put(pEntry_orig->tnl_route);
					pEntry_orig->tnl_route = NULL;
				}
				if ((Ctcmd.format & CT_ORIG_TUNNEL_SIT) && !(Ctcmd.flags & CTCMD_FLAGS_ORIG_DISABLED))
				{
					pEntry_orig->tnl_route = L2_route_get(Ctcmd.tunnel_route_id);
			  	}

				if (pEntry_rep->tnl_route)
				{
					L2_route_put(pEntry_rep->tnl_route);
					pEntry_rep->tnl_route = NULL;
				}
				if ((Ctcmd.format & CT_REPL_TUNNEL_SIT) && !(Ctcmd.flags & CTCMD_FLAGS_REP_DISABLED))
				{
					pEntry_rep->tnl_route = L2_route_get(Ctcmd.tunnel_route_id_reply);
			  	}

				ct_update((PCtEntry)pEntry_orig, (PCtEntry)pEntry_rep, hash_key_ct_orig, hash_key_ct_rep);
				return NO_ERR;

		case ACTION_QUERY:
			reset_action = 1;

		case ACTION_QUERY_CONT:
		{
			PCtExCommandIPv6 pCt = (CtExCommandIPv6*)p;
			int rc;

			rc = IPv6_Get_Next_Hash_CTEntry(pCt, reset_action);

			return rc;
		}

		default :
			return ERR_UNKNOWN_COMMAND;

	}

	return NO_ERR;

}


/**
 * M_ipv6_cmdproc
 *
 *
 *
 */
U16 M_ipv6_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 querySize = 0;
	U16 action;

	switch (cmd_code)
	{
		case CMD_IPV6_CONNTRACK:			
			action = *pcmd;
			rc = IPv6_handle_CONNTRACK(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				querySize = sizeof(CtExCommandIPv6);
			break;

		case CMD_IPV6_RESET:			
			//now handled as part of IPv4 reset -- just return success
			rc = NO_ERR;
			break;

		case CMD_IPV6_GET_TIMEOUT:
			rc = IPv6_HandleIP_Get_Timeout(pcmd, cmd_len);
			if (rc == NO_ERR)
				querySize = sizeof(TimeoutCommand);
			break;

		case CMD_IPV6_SOCK_OPEN:
			rc = SOCKET6_HandleIP_Socket_Open(pcmd, cmd_len);
			break;

		case CMD_IPV6_SOCK_CLOSE:
			rc = SOCKET6_HandleIP_Socket_Close(pcmd, cmd_len);
			break;

		case CMD_IPV6_SOCK_UPDATE:
			rc = SOCKET6_HandleIP_Socket_Update(pcmd, cmd_len);
			break;

		case CMD_IPV6_FRAGTIMEOUT:
			rc = IPv6_HandleIP_Set_FragTimeout(pcmd, cmd_len);
			break;

		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;
	return 2 + querySize;
}


int ipv6_init(void)
{
#if !defined(COMCERTO_2000)
	set_event_handler(EVENT_IPV6, M_ipv6_entry);
#endif

	set_cmd_handler(EVENT_IPV6, M_ipv6_cmdproc);


	return 0;
}

void ipv6_exit(void)
{

}
