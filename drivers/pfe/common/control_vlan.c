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

#include "modules.h"
#include "module_hidrv.h"
#include "system.h"
#include "gemac.h"
#include "fpp.h"
#include "module_vlan.h"
#include "layer2.h"


int Vlan_Get_Next_Hash_Entry(PVlanCommand pVlanCmd, int reset_action);


#if defined(COMCERTO_2000)

static PVlanEntry vlan_alloc(void)
{
	int i;

	for (i = 0; i < VLAN_MAX_ITF; i++)
		if (!vlan_itf[i].itf.type)
			return &vlan_itf[i];

	return NULL;
}

static void vlan_free(PVlanEntry pEntry)
{
	pEntry->itf.type = 0;
}

static void vlan_add(PVlanEntry pEntry, U16 hash_key)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int id;
	VlanEntry vlan;

	/* Add to our local hash */
	slist_add(&vlan_cache[hash_key], &pEntry->list);

	/* Construct the hardware entry, converting virtual addresses and endianess where needed */
	memcpy(&vlan, pEntry, sizeof(VlanEntry));
	vlan.itf.phys = (void *)cpu_to_be32(virt_to_class(pEntry->itf.phys));
	slist_set_next(&vlan.list, (void *)cpu_to_be32(virt_to_class_dmem(slist_next(&pEntry->list))));

	/* Update the PE vlan interface in DMEM, for each PE */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_memcpy_to32(id, virt_to_class_dmem(pEntry), &vlan, sizeof(VlanEntry));

	/* Now add the interface to the PE vlan hash in DMEM, for each PE and atomically */

	pe_sync_stop(ctrl, CLASS_MASK);

	/* We use the fact that slist_add() always adds to the head of the list */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_write(id, cpu_to_be32(virt_to_class_dmem(&pEntry->list)), virt_to_class_dmem(&vlan_cache[hash_key]), sizeof(struct slist_entry *));

	pe_start(ctrl, CLASS_MASK);
}

static void vlan_remove(PVlanEntry pEntry, U16 hash_key)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct slist_entry *prev;
	int id;

	/* Now remove the interface from the PE vlan hash in DMEM, for each PE and atomically */

	prev = slist_prev(&vlan_cache[hash_key], &pEntry->list);

	pe_sync_stop(ctrl, CLASS_MASK);

	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_write(id, cpu_to_be32(virt_to_class_dmem(slist_next(&pEntry->list))), virt_to_class_dmem(prev), sizeof(struct slist_entry *));

	pe_start(ctrl, CLASS_MASK);

	/* Remove from our local hash */
	slist_remove_after(prev);
}

void M_vlan_encapsulate(PMetadata mtd, PVlanEntry pEntry, U16 ethertype, U8 update)
{

}
#else

static PVlanEntry vlan_alloc(void)
{
	return Heap_Alloc_ARAM(sizeof (VlanEntry));
}

static void vlan_free(PVlanEntry pEntry)
{
	Heap_Free(pEntry);
}

static void vlan_add(PVlanEntry pEntry, U16 hash_key)
{
	slist_add(&vlan_cache[hash_key], &pEntry->list);
}

static void vlan_remove(PVlanEntry pEntry, U16 hash_key)
{
	slist_remove(&vlan_cache[hash_key], &pEntry->list);
}

#endif

static U16 Vlan_handle_reset(void)
{
	PVlanEntry pEntry;
	struct slist_entry *entry;
	int i;

	/* free VLAN entries */
	for(i = 0; i < NUM_VLAN_ENTRIES; i++)
	{
		slist_for_each_safe(pEntry, entry, &vlan_cache[i], list)
		{
			remove_onif_by_index(pEntry->itf.index);
			vlan_remove(pEntry, i);
			vlan_free(pEntry);
		}
	}

	return NO_ERR;
}


static U16 Vlan_handle_entry(U16 * p,U16 Length)
{
	PVlanEntry pEntry;
	struct slist_entry *entry;
	VlanCommand vlancmd;
	POnifDesc phys_onif;
	U16	hash_key;
	int reset_action = 0;

	// Check length
	if (Length != sizeof(VlanCommand))
		return ERR_WRONG_COMMAND_SIZE;
	
	SFL_memcpy((U8*)&vlancmd, (U8*)p,  sizeof(VlanCommand));

	hash_key = HASH_VLAN(htons(vlancmd.vlanID));

	switch(vlancmd.action)
	{
		case ACTION_DEREGISTER: 

			slist_for_each(pEntry, entry, &vlan_cache[hash_key], list)
			{
				if ((pEntry->VlanId == htons(vlancmd.vlanID & 0xfff)) && (strcmp(get_onif_name(pEntry->itf.index), (char *)vlancmd.vlanifname) == 0))
					goto found;
			}

			return ERR_VLAN_ENTRY_NOT_FOUND;

		found:
			vlan_remove(pEntry, hash_key);

			/*Tell the Interface Manager to remove the Vlan IF*/
			remove_onif_by_index(pEntry->itf.index);
			
			vlan_free(pEntry);

			break;

		case ACTION_REGISTER: 

			if (get_onif_by_name(vlancmd.vlanifname))
				return ERR_VLAN_ENTRY_ALREADY_REGISTERED;

			slist_for_each(pEntry, entry, &vlan_cache[hash_key], list)
			{
				if ((pEntry->VlanId == htons(vlancmd.vlanID & 0xfff)) && (strcmp(get_onif_name(pEntry->itf.index), (char *)vlancmd.vlanifname) == 0) )
					return ERR_VLAN_ENTRY_ALREADY_REGISTERED; //trying to add exactly the same vlan entry
			}

			if ((pEntry = vlan_alloc()) == NULL)
			{
			  	return ERR_NOT_ENOUGH_MEMORY;
			}
			
	  		memset(pEntry, 0, sizeof (VlanEntry));
			pEntry->VlanId = htons(vlancmd.vlanID & 0xfff);

			/*Check if the Physical interface is known by the Interface manager*/
			phys_onif = get_onif_by_name(vlancmd.phyifname);
			if (!phys_onif)
			{
				vlan_free(pEntry);
				return ERR_UNKNOWN_INTERFACE;
			}

			/*Now create a new interface in the Interface Manager and remember the index*/
			if (!add_onif(vlancmd.vlanifname, &pEntry->itf, phys_onif->itf, IF_TYPE_VLAN))
			{
				vlan_free(pEntry);
				return ERR_CREATION_FAILED;
			}

			vlan_add(pEntry, hash_key);

			break;
			
		case ACTION_QUERY:
			reset_action = 1;
		case ACTION_QUERY_CONT:
		{
			PVlanCommand pVlan = (VlanCommand*)p;
			int rc;
			
			rc = Vlan_Get_Next_Hash_Entry(pVlan, reset_action);
			return rc;
		}
		
			
		default:
			return ERR_UNKNOWN_ACTION;
	}

	return NO_ERR;
}


static U16 M_vlan_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 retlen = 2;
	U16 action;

	switch (cmd_code)
	{
		case CMD_VLAN_ENTRY:
			action = *pcmd;
			rc = Vlan_handle_entry(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				retlen += sizeof (VlanCommand);
			break;

		case CMD_VLAN_ENTRY_RESET:
			rc = Vlan_handle_reset();
			break;

		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;
	return retlen;
}


int vlan_init(void)
{
	int i;

#if !defined(COMCERTO_2000)
	set_event_handler(EVENT_VLAN, M_vlan_entry);
#endif

	set_cmd_handler(EVENT_VLAN, M_vlan_cmdproc);

	for(i = 0; i < NUM_VLAN_ENTRIES; i++)
	{
		slist_head_init(&vlan_cache[i]);
	}

	return 0;
}


void vlan_exit(void)
{
	/* FIXME just call vlan reset ? */
}
