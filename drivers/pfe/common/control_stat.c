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

#include "module_stat.h"
#include "module_tunnel.h"
#include "module_hidrv.h"
#include "module_timer.h"
#include "module_Rx.h"
#include "module_ipsec.h"
#include "fpp.h"
#include "fe.h"
#include "module_bridge.h"
#include "module_tunnel.h"

#ifdef CFG_STATS

int stat_Get_Next_SAEntry(PStatIpsecEntryResponse pSACmd, int reset_action);
void stat_pppoe_reset(pPPPoE_Info pEntry);
int stat_PPPoE_Get_Next_SessionEntry(PStatPPPoEEntryResponse pStatSessionCmd, int reset_action);
int rx_Get_Next_Hash_Stat_BridgeEntry(PStatBridgeEntryResponse pStatBridgeCmd, int reset_action);
void stat_vlan_reset(PVlanEntry pEntry);
int stat_VLAN_Get_Next_SessionEntry(PStatVlanEntryResponse pStatVlanCmd, int reset_action);
void stat_tunnel_reset(PTnlEntry pEntry);
int stat_tunnel_Get_Next_SessionEntry(PStatTunnelEntryResponse pStatTunnelCmd, int reset_action);

U32 gFpCtrlStatFeatureBitMask;

#if defined(COMCERTO_2000)

tFpCtrlStatistics gFpCtrlStatistics;
struct tmu_stats TMU_DMEM_SH2(stats);

U32 CLASS_DMEM_SH2(gFpStatFeatureBitMask);
U32 TMU_DMEM_SH2(gFpStatFeatureBitMask);
U32 UTIL_DMEM_SH2(gFpStatFeatureBitMask);


/** pe_stats32_get
* Gets a 32bit statistic counter from the PE dmem
*
* @param id		PE id
* @param dmem_addr	PE internal memory address for the statistics counter
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
static inline U32 pe_stats32_get(int id, U32 dmem_addr, U32 do_reset)
{
	U32 result;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	if (do_reset)
	{
		if (pe_sync_stop(ctrl, (1 << id)) < 0)
			return 0;
	}

	result = be32_to_cpu(pe_dmem_read(id, dmem_addr, 4));
	
	if (do_reset)
	{
		pe_dmem_write(id, 0, dmem_addr, 4);
		pe_start(ctrl, (1 << id));
	}

	return result;
}


/** pe_stats64_get
* Gets a 64bit statistic counter from the pe memory
*
* @param id		PE id
* @param dmem_addr	PE internal memory address for the statistics counter
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
static inline U64 pe_stats64_get(int id, U32 dmem_addr, U32 do_reset)
{
	U32 val_msb, val_lsb;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	/* Since neither the updating nor reading of 64-bit counters are atomic,
	we must always stop the PE to read them. */

	if (pe_sync_stop(ctrl, (1 << id)) < 0)
		return 0;

	val_msb = be32_to_cpu(pe_dmem_read(id, dmem_addr, 4));
	val_lsb = be32_to_cpu(pe_dmem_read(id, dmem_addr + 4, 4));

	if (do_reset)
	{
		pe_dmem_write(id, 0, dmem_addr, 4);
		pe_dmem_write(id, 0, dmem_addr + 4, 4);
	}

	pe_start(ctrl, (1 << id));
	return ((U64)val_msb << 32) | val_lsb;
}

/** pe_stats32_get_pe_lmem
* Gets a 32bit statistic counter from the PE lmem
*
* @param id		PE id
* @param addr		PE LMEM memory address for the statistics counter
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
static inline U32 pe_stats32_get_pe_lmem(int id, U32 addr, U32 do_reset)
{
	U32 result;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	if (do_reset)
	{
		if (pe_sync_stop(ctrl, (1 << id)) < 0)
			return 0;
	}

	result = be32_to_cpu(class_bus_readl(addr));

	if (do_reset)
	{
		class_bus_writel( 0, addr);
		pe_start(ctrl, (1 << id));
	}

	return result;
}


/** pe_stats64_get_pe_lmem
* Gets a 64bit statistic counter from the pe LMEM
*
* @param id		PE id
* @param addr		PE LMEM memory address for the statistics counter
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
static inline U64 pe_stats64_get_pe_lmem(int id, U32 addr, U32 do_reset)
{
	U32 val_msb, val_lsb;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	/* Since neither the updating nor reading of 64-bit counters are atomic,
	we must always stop the PE to read them. */

	if (pe_sync_stop(ctrl, (1 << id)) < 0)
		return 0;

	val_msb = be32_to_cpu(class_bus_readl(addr));
	val_lsb = be32_to_cpu(class_bus_readl(addr + 4));

	if (do_reset)
	{
		class_bus_writel( 0, addr);
		class_bus_writel( 0, addr + 4);
	}

	pe_start(ctrl, (1 << id));
	return ((U64)val_msb << 32) | val_lsb;
}


/** pe_stats32_get_ddr
* Gets a 32bit statistic counter from DDR
*
* @param id		PE id
* @param ddr_addr	DDR address for the statistics counter
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
static inline U32 pe_stats32_get_ddr(int id, U32 *ddr_addr, U32 do_reset)
{
	U32 result;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	if (do_reset)
	{
		if (pe_sync_stop(ctrl, (1 << id)) < 0)
			return 0;
	}

	result = be32_to_cpu(*ddr_addr);
	
	if (do_reset)
	{
		*ddr_addr = 0;
		pe_start(ctrl, (1 << id));
	}

	return result;
}


/** pe_stats64_get_ddr
* Gets a 64bit statistic counter from DDR
*
* @param id		PE id
* @param ddr_addr	DDR address for the statistics counter
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
static inline U64 pe_stats64_get_ddr(int id, U64 *ddr_addr, U32 do_reset)
{
	U64 result;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	/* Since neither the updating nor reading of 64-bit counters are atomic,
	we must always stop the PE to read them. */

	if (pe_sync_stop(ctrl, (1 << id)) < 0)
		return 0;

	result = be64_to_cpu(*ddr_addr);

	if (do_reset)
	{
		*ddr_addr = 0;
	}

	pe_start(ctrl, (1 << id));
	return result;
}

/** class_statistics_get
* Gets a statistic counter from the CLASS PE's dmem
*
* @param addr		PE internal memory address for the statistics counter
* @param val		Local memory address for the statistics counter
* @param size		Size in bytes of the counter being read (4 or 8)
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
void class_statistics_get(void *addr, void *val, U8 size, U32 do_reset)
{
	U32 dmem_addr = virt_to_class_dmem(addr);
	int id;

	if (size == 4)
		*(U32 *)val = 0;
	else
		*(U64 *)val = 0;
		
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
	{
		if (size == 4)
			*(U32 *)val += pe_stats32_get(id, dmem_addr, do_reset);
		else
			*(U64 *)val += pe_stats64_get(id, dmem_addr, do_reset);
	}
}

/** class_statistics_get_ddr
* Gets a statistic counter from DDR, combining results for each class PE
*
* @param addr		DDR address for the statistics counter array
* @param val		Local memory address for the statistics counter
* @param size		Size in bytes of the counter being read (4 or 8)
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
void class_statistics_get_ddr(void *addr, void *val, U8 size, U32 do_reset)
{
	int id;

	if (size == 4)
		*(U32 *)val = 0;
	else
		*(U64 *)val = 0;
		
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
	{
		if (size == 4)
			*(U32 *)val += pe_stats32_get_ddr(id, addr, do_reset);
		else
			*(U64 *)val += pe_stats64_get_ddr(id, addr, do_reset);
		addr += size;
	}
}

static int interface_to_tmu_id(U8 interface)
{
	if (interface == 0)
		return TMU0_ID;
	else if (interface == 1)
		return TMU1_ID;
	else
		return TMU2_ID;
}

/** tmu_statistics_get
* Gets a statistic counter from a TMU PE dmem
*
* @param interface	Physical port
* @param addr		PE internal memory address for the statistics counter
* @param val		Local memory address for the statistics counter
* @param size		Size in bytes of the counter being read (4 or 8)
* @param do_reset	flag to clear after reading (0=no, non-zero=yes)
*
*/
void tmu_statistics_get(U8 interface, void *addr, void *val, U8 size, U32 do_reset)
{
	int id = interface_to_tmu_id(interface);
	U32 dmem_addr = virt_to_tmu_dmem(addr);

	if (size == 4)
		*(U32 *)val = pe_stats32_get(id, dmem_addr, do_reset);
	else
		*(U64 *)val = pe_stats64_get(id, dmem_addr, do_reset);
}

/** pe_statistics_enable
* Enable the statistics feature bitmask in both Classifier and TMU internal memory
*
*/
static void pe_statistics_enable(U32 mask)
{
	int id;

	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
		pe_dmem_write(id, cpu_to_be32(mask), virt_to_class_dmem(&class_gFpStatFeatureBitMask), 4);

	for (id = TMU0_ID; id <= TMU2_ID; id++)
		pe_dmem_write(id, cpu_to_be32(mask), virt_to_tmu_dmem(&tmu_gFpStatFeatureBitMask), 4);

	pe_dmem_write(UTIL_ID, cpu_to_be32(mask), virt_to_util_dmem(&util_gFpStatFeatureBitMask), 4);

}

/* C2K PFE statistics counters are sometimes, spread over differents PEs */

static void stats_queue_reset(U8 interface, U8 queue)
{
	U64 temp;
	STATISTICS_TMU_GET(interface, emitted_pkts[queue], temp, TRUE);
	qm_read_drop_stat(interface, queue, NULL, TRUE);
	STATISTICS_TMU_GET(interface, peak_queue_occ[queue], temp, TRUE);
}

static void stats_queue_get(U8 interface, U8 queue, PStatQueueResponse rsp, U32 do_reset)
{
	STATISTICS_TMU_GET(interface, emitted_pkts[queue], rsp->emitted_pkts, do_reset);
	qm_read_drop_stat(interface, queue, &rsp->dropped_pkts, do_reset);
	STATISTICS_TMU_GET(interface, peak_queue_occ[queue], rsp->peak_queue_occ, do_reset);
}

static void stats_interface_pkt_reset(U8 interface)
{
	U64 temp;
	struct physical_port	*port = phy_port_get(interface);

	if( interface < MAX_PHY_PORTS_FAST) {

		STATISTICS_CLASS_GET_DMEM(port->rx_bytes, temp, TRUE);
		STATISTICS_CLASS_GET_DMEM(port->rx_pkts, temp, TRUE);
	}else{

		pe_stats64_get_pe_lmem(0, virt_to_class_pe_lmem(&port->rx_bytes), TRUE);
		pe_stats32_get_pe_lmem(0, virt_to_class_pe_lmem(&port->rx_pkts), TRUE);
	}

	STATISTICS_TMU_GET(interface, total_bytes_transmitted, temp, TRUE);
	STATISTICS_TMU_GET(interface, total_pkts_transmitted, temp, TRUE);
}

static void stats_interface_pkt_get(U8 interface, PStatInterfacePktResponse rsp, U32 do_reset)
{
	struct physical_port	*port = phy_port_get(interface);

	if( interface < MAX_PHY_PORTS_FAST) {
		STATISTICS_CLASS_GET_DMEM(port->rx_bytes, rsp->total_bytes_received[0], do_reset);
		STATISTICS_CLASS_GET_DMEM(port->rx_pkts, rsp->total_pkts_received, do_reset);
	}else{

		rsp->total_pkts_received += pe_stats64_get_pe_lmem(0, virt_to_class_pe_lmem(&port->rx_bytes), do_reset);
		*(U64 *)&rsp->total_bytes_received[0] += pe_stats32_get_pe_lmem(0, virt_to_class_pe_lmem(&port->rx_pkts), do_reset);
	}

	STATISTICS_TMU_GET(interface, total_bytes_transmitted, rsp->total_bytes_transmitted[0], do_reset);
	STATISTICS_TMU_GET(interface, total_pkts_transmitted, rsp->total_pkts_transmitted, do_reset);
}

#else

static void stats_queue_reset(U8 interface, U8 queue)
{
	STATISTICS_SET(emitted_pkts[interface][queue], 0);
	STATISTICS_SET(dropped_pkts[interface][queue], 0);
	STATISTICS_SET(peak_queue_occ[interface][queue], 0);
}

static void stats_queue_get(U8 interface, U8 queue, PStatQueueResponse rsp, U32 do_reset)
{
	STATISTICS_GET(emitted_pkts[interface][queue], rsp->emitted_pkts);
	STATISTICS_GET(dropped_pkts[interface][queue], rsp->dropped_pkts);
	STATISTICS_GET(peak_queue_occ[interface][queue], rsp->peak_queue_occ);
	if (do_reset)
		stats_queue_reset(interface, queue);
}

static void stats_interface_pkt_reset(U8 interface)
{
	STATISTICS_SET(total_bytes_received[interface], 0);
	STATISTICS_SET(total_pkts_received[interface], 0);

	STATISTICS_SET(total_bytes_transmitted[interface], 0);
	STATISTICS_SET(total_pkts_transmitted[interface], 0);
}

static void stats_interface_pkt_get(U8 interface, PStatInterfacePktResponse rsp, U32 do_reset)
{
	STATISTICS_GET_LSB(total_bytes_received[interface], rsp->total_bytes_received[0], U32);
	STATISTICS_GET_MSB(total_bytes_received[interface], rsp->total_bytes_received[1], U32);

	STATISTICS_GET(total_pkts_received[interface], rsp->total_pkts_received);

	STATISTICS_GET_LSB(total_bytes_transmitted[interface], rsp->total_bytes_transmitted[0], U32);
	STATISTICS_GET_MSB(total_bytes_transmitted[interface], rsp->total_bytes_transmitted[1], U32);
	STATISTICS_GET(total_pkts_transmitted[interface], rsp->total_pkts_transmitted);

	if (do_reset)
		stats_interface_pkt_reset(interface);
}

#endif


static U16 stats_queue(U8 action, U8 queue, U8 interface, PStatQueueResponse statQueueRsp, U16 *acklen)
{
	U16 ackstatus = CMD_OK;

	if(action & FPP_STAT_QUERY)
	{
		stats_queue_get(interface, queue, statQueueRsp, action & FPP_STAT_RESET);

		*acklen = sizeof(StatQueueResponse);
	}
	else if(action & FPP_STAT_RESET)
	{
		stats_queue_reset(interface, queue);
	}
	else
		ackstatus = ERR_WRONG_COMMAND_PARAM;

	return ackstatus;
}

static U16 stats_interface_pkt(U8 action, U8 interface, PStatInterfacePktResponse statInterfacePktRsp, U16 *acklen)
{
	U16 ackstatus = CMD_OK;

	if(action & FPP_STAT_QUERY)
	{
		stats_interface_pkt_get(interface, statInterfacePktRsp, action & FPP_STAT_RESET);

		statInterfacePktRsp->rsvd1 = 0;
		*acklen = sizeof(StatInterfacePktResponse);
	}
	else if(action & FPP_STAT_RESET)
	{
		stats_interface_pkt_reset(interface);
	}
	else
		ackstatus = ERR_WRONG_COMMAND_PARAM;

	return ackstatus;
}



static U16 stats_enable(U8 action, U32 bitmask)
{
	U16 ackstatus = CMD_OK;

	if (action == FPP_STAT_ENABLE)
		gFpCtrlStatFeatureBitMask |= bitmask;
	else if (action == FPP_STAT_DISABLE)
		gFpCtrlStatFeatureBitMask &= ~bitmask;
	else
		ackstatus = ERR_WRONG_COMMAND_PARAM;

	/* now update data path's feature bitmask  */
#if defined(COMCERTO_2000)
	pe_statistics_enable(gFpCtrlStatFeatureBitMask);
#else
	gFpStatFeatureBitMask = gFpCtrlStatFeatureBitMask;
#endif

	return ackstatus;
}


static U16 stats_connection(U16 action, PStatConnResponse statConnRsp, U16 *acklen)
{
	U16 ackstatus = CMD_OK;

	if(action & FPP_STAT_QUERY)
	{
		STATISTICS_CTRL_GET(max_active_connections, statConnRsp->max_active_connections);
		STATISTICS_CTRL_GET(num_active_connections, statConnRsp->num_active_connections);
		*acklen = sizeof(StatConnResponse);
	}
	else
		ackstatus = ERR_WRONG_COMMAND_PARAM;

	return ackstatus;
}


/**
 * M_stat_cmdproc
 *
 *
 *
 */
static U16 M_stat_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 acklen;
	U16 ackstatus;
	U16 action;
	U32 bitmask;
	U16 queue;
	U16 interface;
	StatEnableCmd enableDisableCmd;
	StatQueueCmd queueCmd;
	PStatQueueResponse statQueueRsp;
	StatInterfaceCmd intPktCmd;
	PStatInterfacePktResponse statInterfacePktRsp;
	StatConnectionCmd connCmd;
	PStatConnResponse statConnRsp;
	StatBridgeStatusCmd bridgeStatusCmd;
	StatPPPoEStatusCmd pppoeStatusCmd;
	StatIpsecStatusCmd ipsecStatusCmd;
	StatVlanStatusCmd vlanStatusCmd;	
	StatTunnelStatusCmd tunnelStatusCmd;

	acklen = 2;
	ackstatus = CMD_OK;

	switch (cmd_code)
	{

	case CMD_STAT_ENABLE:

		// Ensure alignment
		SFL_memcpy((U8*)&enableDisableCmd, (U8*)pcmd, sizeof(StatEnableCmd));

		action = enableDisableCmd.action;
		bitmask = enableDisableCmd.bitmask;
		ackstatus = stats_enable(action, bitmask);

		break;

	case CMD_STAT_QUEUE:

		if(gFpCtrlStatFeatureBitMask & STAT_QUEUE_BITMASK) 
		{
			// Ensure alignment
			SFL_memcpy((U8*)&queueCmd, (U8*)pcmd, sizeof(StatQueueCmd));

			action = queueCmd.action;
			queue = queueCmd.queue;
			interface = queueCmd.interface;
			statQueueRsp = 	(PStatQueueResponse)pcmd;

			ackstatus = stats_queue(action, queue, interface, statQueueRsp, &acklen);
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;
		
		break;


	case CMD_STAT_INTERFACE_PKT:

		if(gFpCtrlStatFeatureBitMask & STAT_INTERFACE_BITMASK) 
		{
			// Ensure alignment
			SFL_memcpy((U8*)&intPktCmd, (U8*)pcmd, sizeof(StatInterfaceCmd));

			interface = intPktCmd.interface;
			action = intPktCmd.action;
			statInterfacePktRsp = (PStatInterfacePktResponse)pcmd;

			ackstatus = stats_interface_pkt(action, interface, statInterfacePktRsp, &acklen);
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;
		
		break;


	case CMD_STAT_CONN:
		
		// Ensure alignment
		SFL_memcpy((U8*)&connCmd, (U8*)pcmd, sizeof(StatConnectionCmd));

		action = connCmd.action;

		statConnRsp = (PStatConnResponse)pcmd;

		ackstatus = stats_connection(action, statConnRsp, &acklen);
		
		break;

	
	case CMD_STAT_PPPOE_STATUS: {
		int x;
		struct slist_entry *entry;
		pPPPoE_Info pEntry;

		if (gFpCtrlStatFeatureBitMask & STAT_PPPOE_BITMASK) {
			// Ensure alignment
			SFL_memcpy((U8*)&pppoeStatusCmd, (U8*)pcmd, sizeof(StatPPPoEStatusCmd));

			action = pppoeStatusCmd.action;

			if (action == FPP_STAT_RESET)
			{
				/* Reset the packet counters for all PPPoE Entries */
				for (x = 0; x < NUM_PPPOE_ENTRIES; x++)
				{
					slist_for_each(pEntry, entry, &pppoe_cache[x], list)
						stat_pppoe_reset(pEntry);
				}
			}
			else if ((action == FPP_STAT_QUERY) || (action == FPP_STAT_QUERY_RESET))
			{
				gStatPPPoEQueryStatus = 0;
				if (action == FPP_STAT_QUERY_RESET)
					gStatPPPoEQueryStatus |= STAT_PPPOE_QUERY_RESET;

				stat_PPPoE_Get_Next_SessionEntry((PStatPPPoEEntryResponse)pcmd, 1);
			}
			else
				ackstatus = ERR_WRONG_COMMAND_PARAM;
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;
		
		break;
	}


	case CMD_STAT_PPPOE_ENTRY:{
		int result;

		PStatPPPoEEntryResponse prsp = (PStatPPPoEEntryResponse)pcmd;

		if (gFpCtrlStatFeatureBitMask & STAT_PPPOE_BITMASK)
		{
			result = stat_PPPoE_Get_Next_SessionEntry(prsp, 0);
			if (result != NO_ERR)
			{
				prsp->eof = 1;
			}

			acklen = sizeof(StatPPPoEEntryResponse);
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;

		break;
	}


	case CMD_STAT_BRIDGE_STATUS:	{
		int x;
		struct slist_entry *entry;
		PL2Bridge_entry pEntry;

                if((L2_BRIDGE_MODE_MANUAL == l2bridge_get_mode()) && (gFpCtrlStatFeatureBitMask & STAT_BRIDGE_BITMASK)) {
		
			// Ensure alignment
			SFL_memcpy((U8*)&bridgeStatusCmd, (U8*)pcmd, sizeof(StatBridgeStatusCmd));

			action = bridgeStatusCmd.action;

			if(action == FPP_STAT_RESET)
			{
				/* Reset the packet counter for all Bridge Entries */	
				for(x=0; x<NUM_BT_ENTRIES;x++) {

					slist_for_each(pEntry, entry, &bridge_cache[x], list)
					{
#if defined(COMCERTO_2000)
						U32 temp;
						class_statistics_get_ddr(pEntry->hw_l2bridge->total_packets_transmitted,
								&temp, sizeof(temp), TRUE);
#else
						pEntry->total_packets_transmitted = 0;
#endif
					}			
				}

			}
			else if( (action == FPP_STAT_QUERY) || (action == FPP_STAT_QUERY_RESET))
			{
				gStatBridgeQueryStatus = 0;
				if(action == FPP_STAT_QUERY_RESET)
					gStatBridgeQueryStatus |= STAT_BRIDGE_QUERY_RESET;
				rx_Get_Next_Hash_Stat_BridgeEntry((PStatBridgeEntryResponse)pcmd, 1);
			}
			else
				ackstatus = ERR_WRONG_COMMAND_PARAM;
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;

		break;
	}


	case CMD_STAT_BRIDGE_ENTRY:{
		int result;
		
		PStatBridgeEntryResponse prsp = (PStatBridgeEntryResponse)pcmd;

                if((L2_BRIDGE_MODE_MANUAL == l2bridge_get_mode()) && (gFpCtrlStatFeatureBitMask & STAT_BRIDGE_BITMASK)) {
			
			result = rx_Get_Next_Hash_Stat_BridgeEntry(prsp, 0);
			if (result != NO_ERR )
			{
				prsp->eof = 1;
			}
			acklen = sizeof(StatBridgeEntryResponse);
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;
		
		break;
	}



	case CMD_STAT_IPSEC_STATUS:	{
		int x;
		PSAEntry pEntry;
		struct slist_entry *entry;

		if(gFpCtrlStatFeatureBitMask & STAT_IPSEC_BITMASK) {
		
			// Ensure alignment
			SFL_memcpy((U8*)&ipsecStatusCmd, (U8*)pcmd, sizeof(StatIpsecStatusCmd));

			action = ipsecStatusCmd.action;

			if(action == FPP_STAT_RESET)
			{
#if defined(COMCERTO_2000)
				pe_send_cmd(CMD_STAT_IPSEC_STATUS, action, 0, 0);
#endif
				/* Reset the packet counter for all SA Entries */	
				for(x=0; x<NUM_SA_ENTRIES;x++) {

					slist_for_each(pEntry, entry, &sa_cache_by_h[x], list_h)
					{
						#if !defined(COMCERTO_2000)
							pEntry->total_pkts_processed = 0;
							pEntry->total_bytes_processed = 0;
						#endif
					}
				}

			}
			else if( (action == FPP_STAT_QUERY) || (action == FPP_STAT_QUERY_RESET))
			{
				gStatIpsecQueryStatus = 0;
				
				if(action == FPP_STAT_QUERY_RESET)
				{
					gStatIpsecQueryStatus |= STAT_IPSEC_QUERY_RESET;
				}
				
#if defined(COMCERTO_2000)
				pe_send_cmd(CMD_STAT_IPSEC_STATUS, action, 0, 0);
#endif
				/* This function just initializes the static variables and returns */
				stat_Get_Next_SAEntry((PStatIpsecEntryResponse)pcmd, 1);
				
			}

			else
				ackstatus = ERR_WRONG_COMMAND_PARAM;
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;
			
					
		break;
	}


	case CMD_STAT_IPSEC_ENTRY:
	{
		int  result; 
		//PSAEntry pEntry;
		PStatIpsecEntryResponse prsp = (PStatIpsecEntryResponse)pcmd;

		if (gFpCtrlStatFeatureBitMask & STAT_IPSEC_BITMASK)
		{
			result = stat_Get_Next_SAEntry(prsp, 0);
			if (result != NO_ERR)
			{
				prsp->eof = 1;
			}

			acklen = sizeof(StatIpsecEntryResponse);
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;

		break;
	}
	
	case CMD_STAT_VLAN_STATUS: {
		int x;
		PVlanEntry pEntry;
		struct slist_entry *entry;

		if (gFpCtrlStatFeatureBitMask & STAT_VLAN_BITMASK)
		{
			// Ensure alignment
			SFL_memcpy((U8*)&vlanStatusCmd, (U8*)pcmd, sizeof(StatVlanStatusCmd));

			action = vlanStatusCmd.action;

			if (action == FPP_STAT_RESET)
			{
				/* Reset the packet counters for all VLAN Entries */
				for (x = 0; x < NUM_VLAN_ENTRIES; x++)
				{
					slist_for_each(pEntry, entry, &vlan_cache[x], list)
						stat_vlan_reset(pEntry);
				}
			}
			else if ((action == FPP_STAT_QUERY) || (action == FPP_STAT_QUERY_RESET))
			{
				gStatVlanQueryStatus = 0;
				if (action == FPP_STAT_QUERY_RESET)
					gStatVlanQueryStatus |= STAT_VLAN_QUERY_RESET;

				stat_VLAN_Get_Next_SessionEntry((PStatVlanEntryResponse)pcmd, 1);
			}
			else
				ackstatus = ERR_WRONG_COMMAND_PARAM;
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;

		break;
	}


	case CMD_STAT_VLAN_ENTRY:{
		int result;
		
		PStatVlanEntryResponse prsp = (PStatVlanEntryResponse)pcmd;

		if (gFpCtrlStatFeatureBitMask & STAT_VLAN_BITMASK)
		{
		
			result = stat_VLAN_Get_Next_SessionEntry(prsp, 0);
			if (result != NO_ERR)
			{
				prsp->eof = 1;
			}

			acklen = sizeof(StatVlanEntryResponse);
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;

		break;
	}

	case CMD_STAT_TUNNEL_STATUS: {
		int x;
		PTnlEntry pEntry;
		struct slist_entry *entry;

		if (gFpCtrlStatFeatureBitMask & STAT_TUNNEL_BITMASK)
		{
			// Ensure alignment
			SFL_memcpy((U8*)&tunnelStatusCmd, (U8*)pcmd, sizeof(StatTunnelStatusCmd));

			action = tunnelStatusCmd.action;
			if (action == FPP_STAT_RESET)
			{
				/* Reset the packet counters for all Tunnel Entries */
				for (x = 0; x < NUM_GRE_TUNNEL_ENTRIES; x++)
				{
					slist_for_each(pEntry, entry, &gre_tunnel_cache[x], list)
						stat_tunnel_reset(pEntry);
				}
			}
			else if ((action == FPP_STAT_QUERY) || (action == FPP_STAT_QUERY_RESET))
			{
				gStatTunnelQueryStatus = 0;
				if (action == FPP_STAT_QUERY_RESET)
					gStatTunnelQueryStatus |= STAT_TUNNEL_QUERY_RESET;

				stat_tunnel_Get_Next_SessionEntry((PStatTunnelEntryResponse)pcmd, 1);
			}
			else
				ackstatus = ERR_WRONG_COMMAND_PARAM;
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;

		break;
	}


	case CMD_STAT_TUNNEL_ENTRY:{
		int result;

		PStatTunnelEntryResponse prsp = (PStatTunnelEntryResponse)pcmd;

		if (gFpCtrlStatFeatureBitMask & STAT_TUNNEL_BITMASK)
		{
			result = stat_tunnel_Get_Next_SessionEntry(prsp, 0);
			if (result != NO_ERR)
			{
				prsp->eof = 1;
			}
			acklen = sizeof(StatTunnelEntryResponse);
		}
		else
			ackstatus = ERR_STAT_FEATURE_NOT_ENABLED;

		break;
	}

	default:
		ackstatus = CMD_ERR;
		break;
	}

	*pcmd = ackstatus;
	return acklen;
}


int statistics_init(void)
{
#if !defined(COMCERTO_2000)
	/* this module has no dedicated entry point as statistics computation is done within others modules */
	set_event_handler(EVENT_STAT, NULL);
#endif

	set_cmd_handler(EVENT_STAT, M_stat_cmdproc);

	/*The combined memory of (ARAM_HEAP_SIZE + DDR_HEAP_SIZE) is consumed by 
	- N*2 Ct Entries, 1 Route Entry, 1 Arp Entry for unidirectional connections
	- N*2 Ct Entries, 2 Route Entries, 2 Arp Entries for bidirectional connections

	Size of Ct Entry = 64 bytes
	Size of Route Entry = 64 bytes
	Size of Arp Entry = 20 bytes, but Heap Manager would allocate 32 bytes for this.

	Maximum Number of Unidirectional connections:
	ARAM_HEAP_SIZE + DDR_HEAP_SIZE = N*2*64 + 64 + 32

	Maximum Number of Bidirectional connections:
	ARAM_HEAP_SIZE + DDR_HEAP_SIZE = N*2*64 + 2*64 + 2*32 */
#if !defined(COMCERTO_2000)
	gFpCtrlStatistics.FS_max_active_connections = (ARAM_HEAP_SIZE + DDR_HEAP_SIZE - 64 - 32 ) / (2 * 64) ;
#else
	gFpCtrlStatistics.FS_max_active_connections = CONNTRACK_MAX; /* Value specified in CMM */
#endif
	gStatBridgeQueryStatus = 0;
	gStatPPPoEQueryStatus = 0;
	gStatIpsecQueryStatus = 0;
	gFpCtrlStatFeatureBitMask = 0;

	return 0;
}

void statistics_exit(void)
{

}



#endif /* CFG_STATS */
