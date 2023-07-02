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

#include "module_Rx.h"
#include "module_hidrv.h"

#ifdef CFG_LRO

#include "module_lro.h"

static U8 CLASS_DMEM_SH(lro_enable);
#endif

static U16 M_rx_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U32  portid;
	U16 acklen;
	U16 ackstatus;
	U8 enable;
#ifdef CFG_LRO
	int id;
	struct pfe_ctrl *ctrl = &pfe->ctrl;
#endif

	acklen = 2;
	ackstatus = CMD_OK;

	switch (cmd_code)
	{
	case CMD_RX_ENABLE:
		portid = (U8)*pcmd;
		if (portid >= GEM_PORTS) {
			ackstatus = CMD_ERR;
			break;
		}

//		M_expt_rx_enable(portid);
//		M_rx_enable(portid);
		break;

	case CMD_RX_DISABLE:
		portid = (U8)*pcmd;
		if (portid >= GEM_PORTS) {
			ackstatus = CMD_ERR;
			break;
		}

//		M_rx_disable(portid);
//		M_expt_rx_disable(portid);
		break;

	case CMD_RX_LRO:
		enable = (U8)*pcmd;

#ifdef CFG_LRO
		if (enable > LRO_ENABLE_SELECTED) {
			ackstatus = CMD_ERR;
			break;
		}

		if (pe_sync_stop(ctrl, CLASS_MASK) < 0) {
			ackstatus = CMD_ERR;
			break;
		}

		/* update the DMEM in class-pe */
	        for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++){
			pe_dmem_writeb(id, enable, virt_to_class_dmem(&lro_enable));
		}

		pe_start(ctrl, CLASS_MASK);
#else
		if (enable > 0)
			ackstatus = CMD_ERR;
#endif

		break;

	default:
		ackstatus = CMD_ERR;
		break;
	}

	*pcmd = ackstatus;
	return acklen;
}


int rx_init(void)
{
	set_cmd_handler(EVENT_PKT_RX, M_rx_cmdproc);

	ff_enable = 1;

	return 0;
}

void rx_exit(void)
{

}
