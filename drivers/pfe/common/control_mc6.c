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

#include "module_mc6.h"
#include "types.h"
#include "system.h"
#include "fpp.h"
#include "module_ipv6.h"
#include "module_mc4.h"
#include "module_hidrv.h"
#include "module_timer.h"
#include "module_tx.h"
#include "module_Rx.h"
#include "gemac.h"

#include "fe.h"
#include "layer2.h"
#include "module_expt.h"
#include "alt_conf.h"
#include "module_rtp_relay.h"
#include "module_qm.h"
#include "module_wifi.h"

#if defined(COMCERTO_2000)
#include "control_common.h"

struct dma_pool *mc6_dma_pool;
extern TIMER_ENTRY mc6_delayed_remove_timer;
extern TIMER_ENTRY mc6_timer;
struct dlist_head hw_mc6_active_list[MC6_NUM_HASH_ENTRIES];
struct dlist_head hw_mc6_removal_list;
#endif

#if !defined(COMCERTO_2000)
PVOID MC6_entry_alloc(void)
{
	return Heap_Alloc(sizeof(struct _tMC6Entry));
}

void MC6_entry_free(PMC6Entry this_entry)
{
	Heap_Free((PVOID) this_entry);
}

static int MC6_entry_add(PMC6Entry this_entry, U32 hash)
{
	/* Add software entry to local hash */
	slist_add(&mc6_table_memory[hash], &this_entry->list);

	/* check if rtp stats entry is created for this conntrack, if found link the two object and mark the conntrack 's status field for RTP stats */
	rtpqos_mc6_link_stats_entry_by_tuple(this_entry, this_entry->src_addr, this_entry->dst_addr);

	return NO_ERR;
}

static void MC6_entry_remove(PMC6Entry this_entry, U32 hash)
{
	slist_remove(&mc6_table_memory[hash], &this_entry->list);
	MC6_entry_free(this_entry);
}

void MC6_entry_update(PMC6Entry this_entry)
{

}

void MC6_mode_update(void)
{

}

#else


static void MC6_entry_add_to_pe(U32 dma_addr, U32 hash)
{
	int id;
	U32 *pe_addr;

	pe_addr = (U32*)&mc6_table_memory[hash];

	for (id=CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_write(id, dma_addr, virt_to_class_dmem(pe_addr), 4);
}

void MC6_mode_update(void)
{
	int id;
	U32 *pe_addr;

	pe_addr = &CLASS_VARNAME2(g_mc6_mode);

	for (id=CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_write(id, cpu_to_be32(g_mc6_mode), virt_to_class_dmem(pe_addr), 4);

}

void MC6_entry_update(PMC6Entry this_entry)
{
	int i;
	Phw_MC6Entry hw_entry = this_entry->hw_entry;

	hw_entry_set_flags(&this_entry->hw_entry->flags, HASH_ENTRY_UPDATING | HASH_ENTRY_VALID);

	/* FIXME: could be enhanced with a reworked hw_entry structure layout + memcpy */
	SFL_memcpy(hw_entry->src_addr, this_entry->src_addr, IPV6_ADDRESS_LENGTH);
	SFL_memcpy(hw_entry->dst_addr, this_entry->dst_addr, IPV6_ADDRESS_LENGTH);
	hw_entry->status = cpu_to_be16(this_entry->status);
	hw_entry->rtpqos_slot = this_entry->rtpqos_slot;

	memcpy(&hw_entry->mcdest, &this_entry->mcdest, sizeof(MCxEntry));
	hw_entry->mcdest.wifi_listener_timer = cpu_to_be32(this_entry->mcdest.wifi_listener_timer);
	for (i=0; i < this_entry->mcdest.num_listeners; i++) {
		hw_entry->mcdest.listeners[i].itf = (struct itf *) cpu_to_be32(virt_to_class_dmem(this_entry->mcdest.listeners[i].itf));
		hw_entry->mcdest.listeners[i].timer = cpu_to_be32(this_entry->mcdest.listeners[i].timer);
	}

	hw_entry_set_flags(&this_entry->hw_entry->flags, HASH_ENTRY_VALID);
}


static void hw_MC6_entry_schedule_remove(struct _thw_MC6Entry *hw_entry)
{
	hw_entry->removal_time = jiffies + 2;
	dlist_add(&hw_mc6_removal_list, &hw_entry->list);
}

/** Processes hardware socket delayed removal list.
* Free all hardware socket in the removal list that have reached their removal time.
*
*
*/
static void hw_MC6_entry_delayed_remove(void)
{
	struct dlist_head *entry;
	struct _thw_MC6Entry *hw_entry;

	dlist_for_each_safe(hw_entry, entry, &hw_mc6_removal_list, list)
	{
		if (!time_after(jiffies, hw_entry->removal_time))
			continue;

		dlist_remove(&hw_entry->list);

		dma_pool_free(mc6_dma_pool, hw_entry, be32_to_cpu(hw_entry->dma_addr));
	}
}


static PVOID MC6_entry_alloc(void)
{
	return pfe_kzalloc(sizeof(struct _tMC6Entry), GFP_KERNEL);
}

static void MC6_entry_free(PMC6Entry this_entry)
{
	pfe_kfree(this_entry);
}

static int MC6_entry_add(PMC6Entry this_entry, U32 hash)
{
	struct _thw_MC6Entry *hw_entry;
	struct _thw_MC6Entry *hw_entry_first;
	dma_addr_t dma_addr;
	int rc  = NO_ERR;

	/* Allocate hardware entry */
	hw_entry = dma_pool_alloc(mc6_dma_pool, GFP_ATOMIC, &dma_addr);
	if (!hw_entry)
	{
		rc = ERR_NOT_ENOUGH_MEMORY;
		goto err;
	}

	hw_entry->dma_addr = cpu_to_be32(dma_addr);

	/* Link software conntrack to hardware conntrack */
	this_entry->hw_entry = hw_entry;
	hw_entry->sw_entry = this_entry;

	/* add hw entry to active list and update next pointer */
	if(!dlist_empty(&hw_mc6_active_list[hash]))
	{
		/* list is not empty, and we'll be added at head, so current first will become our next pointer */
		hw_entry_first = container_of(dlist_first(&hw_mc6_active_list[hash]), typeof(struct _thw_MC6Entry), list);
		hw_entry_set_field(&hw_entry->next, hw_entry_get_field(&hw_entry_first->dma_addr));
	}
	else
	{
		/* entry is empty, so we'll be the first and only one entry */
		hw_entry_set_field(&hw_entry->next, 0);
	}

	dlist_add(&hw_mc6_active_list[hash], &hw_entry->list);

	/* check if rtp stats entry is created for this conntrack, if found link the two object and mark the conntrack 's status field for RTP stats */
	rtpqos_mc6_link_stats_entry_by_tuple(this_entry, this_entry->src_addr, this_entry->dst_addr);

	/* reflect changes to hardware entry */
	MC6_entry_update(this_entry);

	/* Update PE's internal memory socket cache tables with the HW entry's DDR address */
	MC6_entry_add_to_pe(hw_entry->dma_addr, hash);

	/* Add software entry to local hash */
	slist_add(&mc6_table_memory[hash], &this_entry->list);

	return NO_ERR;

err:
	MC6_entry_free(this_entry);

	return rc;
}

static void MC6_entry_remove(PMC6Entry this_entry, U32 hash)
{
	struct _thw_MC6Entry *hw_entry;
	struct _thw_MC6Entry *hw_entry_prev;

	/* Check if there is a hardware entry */
	if ((hw_entry = this_entry->hw_entry))
	{
		/* Detach from software socket */
		this_entry->hw_entry = NULL;

		/* if the removed entry is first in hash slot then only PE dmem hash need to be updated */
		if (&hw_entry->list == dlist_first(&hw_mc6_active_list[hash]))
		{
			MC6_entry_add_to_pe(hw_entry_get_field(&hw_entry->next), hash);
		}
		else
		{
			hw_entry_prev = container_of(hw_entry->list.prev, typeof(struct _thw_MC6Entry), list);
			hw_entry_set_field(&hw_entry_prev->next, hw_entry_get_field(&hw_entry->next));
		}
		dlist_remove(&hw_entry->list);

		hw_MC6_entry_schedule_remove(hw_entry);
	}

	slist_remove(&mc6_table_memory[hash], &this_entry->list);
	MC6_entry_free(this_entry);
}

#endif
 
int MC6_Get_Next_Hash_Entry(PMC6Command pMC6Cmd, int reset_action);
extern TIMER_ENTRY mc6_timer;

/**
 * MC6_timeout
 *
 *
 */
static void MC6_timeout(void)
{
	int i, j, k;
	PMC6Entry this_entry;
	BOOL entry_found = FALSE;
	struct slist_entry *entry;

	for(i=0;i<MC6_NUM_HASH_ENTRIES;i++)
	{
	  	slist_for_each_safe(this_entry, entry, &mc6_table_memory[i], list)
		{
			if ((this_entry->mcdest.flags & MC_ACP_LISTENER) && this_entry->mcdest.wifi_listener_timer > 0)
			{
				if ((this_entry->mcdest.wifi_listener_timer -= 10) <= 0)
					this_entry->mcdest.flags &= ~MC_ACP_LISTENER;
				else
					entry_found = TRUE;
			}
			for (j = 0; j < this_entry->mcdest.num_listeners; j++)
			{
				if (this_entry->mcdest.listeners[j].timer <= 0)
					continue;
				entry_found = TRUE;
				if ((this_entry->mcdest.listeners[j].timer -= 10) <= 0)
				{
					if(this_entry->mcdest.listeners[j].uc_bit)
						this_entry->mcdest.num_mc_uc_listeners--;
#ifdef CFG_WIFI_OFFLOAD
					else if(IS_WIFI_PORT(this_entry->mcdest.listeners[j].output_port))
						this_entry->mcdest.num_wifi_mc_listeners--;
#endif						
					/* remove listener, and move following entries up */
					for (k = j + 1; k < this_entry->mcdest.num_listeners; k++)
					{
						this_entry->mcdest.listeners[k - 1] = this_entry->mcdest.listeners[k];
					}
					this_entry->mcdest.num_listeners--;
					j -= 1; // need index to remain in place after increment
				}
			}

			if (this_entry->mcdest.num_listeners == 0 && ((this_entry->mcdest.flags & MC_ACP_LISTENER) == 0))
				MC6_entry_remove(this_entry, i);
			else
				MC6_entry_update(this_entry);
	  	}
	}
	/* turn off timer handler if no MC entries */
	if (!entry_found)
		timer_del(&mc6_timer);
}


static int MC6_is_listener(PMC6Entry this_group, POnifDesc onif_desc, U8 * UC_Mac, U16 UC_Bit)
{
	int i;
	U16 onif_index = get_onif_index(onif_desc);

	/* check all listeners */
	for(i = 0; i <this_group->mcdest.num_listeners; i++)
	{
		if (this_group->mcdest.listeners[i].output_index == onif_index)
		{
			if(UC_Bit){
	                	if(TESTEQ_MACADDR(UC_Mac, this_group->mcdest.listeners[i].dstmac))
	                        	return i;
                     	}
			else if(this_group->mcdest.listeners[i].dstmac[0] & 0x01)
	                    return i;
		}
	}
	/* unknown interface name */
	return -1;
}


/**
 * MC6_check_overlapping_pair
 * searches forwarding table for overlapping pairs based on source, destination ipv6 addresses, output interface & mask
 */
static MC6Entry *MC6_check_overlapping_pair(PMC6Command cmd)
{
	PMC6Entry this_entry;
	struct slist_entry *entry;
	U32 hash_key;
	U32 mask_len;
	POnifDesc onif_desc = NULL;
	U32 output_index;
	int i,j;
	
#define _l_src	((PACKED_U32 *) (U8*)cmd->src_addr)
#define _l_dst	((PACKED_U32 *) (U8*)cmd->dst_addr)
	hash_key = HASH_MC6(_l_dst);
	slist_for_each(this_entry, entry, &mc6_table_memory[hash_key], list)
	{
	  	if (!IPV6_CMP(_l_dst,this_entry->dst_addr))
	    	{
			for (i = 0; i < cmd->num_output; i++)
			{
				onif_desc = get_onif_by_name(cmd->output_list[i].output_device_str);
				if (onif_desc)
				{
					output_index = get_onif_index(onif_desc);
					for (j = 0; j < this_entry->mcdest.num_listeners; j++)
					{
						/* Check overlapping pairs on the output indexes that are being programmed */
						if(output_index == this_entry->mcdest.listeners[j].output_index)
						{
		    					if(cmd->src_mask_len < this_entry->mcdest.src_mask_len)
								mask_len = cmd->src_mask_len;
							else
								mask_len = this_entry->mcdest.src_mask_len;
			
	      						if (ipv6_cmp_mask((U32*) _l_src, (U32*)this_entry->src_addr, mask_len)) {
								return this_entry;
	      						}
						}
					}
				}
				else
				{
					if (this_entry->mcdest.flags & MC_ACP_LISTENER)
					{
						if(cmd->src_mask_len < this_entry->mcdest.src_mask_len)
							mask_len = cmd->src_mask_len;
						else
							mask_len = this_entry->mcdest.src_mask_len;

	      					if (ipv6_cmp_mask((U32*) _l_src, (U32*)this_entry->src_addr, mask_len)) {
							return this_entry;
	      					}
					}
				
				}
	      		}
	    	}
	}
#undef _l_src
#undef _l_dst
  	return NULL;
}



/**
 * MC6_command_add()
 *
 *
 */
static int MC6_command_add(PMC6Command cmd)
{
	PMC6Entry this_entry = NULL;
	struct slist_entry *entry;
	PMC6Entry tmpEntry;
	int i, rc = NO_ERR;
	U32 hash_key;
	POnifDesc onif_desc = NULL;
	PMC6Listener new_listener;
	U8 num_conf_mc_listeners = 0;

	/* some errors parsing on the command*/
	if(cmd->num_output > MCx_MAX_LISTENERS_IN_QUERY)
		return ERR_MC_MAX_LISTENERS;

	hash_key = HASH_MC6(cmd->dst_addr);

	/* check if group entry already exist in the table */
	slist_for_each(this_entry, entry, &mc6_table_memory[hash_key], list)
	{
		if (!IPV6_CMP(cmd->src_addr, this_entry->src_addr) && !IPV6_CMP(cmd->dst_addr, this_entry->dst_addr)) 
		{
			/* group already registered. just add listeners list the the corresponding entry */

			/* numbers of listeners specified in the API command may be larger than the max supported listeners
			a MC6 entry can handle. For now make it simple and  just nack the command if all listeners can't be added */
			if((this_entry->mcdest.num_listeners + cmd->num_output) > MC6_MAX_LISTENERS_PER_GROUP)
				return ERR_MC_MAX_LISTENERS;

			for (i = 0; i < cmd->num_output; i++) {
				if(!cmd->output_list[i].uc_bit)
					num_conf_mc_listeners++;
			}

			/* If the number of MC Listeners per group is more than MCx_MAX_MC_LISTENERS_PER_GROUP (4), throw the error */
			if(this_entry->mcdest.num_listeners - this_entry->mcdest.num_mc_uc_listeners + num_conf_mc_listeners > MCx_MAX_MC_LISTENERS_PER_GROUP)
				return ERR_MC_MAX_LISTENERS;

			break;
		}
	}

	/* If group doesn't exist, create and initialize the entry */
	if(entry == NULL)
	{
		/* IF entry not already present look for overlapping pair.
  	       Place it here so that we allow addition of Listeners to group that already exists */
       	tmpEntry = MC6_check_overlapping_pair(cmd);
		if(tmpEntry != NULL)
			return ERR_MC_ENTRY_OVERLAP;
		
		if ((this_entry = MC6_entry_alloc()) == NULL)
		  	return  ERR_NOT_ENOUGH_MEMORY;

		memset(this_entry, 0, sizeof (MC6Entry));
		SFL_memcpy(this_entry->src_addr, cmd->src_addr, IPV6_ADDRESS_LENGTH);
		this_entry->mcdest.src_mask_len = cmd->src_mask_len;
		SFL_memcpy(this_entry->dst_addr, cmd->dst_addr, IPV6_ADDRESS_LENGTH);
		this_entry->mcdest.flags = cmd->mode;
	  	this_entry->mcdest.queue_base = cmd->queue;
	}

	for (i = 0; i < cmd->num_output; i++)
	{
		/* We have the linux output interface name. Let's find corresponding output
		in the FPP world and pre-computed L2 headers that will be used within
		all the packets belonging to a same flow */
		onif_desc = get_onif_by_name(cmd->output_list[i].output_device_str);
		if (onif_desc)
		{
			if (!(onif_desc->itf->type & IF_TYPE_TUNNEL) && (MC6_is_listener(this_entry, onif_desc, cmd->output_list[i].uc_mac,cmd->output_list[i].uc_bit) < 0))  // ignore if listener already exists or is a tunnel interface
			{
				new_listener = &this_entry->mcdest.listeners[this_entry->mcdest.num_listeners++];

				new_listener->timer = cmd->output_list[i].timer;
				new_listener->shaper_mask = cmd->output_list[i].shaper_mask;
				new_listener->output_index = get_onif_index(onif_desc);
				new_listener->output_port = itf_get_phys_port(onif_desc->itf);
				
#ifdef COMCERTO_100
				U8 dstmac[ETHER_ADDR_LEN];

				/* Set MAC destination address */
				*(U16 *)dstmac = MC6_MAC_DEST_MARKER;
				*(U32 *)(dstmac + 2) = this_entry->dst_addr[IP6_LO_ADDR];
				new_listener->L2_header_size = l2_precalculate_header(onif_desc->itf, new_listener->L2_header, ETHERTYPE_IPV6_END, ETH_HDR_SIZE + VLAN_HDR_SIZE, &new_listener->output_port, dstmac);
				L1_dc_clean(new_listener->L2_header,new_listener->L2_header+sizeof(new_listener->L2_header)-1);
#else
				if(cmd->output_list[i].uc_bit) { // copy unicast Mac as destination address */
					new_listener->uc_bit = 1;
					COPY_MACADDR(new_listener->dstmac, cmd->output_list[i].uc_mac);
					this_entry->mcdest.num_mc_uc_listeners++;
				}
				else
				{
					new_listener->uc_bit = 0;
					/* Set MAC destination address */
					*(U16 *)(new_listener->dstmac) = MC6_MAC_DEST_MARKER;
					*(U32 *)(new_listener->dstmac + 2) = this_entry->dst_addr[IP6_LO_ADDR];
#ifdef CFG_WIFI_OFFLOAD
					if(IS_WIFI_PORT(new_listener->output_port))
						this_entry->mcdest.num_wifi_mc_listeners++;
#endif					
				}
				new_listener->q_bit = cmd->output_list[i].q_bit;
				new_listener->queue_base =  (cmd->output_list[i].q_bit) ? cmd->output_list[i].queue: this_entry->mcdest.queue_base;
				new_listener->itf = onif_desc->itf;
				new_listener->family = PROTO_IPV6;
#endif
#if defined(COMCERTO_2000_CONTROL)
				/* If first multicast listner output is VLAN, the following lisners also should
				 * be multicast(This is limitation in C2000). So make sure that first multicast
                                 * listners are not VLAN.
				 */
				if (!(onif_desc->itf->type & IF_TYPE_VLAN))
        			{
					int j = 0, last_idx = this_entry->mcdest.num_listeners - 1;
					MCListener tmp_listener;

					for (; j < last_idx; j++)
					{
						if (this_entry->mcdest.listeners[j].itf->type & IF_TYPE_VLAN)
							break;
					}

					/* Move non VLAN lisner to first */
					if (j < last_idx)
					{
						tmp_listener = *new_listener;
						*new_listener = this_entry->mcdest.listeners[j];
						this_entry->mcdest.listeners[j] = tmp_listener;
					}
				}
#endif
			}
		}
		else
		{
			if(cmd->output_list[i].uc_bit) // Unicasting is unsupported when Wi-Fi is not offloaded.
			{
				MC6_entry_free(this_entry);
				return ERR_MC_INTERFACE_NOT_ALLOWED;
			}
			/* default path is to ACP. Don't need to pre-compute L2 header */
			this_entry->mcdest.flags |= MC_ACP_LISTENER;
			this_entry->mcdest.wifi_listener_timer = cmd->output_list[i].timer;
		}

		if ( (cmd->output_list[i].timer != 0xFFFFFFFF) && (cmd->output_list[i].timer > 0))
			timer_add(&mc6_timer, TIMER_TICKS_PER_SEC * 10);

	}

	/* insert new entry in the multicast table */
	/* If group doesn't exist, add it to the table with the listeners list */
	if (entry == NULL)
		rc = MC6_entry_add(this_entry, hash_key);
	else
		MC6_entry_update(this_entry);

	return rc;
}


/**
 * MC6_command_remove()
 *
 *
 */
static int MC6_command_remove(PMC6Command cmd)
{
	PMC6Entry this_entry;
	struct slist_entry *entry;
	int i, j;
	U32 hash_key;
	int	listener_index;
	POnifDesc onif_desc;
	
	hash_key = HASH_MC6(cmd->dst_addr);
	slist_for_each(this_entry, entry, &mc6_table_memory[hash_key], list)
	{
		if (!IPV6_CMP(this_entry->src_addr, cmd->src_addr) && !IPV6_CMP(this_entry->dst_addr, cmd->dst_addr))
			goto found;
	}
	return ERR_MC_ENTRY_NOT_FOUND;

found:
	/* check all listeners to be removed */
	for(i = 0; i < cmd->num_output; i++)
	{
		onif_desc = get_onif_by_name(cmd->output_list[i].output_device_str);
		if(onif_desc)
		{
			if((listener_index = MC6_is_listener(this_entry, onif_desc,cmd->output_list[i].uc_mac,cmd->output_list[i].uc_bit)) >= 0)
			{
				if(this_entry->mcdest.listeners[listener_index].uc_bit)
					this_entry->mcdest.num_mc_uc_listeners--;
#ifdef CFG_WIFI_OFFLOAD
				else if(IS_WIFI_PORT(this_entry->mcdest.listeners[listener_index].output_port))
					this_entry->mcdest.num_wifi_mc_listeners--;
#endif					
				/* remove listener, and move following entries up */
				for (j = listener_index + 1; j < this_entry->mcdest.num_listeners; j++)
				{
					this_entry->mcdest.listeners[j - 1] = this_entry->mcdest.listeners[j];
#if !defined(COMCERTO_2000_CONTROL)
					L1_dc_clean(&this_entry->mcdest.listeners[j - 1], ((U8*) &this_entry->mcdest.listeners[j])-1);
#endif
				}
				this_entry->mcdest.num_listeners--;
			}
		/* if listener does not belong to the group, just silently continue processing */
		}
		else
		{
			this_entry->mcdest.flags &= ~MC_ACP_LISTENER;
		}
	}
		
	/* no more listerners attached to the group - remove entry */
	if(this_entry->mcdest.num_listeners == 0 && ((this_entry->mcdest.flags & MC_ACP_LISTENER) == 0))
		MC6_entry_remove(this_entry, hash_key);
	else
		MC6_entry_update(this_entry);

	return NO_ERR;
}


// MC6_interface_purge -- remove any listeners referencing an output interface
void MC6_interface_purge(U32 if_index)
{
	int i, j, k;
	PMC6Entry this_entry;
	struct slist_entry *entry;

	for (i = 0; i < MC6_NUM_HASH_ENTRIES; i++)
	{
	  	slist_for_each_safe(this_entry, entry, &mc6_table_memory[i], list)
		{
			for (j = 0; j < this_entry->mcdest.num_listeners; j++)
			{
				if (if_index == this_entry->mcdest.listeners[j].output_index)
				{
					if(this_entry->mcdest.listeners[j].uc_bit)
						this_entry->mcdest.num_mc_uc_listeners--;
#ifdef CFG_WIFI_OFFLOAD
					else if(IS_WIFI_PORT(this_entry->mcdest.listeners[j].output_port))
						this_entry->mcdest.num_wifi_mc_listeners--;
#endif
					/* remove listener, and move following entries up */
					for (k = j + 1; k < this_entry->mcdest.num_listeners; k++)
					{
						this_entry->mcdest.listeners[k - 1] = this_entry->mcdest.listeners[k];
					}
					this_entry->mcdest.num_listeners--;
					j -= 1; // need index to remain in place after increment
				}
			}

			if (this_entry->mcdest.num_listeners == 0 && ((this_entry->mcdest.flags & MC_ACP_LISTENER) == 0))
				MC6_entry_remove(this_entry, i);
			else
				MC6_entry_update(this_entry);
	  	}
	}
}



/**
 * MC6_command_refresh()
 *
 *
 */
static int MC6_command_refresh(PMC6Command cmd)
{
	PMC6Entry this_entry;
	POnifDesc onif_desc, new_onif_desc;
	int listener_refreshed, listener_index;
    int group_qb_refreshed = 0;
	PMC6Listener update_listener;

	this_entry = MC6_rule_search(cmd->src_addr, cmd->dst_addr);
	if(this_entry != NULL)
	{

               this_entry->mcdest.queue_base = cmd->queue;
               for(group_qb_refreshed = 0; group_qb_refreshed < this_entry->mcdest.num_listeners; group_qb_refreshed++)
                       if(!this_entry->mcdest.listeners[group_qb_refreshed].q_bit)
                               this_entry->mcdest.listeners[group_qb_refreshed].queue_base = cmd->queue;
		/* now find listeners to refresh */
		for(listener_refreshed = 0; listener_refreshed < cmd->num_output; listener_refreshed++)
		{
			onif_desc = get_onif_by_name(cmd->output_list[listener_refreshed].output_device_str);
			if (onif_desc)
			{
				if ((listener_index = MC6_is_listener(this_entry, onif_desc, cmd->output_list[listener_refreshed].uc_mac, cmd->output_list[listener_refreshed].uc_bit)) >=0)
				{
					update_listener = &this_entry->mcdest.listeners[listener_index];
					if(cmd->output_list[listener_refreshed].if_bit)
					{
						if( (new_onif_desc = (get_onif_by_name(cmd->output_list[listener_refreshed].new_output_device_str))) != NULL) 
						{
							if(MC6_is_listener(this_entry, new_onif_desc, cmd->output_list[listener_refreshed].uc_mac, cmd->output_list[listener_refreshed].uc_bit) < 0)
							{
								update_listener->output_index = get_onif_index(new_onif_desc);
								update_listener->output_port = itf_get_phys_port(new_onif_desc->itf);
#ifdef COMCERTO_100
								{
									U8 dstmac[ETHER_ADDR_LEN];

									/* Set MAC destination address */
									*(U16 *)dstmac = MC6_MAC_DEST_MARKER;
									*(U32 *)(dstmac + 2) = this_entry->dst_addr[IP6_LO_ADDR];

									update_listener->L2_header_size = l2_precalculate_header(new_onif_desc->itf, update_listener->L2_header, ETHERTYPE_IPV6_END, ETH_HDR_SIZE + VLAN_HDR_SIZE, &update_listener->output_port, dstmac);
									L1_dc_clean(update_listener->L2_header, update_listener->L2_header+sizeof(update_listener->L2_header)-1);
								}
#else
                						if(cmd->output_list[listener_refreshed].uc_bit) { // copy unicast Mac as destination address
									update_listener->uc_bit = 1;
									COPY_MACADDR(update_listener->dstmac, cmd->output_list[listener_refreshed].uc_mac);
                						}
								else
								{
									update_listener->uc_bit = 0;
									*(U16 *)(update_listener->dstmac) = MC6_MAC_DEST_MARKER;
									*(U32 *)(update_listener->dstmac + 2) = this_entry->dst_addr[IP6_LO_ADDR];
								}
								update_listener->itf = new_onif_desc->itf;
#endif
							}
							else
								return ERR_MC_DUP_LISTENER;
						}
						else
							return ERR_UNKNOWN_INTERFACE;
					}

					update_listener->timer = cmd->output_list[listener_refreshed].timer;
					update_listener->shaper_mask = cmd->output_list[listener_refreshed].shaper_mask;
					if(cmd->output_list[listener_refreshed].q_bit)
                                               update_listener->queue_base = cmd->output_list[listener_refreshed].queue;
				}	
			}
			else
			{
				this_entry->mcdest.wifi_listener_timer = cmd->output_list[listener_refreshed].timer;
			}
			if ((cmd->output_list[listener_refreshed].timer != 0xFFFFFFFF) && (cmd->output_list[listener_refreshed].timer > 0))
				timer_add(&mc6_timer, TIMER_TICKS_PER_SEC * 10);
		}		

		MC6_entry_update(this_entry);
	}
	else
		return ERR_MC_ENTRY_NOT_FOUND;

	return NO_ERR;
}


/**
 * MC6_handle_MULTICAST
 *
 *
 */
static int MC6_handle_MULTICAST(U16 *p, U16 Length)
{
	MC6Command 	cmd;
	U16 rc = NO_ERR;
	int reset_action = 0;
	
	/* Check length */
	if ((Length > sizeof(MC6Command)) || (Length < MC6_MIN_COMMAND_SIZE))
		return ERR_WRONG_COMMAND_SIZE;

	memset(&cmd, 0, sizeof(MC6Command));
	SFL_memcpy((U8*)&cmd, (U8*)p,  Length);
		
	switch(cmd.action)
	{
		case MC_ACTION_REMOVE: 
			rc =  MC6_command_remove(&cmd);
			break;
	
		case MC_ACTION_ADD:
			rc =  MC6_command_add(&cmd);
			break;

		case MC_ACTION_REFRESH:
			rc =  MC6_command_refresh(&cmd);
			break;
		case ACTION_QUERY:
			reset_action = 1;
		case ACTION_QUERY_CONT:
			rc = MC6_Get_Next_Hash_Entry((MC6Command*)p, reset_action);
			break;
		default :
			rc =  ERR_UNKNOWN_ACTION;
			break;
	} 

	return rc;

}


static void MC6_Flush_Entries(void)
{
	PMC6Entry this_entry;
	struct slist_entry *entry;
	int i;

	for (i = 0; i < MC6_NUM_HASH_ENTRIES; i++)
	{
		slist_for_each_safe(this_entry, entry, &mc6_table_memory[i], list)
		{
			MC6_entry_remove(this_entry, i);
		}
	}
}

static int MC6_handle_MODE(U16 *p , U16 Length)
{
	U16 mode = *p;
	int rc = NO_ERR;
	
	if (Length != 2)
		return ERR_WRONG_COMMAND_SIZE;
	
	switch (mode)
	{
		case MC_MODE_BRIDGED:
		case MC_MODE_ROUTED:
			g_mc6_mode = mode;
			MC6_mode_update();
		break;
		default:
			rc = ERR_UNKNOWN_ACTION;
		break;
			
	}
	
	return rc;
}



   
/**
 * MC6_handle_RESET
 *
 *
 */
static int MC6_handle_RESET(void)
{
	MC6_Flush_Entries();

	return NO_ERR;
}

/**
 * M_mc6_cmdproc
 *
 *
 *
 */
static U16 M_mc6_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 action;
	U16 querySize = 0;
		
	switch (cmd_code)
	{
		case CMD_MC6_MULTICAST:			
			action = *pcmd;
			rc = MC6_handle_MULTICAST(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				querySize = sizeof(MC6Command);
			break;

		case CMD_MC6_RESET:			
			rc = MC6_handle_RESET();
			break;

		case CMD_MC6_MODE:
			rc = MC6_handle_MODE(pcmd, cmd_len);
			break;

		case CMD_MC4_MULTICAST:
			action = *pcmd;
			rc = MC4_handle_MULTICAST(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				querySize = sizeof(MC4Command);
			break;

		case CMD_MC4_RESET:			
			rc = MC4_handle_RESET();
			break;
		
		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;
	return 2 + querySize;
}

  
/**
 * M_mc6_init
 *
 *
 *
 */
BOOL mc6_init(void)
{
	int i;
#if defined(COMCERTO_2000)	
	struct pfe_ctrl *ctrl = &pfe->ctrl;
#endif
#if defined(COMCERTO_1000)
	if (multicast_init())
		return 1;
#endif

	/* module entry point and channels registration */
#if !defined(COMCERTO_2000)
	set_event_handler(EVENT_MC6, M_mc6_entry);
#endif
	set_cmd_handler(EVENT_MC6, M_mc6_cmdproc);

	for (i = 0; i < MC6_NUM_HASH_ENTRIES; i++)
	{
		slist_head_init(&mc6_table_memory[i]);
	}

#if defined(COMCERTO_2000)
	mc6_dma_pool = ctrl->dma_pool_512;

	for (i = 0; i < MC6_NUM_HASH_ENTRIES; i++)
		dlist_head_init(&hw_mc6_active_list[i]);

	dlist_head_init(&hw_mc6_removal_list);

	timer_init(&mc6_delayed_remove_timer, hw_MC6_entry_delayed_remove);
	timer_add(&mc6_delayed_remove_timer, CT_TIMER_INTERVAL);
#endif

	timer_init(&mc6_timer, MC6_timeout);

	g_mc6_mode = MC_MODE_BRIDGED;
	MC6_mode_update();
	return 0;
}

void mc6_exit(void)
{
#if defined(COMCERTO_2000)
	struct dlist_head *entry;
	struct _thw_MC6Entry *hw_entry;

	timer_del(&mc6_timer);
	timer_del(&mc6_delayed_remove_timer);

	MC6_handle_RESET();

	dlist_for_each_safe(hw_entry, entry, &hw_mc6_removal_list, list)
	{
		dlist_remove(&hw_entry->list);
		dma_pool_free(mc6_dma_pool, hw_entry, be32_to_cpu(hw_entry->dma_addr));
	}
#endif
}
