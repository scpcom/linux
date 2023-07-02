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
#include "fpp_globals.h"
#include "modules.h"
#include "events.h"
#include "module_Rx.h"
#include "gemac.h"
#include "fpool.h"
#include "fpp.h"
#include "layer2.h"
#include "module_hidrv.h"
#include "module_timer.h"
#include "module_expt.h"
#include "module_ethernet.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_stat.h"
#include "system.h"
#include "fe.h"
#include "channels.h"
#include "module_l2tp.h"
#include "control_common.h"


struct slist_head CLASS_PE_LMEM_SH(l2tp_cache)[NUM_L2TP_ENTRIES];
l2tp_entry CLASS_PE_LMEM_SH(l2tp_table)[MAX_L2TP_ITF];


static pl2tp_entry l2tp_get_entry(void)
{
	int i = 0;
	pl2tp_entry pEntry;

	for (i = 0; i < MAX_L2TP_ITF; i++)
	{
		pEntry = &l2tp_table[i];
		/* Tunnel ID cannot be NULL, we use it as a free/busy flag */
		if (pEntry->local_tun_id == L2TP_IF_FREE)
			return pEntry;
	}
	return NULL;
}

static int l2tp_add(pl2tp_entry sw_entry, U32 hash)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	l2tp_entry hw_entry;

	/* Add software entry to local hash */
	slist_add(&l2tp_cache[hash], &sw_entry->list);
	
	/* Setup HW entry */

	memcpy(&hw_entry, sw_entry, sizeof(l2tp_entry));

	//sw_entry->hash = hash;
	hw_entry.sock_id = cpu_to_be16(sw_entry->sock_id);
	hw_entry.options = cpu_to_be16(sw_entry->options);
	
	hw_entry.itf.phys = (void *)cpu_to_be32(virt_to_class_pe_lmem(sw_entry->itf.phys));
	slist_set_next(&hw_entry.list, (void *)cpu_to_be32(virt_to_class_pe_lmem(slist_next(&sw_entry->list))));


	/* Update interface in PE cluster mem */
	class_pe_lmem_memcpy_to32(virt_to_class_pe_lmem(sw_entry), &hw_entry, sizeof(l2tp_entry));

	pe_sync_stop(ctrl, CLASS_MASK);

	/* Update PE cluster mem hash table  */
	class_bus_write(cpu_to_be32(virt_to_class_pe_lmem(&sw_entry->list)), virt_to_class_pe_lmem(&l2tp_cache[hash]), sizeof(struct slist_entry *));


	pe_start(ctrl, CLASS_MASK);


	return NO_ERR;
}

static void l2tp_remove(pl2tp_entry sw_entry, U32 hash)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct slist_entry *prev;

	/* Now remove the interface PE cluster mem */
	prev = slist_prev(&l2tp_cache[hash], &sw_entry->list);
	
	pe_sync_stop(ctrl, CLASS_MASK);

	class_bus_write(cpu_to_be32(virt_to_class_pe_lmem(slist_next(&sw_entry->list))),virt_to_class_pe_lmem(prev), sizeof(struct slist_entry *));

	pe_start(ctrl, CLASS_MASK);

	/* Remove from our local hash */
	slist_remove_after(prev);

	/* Flag entry as free */
	sw_entry->local_tun_id = L2TP_IF_FREE;

}

static pl2tp_entry l2tp_find_entry(U32 hash, U16 local_tun_id, U16 peer_tun_id, U16 local_ses_id, U16 peer_ses_id)
{
	pl2tp_entry pEntry;
	struct slist_entry *entry;

	slist_for_each(pEntry, entry, &l2tp_cache[hash], list) {
		if ((pEntry->local_tun_id == local_tun_id) && (pEntry->peer_tun_id == peer_tun_id)
		&& (pEntry->local_ses_id == local_ses_id) && (pEntry->peer_ses_id == peer_ses_id))
			return pEntry;
	}
	return NULL;
}

U16 M_l2tp_cmdproc(U16 cmd_code, U16 cmd_len, U16 *p)
{
	U16 acklen;
	U16 ackstatus;
	pl2tp_entry l2tp_entry;

	acklen = 2;
	ackstatus = CMD_OK;

	switch (cmd_code)
	{
	case CMD_L2TP_ITF_ADD: {
		U32 hash;
		pl2tp_itf_add_cmd pcmd = (pl2tp_itf_add_cmd)p;

		if(get_onif_by_name(pcmd->ifname)) {
			ackstatus = ERR_UNKNOWN_INTERFACE;	
			break;
		}
		
		hash = HASH_L2TP(pcmd->local_tun_id, pcmd->local_ses_id);
		if (l2tp_find_entry(hash, pcmd->local_tun_id, pcmd->peer_tun_id, pcmd->local_ses_id, pcmd->peer_ses_id)) {
			ackstatus = ERR_CREATION_FAILED;
			break;
		}

		l2tp_entry = l2tp_get_entry();
		if(!l2tp_entry)
		{
			ackstatus = ERR_CREATION_FAILED;
			break;
		}

		l2tp_entry->sock_id = pcmd->sock_id;
		l2tp_entry->local_tun_id = pcmd->local_tun_id;
		l2tp_entry->peer_tun_id = pcmd->peer_tun_id;
		l2tp_entry->local_ses_id = pcmd->local_ses_id;
		l2tp_entry->peer_ses_id = pcmd->peer_ses_id;
		l2tp_entry->options = pcmd->options;

#if 0 //DEBUG
		printk(KERN_INFO "sock_id(%d)\n", l2tp_entry->sock_id);
		printk(KERN_INFO "local_tun_id(%d)\n", l2tp_entry->local_tun_id);
		printk(KERN_INFO "peer_tun_id(%d)\n", l2tp_entry->peer_tun_id);
		printk(KERN_INFO "local_ses_id(%d)\n", l2tp_entry->local_ses_id);
		printk(KERN_INFO "peer_ses_id(%d)\n", l2tp_entry->peer_ses_id);
		printk(KERN_INFO "hash(%d)\n", hash);
#endif


		/* Now create a new interface in the Interface Manager */
		if (!add_onif(pcmd->ifname, &l2tp_entry->itf, NULL, IF_TYPE_L2TP)) {
			ackstatus = ERR_CREATION_FAILED;
			l2tp_entry->local_tun_id = L2TP_IF_FREE;
			break;
		}
		if((ackstatus = l2tp_add(l2tp_entry, hash)) != NO_ERR)
		{
			/* Remove onif already created*/
			remove_onif_by_index(l2tp_entry->itf.index);

			/* Free up the l2tp_entry for later use*/
			l2tp_entry->local_tun_id = L2TP_IF_FREE;
			break;
		}

		break;
	}
	case CMD_L2TP_ITF_DEL: {
		pl2tp_itf_del_cmd pcmd = (pl2tp_itf_del_cmd)p;
		POnifDesc pOnif;
		pl2tp_entry pEntry;
		U32 hash;

		if((pOnif = get_onif_by_name(pcmd->ifname)) == NULL) {
			ackstatus = ERR_UNKNOWN_INTERFACE;	
			break;
		}
		pEntry = container_of(pOnif->itf, typeof(struct _tl2tp_entry), itf);
		remove_onif_by_index(pEntry->itf.index);

		hash = HASH_L2TP(pEntry->local_tun_id,pEntry->local_ses_id);

		l2tp_remove(pEntry, hash);
		break;
	}
	default:
		ackstatus = ERR_UNKNOWN_COMMAND;
		break;
	}
	*p = ackstatus;
	return acklen;
}

int l2tp_init(void)
{
	int i;

	set_cmd_handler(EVENT_L2TP, M_l2tp_cmdproc);

	for (i = 0; i < NUM_L2TP_ENTRIES; i++)
		slist_head_init(&l2tp_cache[i]);

	return 0;
}

void l2tp_exit(void)
{
	return;
}

