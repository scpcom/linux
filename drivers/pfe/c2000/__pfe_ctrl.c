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

#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_pppoe.h"
#include "module_vlan.h"
#include "module_hidrv.h"
#include "module_tx.h"
#include "module_Rx.h"
#include "module_stat.h"
#include "module_rtp_relay.h"
#include "module_socket.h"
#include "module_natpt.h"
#include "module_qm.h"
#include "module_mc4.h"
#include "module_mc6.h"
#include "icc.h"
#include "module_ipsec.h"
#include "module_bridge.h"
#include "module_tunnel.h"
#include "module_wifi.h"
#include "module_capture.h"
#include "module_l2tp.h"

void __pfe_ctrl_cmd_handler(U16 fcode, U16 length, U16 *payload, U16 *rlen, U16 *rbuf)
{
	CmdProc cmdproc;
	U32 eventid;

	eventid = FCODE_TO_EVENT(fcode);
	cmdproc = gCmdProcTable[eventid];

	if (cmdproc)
	{
		memcpy(rbuf, payload, length);
		*rlen = (*cmdproc)(fcode, length, rbuf);
		if (*rlen == 0)
		{
			rbuf[0] = NO_ERR;
			*rlen = 2;
		}
	}
	else
	{
		rbuf[0] = ERR_UNKNOWN_COMMAND;
		*rlen = 2;
	}
}


int __pfe_ctrl_init(void)
{
	int rc;

	rc = tx_init();
	if (rc < 0)
		goto err_tx;

	rc = rx_init();
	if (rc < 0)
		goto err_rx;
	
	rc = pppoe_init();
	if (rc < 0)
		goto err_pppoe;

	rc = vlan_init();
	if (rc < 0)
		goto err_vlan;
	
	rc = ipv4_init();
	if (rc < 0)
		goto err_ipv4;

	rc = ipv6_init();
	if (rc < 0)
		goto err_ipv6;

	rc = mc4_init();
	if (rc < 0)
		goto err_mc4;

	rc = mc6_init();
	if (rc < 0)
		goto err_mc6;

#ifdef CFG_STATS
	rc = statistics_init();
	if (rc < 0)
		goto err_statistics;
#endif

	rc = socket_init();
	if (rc < 0)
		goto err_socket;

	rc = rtp_relay_init();
	if (rc < 0)
		goto err_rtp_relay;

#ifdef CFG_NATPT
	rc = natpt_init();
	if (rc < 0)
		goto err_natpt;
#endif

	rc = qm_init();
	if (rc < 0)
		goto err_qm;

#ifdef CFG_ICC
	rc = icc_control_init();
	if (rc < 0)
		goto err_icc;
#endif

	rc = bridge_init();
	if (rc < 0)
		goto err_bridge;

	rc = ipsec_init();
	if (rc < 0)
		goto err_ipsec;

	rc = tunnel_init();
	if (rc < 0)
		goto err_tunnel;

#ifdef CFG_WIFI_OFFLOAD
	rc = wifi_init();
	if (rc < 0)
		goto err_wifi;
#endif
#ifdef CFG_PCAP
	rc = pktcap_init();
	if (rc < 0)
		goto err_pcap;
#endif
	rc = l2tp_init();
	if (rc < 0)
		goto err_l2tp;
	return 0;
err_l2tp:
#ifdef CFG_PCAP
	pktcap_exit();
err_pcap:
#endif
#ifdef CFG_WIFI_OFFLOAD
	wifi_exit();
err_wifi:
#endif
	tunnel_exit();

err_tunnel:
	ipsec_exit();

err_ipsec:
	bridge_exit();

err_bridge:
#ifdef CFG_ICC
	icc_control_exit();

err_icc:
#endif
	qm_exit();

err_qm:
#ifdef CFG_NATPT
	natpt_exit();

err_natpt:
#endif
	rtp_relay_exit();

err_rtp_relay:
	socket_exit();

err_socket:
#ifdef CFG_STATS
	statistics_exit();
	
err_statistics:
#endif
	mc6_exit();

err_mc6:
	mc4_exit();

err_mc4:
	ipv6_exit();

err_ipv6:
	ipv4_exit();

err_ipv4:
	vlan_exit();

err_vlan:
	pppoe_exit();

err_pppoe:
	rx_exit();

err_rx:
	tx_exit();

err_tx:
	return rc;
}


void __pfe_ctrl_exit(void)
{
#ifdef CFG_PCAP
	pktcap_exit();
#endif

#ifdef CFG_WIFI_OFFLOAD
	wifi_exit();
#endif

	tunnel_exit();

	ipsec_exit();

	bridge_exit();

#ifdef CFG_ICC
	icc_control_exit();
#endif

	qm_exit();

	/* natpt_exit() and rtp_relay_exit() should be called before socket_exit() */
#ifdef CFG_NATPT
	natpt_exit();
#endif

	rtp_relay_exit();

	socket_exit();

#ifdef CFG_STATS
	statistics_exit();
#endif

	mc6_exit();

	mc4_exit();

	ipv6_exit();

	ipv4_exit();

	vlan_exit();

	pppoe_exit();

	rx_exit();

	tx_exit();
}
