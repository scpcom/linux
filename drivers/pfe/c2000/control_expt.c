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

#include "module_expt.h"
#include "events.h"
#include "module_Rx.h"
#include "fpp.h"
#include "module_ethernet.h"
#include "module_vlan.h"
#include "layer2.h"
#include "module_pppoe.h"
#include "module_hidrv.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_ipsec.h"
#include "module_qm.h"
#include "module_wifi.h"

static int M_expt_update_classpe(u8 ctl_flag)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int id;

	if (pe_sync_stop(ctrl, CLASS_MASK) < 0)
		return CMD_ERR;
	/* update the DMEM in class-pe */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
	{
		if (ctl_flag & EXPT_CTRLQ_CONFIG)
			pe_dmem_writeb(id,  CTRL_to_Q, virt_to_class_dmem(&CTRL_to_Q));
		if (ctl_flag & EXPT_DSCP_CONFIG)
			pe_dmem_memcpy_to32(id, virt_to_class_dmem(DSCP_to_Q), DSCP_to_Q, sizeof (DSCP_to_Q));
	}

	pe_start(ctrl, CLASS_MASK);

	return NO_ERR;
}


int M_expt_queue_reset(void)
{
	int i;

	CTRL_to_Q = EXPT_MAX_QUEUE;  /* control protocol packets assigned to highest priority queue */
	DSCP_to_Q[0] = EXPT_Q0; /* dscp == 0 assigned to lowest priority queue */
	for(i = 1; i <= DSCP_MAX_VALUE; i++)
		DSCP_to_Q[i] = EXPT_MAX_QUEUE; /* dscp > 0 assigned to highest priority queue */
	return M_expt_update_classpe(EXPT_CTRLQ_CONFIG | EXPT_DSCP_CONFIG);
}


static int EXPT_Handle_ExptQueue_DSCP(U16 *p, U16 Length)
{
	ExptQueueDSCPCommand 	cmd;
	int i;

	if (Length > sizeof(ExptQueueDSCPCommand))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(ExptQueueDSCPCommand));

	if(cmd.queue > EXPT_MAX_QUEUE)
		return ERR_EXPT_QUEUE_OUT_OF_RANGE;

	if(cmd.num_dscp > (DSCP_MAX_VALUE + 1))
		return ERR_EXPT_NUM_DSCP_OUT_OF_RANGE;

	for(i = 0; i < cmd.num_dscp; i++)
		if(cmd.dscp[i] > DSCP_MAX_VALUE) 
			return ERR_EXPT_DSCP_OUT_OF_RANGE;

	//the whole command is correct, we can assign dspc to queues
	for(i = 0; i < cmd.num_dscp; i++)
		DSCP_to_Q[cmd.dscp[i]] = cmd.queue;

	return M_expt_update_classpe(EXPT_DSCP_CONFIG);
}

static int EXPT_Handle_ExptQueue_CTRL(U16 *p, U16 Length)
{
	ExptQueueCTRLCommand 	cmd;

	if (Length > sizeof(ExptQueueCTRLCommand))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy((U8*)&cmd, (U8*)p,  sizeof(ExptQueueCTRLCommand));

	//the whole command is correct, we can assign control protocol packets  to the specified queue
	CTRL_to_Q = cmd.queue;
	return M_expt_update_classpe(EXPT_CTRLQ_CONFIG);

}


static int EXPT_Handle_ExptQueue_RESET(U16 *p, U16 Length)
{
	return M_expt_queue_reset();

}

U16 M_expt_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rtncode;

	switch (cmd_code)
	{
		case CMD_EXPT_QUEUE_DSCP:
			{
				rtncode = EXPT_Handle_ExptQueue_DSCP(pcmd, cmd_len);
				break;
			}

		case CMD_EXPT_QUEUE_CTRL:	
			{
				rtncode = EXPT_Handle_ExptQueue_CTRL(pcmd, cmd_len);
				break;
			}

		case CMD_EXPT_QUEUE_RESET:
			{
				rtncode = EXPT_Handle_ExptQueue_RESET(pcmd, cmd_len);
				break;
			}

			// unknown command code
		default: {
				rtncode = CMD_ERR;
				break;
			 }
	}

	*pcmd = rtncode;
	return 2;
}

