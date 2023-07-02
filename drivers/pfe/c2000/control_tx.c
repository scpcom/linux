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

#include "module_tx.h"
#include "module_hidrv.h"
#include "control_bridge.h"


char IF0_NAME[10] = TOSTR(DEFAULT_NAME_0);
char IF1_NAME[10] = TOSTR(DEFAULT_NAME_1);
char IF2_NAME[10] = TOSTR(DEFAULT_NAME_2);


static void M_tx_port_update(PPortUpdateCommand cmd)
{
	char *if_name = get_onif_name(phy_port[cmd->portid].itf.index);

	strncpy(if_name, cmd->ifname, INTERFACE_NAME_LENGTH);
	if_name[INTERFACE_NAME_LENGTH - 1] = '\0';
}

static U16 M_tx_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U32 portid;
	U16 rc;
	U16 retlen = 2;
	int id;

	portid = *pcmd;

	if (portid >= GEM_PORTS) {
		rc = CMD_ERR;
		goto out;
	}

	switch (cmd_code)
	{
	case CMD_TX_ENABLE:

//		M_tx_enable(portid);

		if (cmd_len > 2) {
			if (cmd_len > 14) {
				memcpy(phy_port[portid].mac_addr, &(((U8*)pcmd)[14]), 6);
				/* Set tx enable flag in Class */
				phy_port[portid].flags |= TX_ENABLED;

				for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++) {
					pe_dmem_memcpy_to32(id, virt_to_class_dmem(&phy_port[portid].mac_addr), phy_port[portid].mac_addr, 6);
					/*Set tx enable flag in class and Util for this physical port*/
					pe_dmem_writeb(id, phy_port[portid].flags,
							virt_to_class_dmem(&phy_port[portid].flags));
				}
				/*Set tx enable flag in Util for this physical port*/
				pe_dmem_writeb(UTIL_ID, phy_port[portid].flags, virt_to_util_dmem(&util_phy_port[portid].flags));
			}
		}

//		M_expt_tx_enable(portid);

		rc = CMD_OK;
		break;

	case CMD_TX_DISABLE:

//		M_expt_tx_disable(portid);
//		M_tx_disable(portid);

		/*Reset tx enable flag in class and Util for this physical port*/
		phy_port[portid].flags &= ~TX_ENABLED;
		for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
			pe_dmem_writeb(id, phy_port[portid].flags, virt_to_class_dmem(&phy_port[portid].flags));

		pe_dmem_writeb(UTIL_ID, phy_port[portid].flags, virt_to_util_dmem(&util_phy_port[portid].flags));

		rc = CMD_OK;
		break;

	case CMD_PORT_UPDATE:

		/* Update the port info in the onif */
		M_tx_port_update((PPortUpdateCommand)pcmd);
		rc = CMD_OK;
		break;

	default:
		rc = CMD_ERR;
		break;
	}

out:
	*pcmd = rc;
	return retlen;
}


int tx_init(void)
{
	int i, id;

	set_cmd_handler(EVENT_PKT_TX, M_tx_cmdproc);

	add_onif((U8 *)IF0_NAME, &phy_port[0].itf, NULL, IF_TYPE_ETHERNET | IF_TYPE_PHYSICAL); /* FIXME check result */
	add_onif((U8 *)IF1_NAME, &phy_port[1].itf, NULL, IF_TYPE_ETHERNET | IF_TYPE_PHYSICAL); /* FIXME check result */
	add_onif((U8 *)IF2_NAME, &phy_port[2].itf, NULL, IF_TYPE_ETHERNET | IF_TYPE_PHYSICAL); /* FIXME check result */

	for (i = 0; i < GEM_PORTS; i++) {
		phy_port[i].id = i;

		for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
			pe_dmem_writeb(id, phy_port[i].itf.index, virt_to_class_dmem(&phy_port[i].itf.index));
	}

	/* Register interfaces with bridge */
	bridge_interface_register((U8 *) IF0_NAME, 0);
	bridge_interface_register((U8 *) IF1_NAME, 1);
	bridge_interface_register((U8 *) IF2_NAME, 2);

	return 0;
}

void tx_exit(void)
{
	int i;

	for (i = 0; i < GEM_PORTS; i++)
		remove_onif_by_index(phy_port[i].itf.index);
}
