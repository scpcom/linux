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

#include "module_wifi.h"
#include "fpp_globals.h"
#include "events.h"
#include "module_Rx.h"
#include "module_tx.h"

#include "gemac.h"
#include "xdma.h"
#include "fpool.h"
#include "fpp.h"
#include "module_ethernet.h"
#include "module_hidrv.h"
#include "module_timer.h"
#include "module_expt.h"
#include "module_pppoe.h"
#include "module_stat.h"
#include "fe.h"
#include "channels.h"
//#include "scc.h"
#include "module_ipv4.h"
#include "module_bridge.h"
#include "control_bridge.h"

#ifdef CFG_WIFI_OFFLOAD

#ifdef CFG_WIFI_GUEST
U32 wifi_guest_mask;
U32 CLASS_DMEM_SH2(wifi_guest_mask);
U32 UTIL_DMEM_SH2(wifi_guest_mask);
#endif

struct 	tWifiIfDesc wifiDesc[MAX_WIFI_VAPS];

struct tRX_wifi_context gWifiRxCtx;

#if defined(COMCERTO_2000_CONTROL) || defined(COMCERTO_1000)

static int wifi_vap_entry( U16 *ptr, U16 len )
{
	struct wifiCmd cmd;
	struct tRX_wifi_context *rxc;
	int id;
	int portid;
	struct physical_port	*port;
#ifdef CFG_WIFI_GUEST
	U32 mask_be;
#endif

	rxc = &gWifiRxCtx;
	//printk("%s:%d\n", __func__, __LINE__);

	if (len != sizeof(struct wifiCmd))
		return ERR_WRONG_COMMAND_SIZE;

	SFL_memcpy( &cmd, ptr, sizeof(struct wifiCmd));

	if( cmd.VAPID >= MAX_WIFI_VAPS )
		return ERR_UNKNOWN_ACTION;

	portid = PORT_WIFI_IDX + cmd.VAPID;
	port = phy_port_get(portid);

	switch (cmd.action)
	{
		case WIFI_REMOVE_VAP:
			printk("%s:%d Remove entry\n", __func__, __LINE__);
			if( wifiDesc[cmd.VAPID].VAPID == 0XFFFF )
				return ERR_WLAN_DUPLICATE_OPERATION;
			//printk("%s: PHYID:%d vapid:%d\n", __func__, portid, cmd.VAPID);

#ifdef CFG_WIFI_GUEST
			wifi_guest_mask &= ~(1 << cmd.VAPID);
			mask_be = cpu_to_be32(wifi_guest_mask);
			for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
				pe_dmem_writel(id, mask_be, virt_to_class_dmem(&class_wifi_guest_mask));
			pe_dmem_writel(UTIL_ID, mask_be, virt_to_util_dmem(&util_wifi_guest_mask));
#endif

			wifiDesc[cmd.VAPID].VAPID = 0xFFFF;

			bridge_interface_deregister(portid);

			remove_onif_by_index(port->itf.index);

			if ( rxc->users  )
				rxc->users--;

			break;

		case WIFI_ADD_VAP:
			if ( rxc->users >= MAX_WIFI_VAPS )
				return CMD_ERR;

			printk("%s:%d ADD entry\n", __func__, __LINE__);
			if( wifiDesc[cmd.VAPID].VAPID != 0XFFFF )
				return ERR_WLAN_DUPLICATE_OPERATION;

			if(!add_onif(cmd.ifname, &port->itf, NULL, IF_TYPE_WLAN | IF_TYPE_PHYSICAL))
			{
				return CMD_ERR;
			}

			wifiDesc[cmd.VAPID].VAPID = cmd.VAPID;
			bridge_interface_register(cmd.ifname, portid);

			SFL_memcpy(port->mac_addr, cmd.mac_addr, 6);
#if defined(COMCERTO_2000_CONTROL)
			printk("%s: PHYID:%d vapid:%d mac_addr: %02x:%02x:%02x:%02x:%02x:%02x\n", __func__, portid, cmd.VAPID,
									port->mac_addr[0],
									port->mac_addr[1],
									port->mac_addr[2],
									port->mac_addr[3],
									port->mac_addr[4],
									port->mac_addr[5]);
			if( portid < MAX_PHY_PORTS_FAST) {
				for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++) {
					pe_dmem_memcpy_to32(id, virt_to_class_dmem(&port->mac_addr),
							port->mac_addr, 6);
					pe_dmem_writeb(id, port->itf.index, virt_to_class_dmem(&port->itf.index));
				}
			} else {
				class_pe_lmem_memcpy_to32(virt_to_class_pe_lmem(&port->mac_addr),
						&port->mac_addr[0], 6);
				class_bus_writeb(port->itf.index, virt_to_class_pe_lmem(&port->itf.index));
			}

#ifdef CFG_WIFI_GUEST
			wifi_guest_mask &= ~(1 << cmd.VAPID);
			if (cmd.wifi_guest_flag)
				wifi_guest_mask |= 1 << cmd.VAPID;
			mask_be = cpu_to_be32(wifi_guest_mask);
			for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
				pe_dmem_writel(id, mask_be, virt_to_class_dmem(&class_wifi_guest_mask));
			pe_dmem_writel(UTIL_ID, mask_be, virt_to_util_dmem(&util_wifi_guest_mask));
#endif

#endif

			if ( rxc->users < MAX_WIFI_VAPS )
				rxc->users++;

			break;

		case WIFI_UPDATE_VAP:
			printk("%s:%d Update Entry\n", __func__, __LINE__);
			if( wifiDesc[cmd.VAPID].VAPID == 0XFFFF )
				return CMD_ERR;

			SFL_memcpy(port->mac_addr, cmd.mac_addr, 6);
#if defined(COMCERTO_2000_CONTROL)
			printk("%s: PHYID:%d vapid:%d mac_addr: %02x:%02x:%02x:%02x:%02x:%02x\n", __func__, portid, cmd.VAPID,
									port->mac_addr[0],
									port->mac_addr[1],
									port->mac_addr[2],
									port->mac_addr[3],
									port->mac_addr[4],
									port->mac_addr[5]);
			if( portid < MAX_PHY_PORTS_FAST) {
				for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
					pe_dmem_memcpy_to32(id, virt_to_class_dmem(&port->mac_addr),
							port->mac_addr, 6);
			} else {
				class_pe_lmem_memcpy_to32(virt_to_class_pe_lmem(&port->mac_addr),
						&port->mac_addr[0], 6);
			}

#ifdef CFG_WIFI_GUEST
			wifi_guest_mask &= ~(1 << cmd.VAPID);
			if (cmd.wifi_guest_flag)
				wifi_guest_mask |= 1 << cmd.VAPID;
			mask_be = cpu_to_be32(wifi_guest_mask);
			for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
				pe_dmem_writel(id, mask_be, virt_to_class_dmem(&class_wifi_guest_mask));
			pe_dmem_writel(UTIL_ID, mask_be, virt_to_util_dmem(&util_wifi_guest_mask));
#endif

#endif
			break;

		default:
			return ERR_UNKNOWN_ACTION;


	}

	return NO_ERR;


}


static U16 M_wifi_rx_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 acklen;
	U16 ackstatus;
	U16 i;
	struct tRX_wifi_context *rxc;
	struct physical_port	*port;

	rxc = &gWifiRxCtx;

	acklen = 2;
	ackstatus = CMD_OK;
	printk("%s:%d\n", __func__, __LINE__);

	switch (cmd_code)
	{
		case CMD_WIFI_VAP_ENTRY:
			ackstatus = wifi_vap_entry(pcmd, cmd_len);
			break;

#if defined(COMCERTO_1000)
		case CMD_CFG_WIFI_OFFLOAD:
			if( !rxc->enabled )
			{
				M_expt_rx_enable(PORT_WIFI_IDX);
				M_wifi_rx_enable();
			}
			else
				ackstatus = CMD_ERR;
			break;

		case CMD_WIFI_DISABLE:
			if( rxc->enabled )
			{
				M_expt_rx_disable(PORT_WIFI_IDX);
				M_wifi_rx_flush();
				M_wifi_rx_disable();
			}
			else
				ackstatus = CMD_ERR;
			break;
#endif

		case CMD_WIFI_VAP_QUERY: {
						 wifi_vap_query_response_t *vaps;
						 vaps = (wifi_vap_query_response_t *)pcmd;
						 printk("%s:%d\n", __func__, __LINE__);

						 for (i = 0; i < MAX_WIFI_VAPS; i++)
						 {
							 vaps[i].vap_id = wifiDesc[i].VAPID;
							 if( vaps[i].vap_id != 0xFFFF )
								 vaps[i].phy_port_id = PORT_WIFI_IDX + i;
							 port = phy_port_get(PORT_WIFI_IDX + i);

							 SFL_memcpy(vaps[i].ifname, get_onif_name(port->itf.index), IF_NAME_SIZE);
						 }

						 acklen += ( MAX_WIFI_VAPS * sizeof(wifi_vap_query_response_t));
						 break;
					 }

		case CMD_WIFI_VAP_RESET:
					 printk("%s:%d\n", __func__, __LINE__);
					 for (i = 0; i < MAX_WIFI_VAPS; i++)
					 {
						 if( wifiDesc[i].VAPID != 0XFFFF )
						 {
							 wifiDesc[i].VAPID = 0xFFFF;
							 port = phy_port_get(PORT_WIFI_IDX + i);

							 remove_onif_by_index(port->itf.index);

							 if ( rxc->users  )
								 rxc->users--;
						 }
					 }
					 break;

		default:
					 ackstatus = CMD_ERR;
					 break;
	}

	*pcmd = ackstatus;
	return acklen;
}

int onif_is_wifi( POnifDesc pOnif )
{
	int i;
	PWifiIfDesc pWifi;

	for( i = 0; i < MAX_WIFI_VAPS; i++ )
	{
		pWifi = (PWifiIfDesc)&wifiDesc[i];

		if( ( pWifi->VAPID != 0xFFFF ) && ((void *)pWifi == pOnif->itf)  )
		{
			return 1;
		}
	}

	return 0;
}
#endif

#if defined(COMCERTO_1000)
//	M_wifi_rx_enable
//	Enables WiFi fast path queue from ACP->FPP
static void M_wifi_rx_enable(void)
{
	struct tRX_wifi_context *rxc;

	DISABLE_INTERRUPTS();

	rxc = &gWifiRxCtx;

	rxc->rxToCleanIndex = 0;

	HAL_arm1_fiq_enable_1(rxc->PKTTX_irqm);

	// program expt path ring buffer base address
	rxc->rxRing_baseaddr = *(U32*)(rxc->SMI_baseaddr + FPP_SMI_WIFI_RXBASE);

	rxc->enabled = 1;

	//Initialize Rx Offset value, this used by VWD
	*(U32 *)(rxc->SMI_baseaddr + FPP_SMI_WIFI_RX_OFFSET) = BaseOffset;

	ENABLE_INTERRUPTS();


	return ;

}


//	M_wifi_rx_disable
//	Disables WiFi fast path queue from ACP->FPP
static void M_wifi_rx_disable(void)
{
	struct tRX_wifi_context *rxc;

	DISABLE_INTERRUPTS();

	rxc = &gWifiRxCtx;

	HAL_arm1_fiq_disable_1(rxc->PKTTX_irqm);

	rxc->enabled = 0;

	ENABLE_INTERRUPTS();


	return ;

}


static void M_wifi_rx_flush(void)
{
	struct tRX_wifi_context *rxc;
	struct tWiFiRXdesc ThisRXdesc;
	U8* bptr;
	U16 boffset;
	U16 rtc;

	rxc = &gWifiRxCtx;
	rtc = rxc->rxToCleanIndex;

	if ((*(V32*)(rxc->SMI_baseaddr + FPP_SMI_CTRL ) & WIFI_RX_EN) == 0)
		return;

	DISABLE_INTERRUPTS();
	L1_dc_linefill_disable();
	// something from wifi
	*((U64*) &ThisRXdesc) = Read64((U64*)(rxc->rxRing_baseaddr + sizeof(struct tWiFiRXdesc)*rtc));
	L1_dc_linefill_enable();
	ENABLE_INTERRUPTS();

	while ((ThisRXdesc.rx_status0 & WIFIRX_USED_MASK) == 0) {
		bptr = (U8*)ThisRXdesc.rx_data;
		boffset = (ThisRXdesc.rx_status0 & WIFIRX_OFFSET_MASK) >> WIFIRX_OFFSET_SHIFT;

		buffer_put(bptr - boffset, POOLB, 0);
		COUNTER_INCREMENT(packets_dropped_poolB);

		if (ThisRXdesc.rx_status0 & WIFIRX_WRAP)
			rtc = 0;
		else
			rtc++;


		DISABLE_INTERRUPTS();
		L1_dc_linefill_disable();
		*((U64*) &ThisRXdesc) = Read64((U64*)(rxc->rxRing_baseaddr + sizeof(struct tWiFiRXdesc)*rtc));
		L1_dc_linefill_enable();
		ENABLE_INTERRUPTS();
	}

	// re-enable interrupt
	DISABLE_INTERRUPTS();
	HAL_arm1_fiq_enable_1(rxc->PKTTX_irqm);
	ENABLE_INTERRUPTS();

	rxc->rxToCleanIndex = rtc;
}
#endif


void M_wifi_init_rx(void)
{
	int i;
	struct physical_port	*port;

#if defined(COMCERTO_1000)
	struct tRX_wifi_context *rxc;
	rxc = &gWifiRxCtx;

	rxc->SMI_baseaddr = SMI_WIFI_BASE;    //Shared memory base address
	rxc->PKTTX_irqm        = IRQM_CSPVWDTX;
	rxc->PKTRX_irqm        = IRQM_CSPVWDRX;

	set_event_handler(EVENT_PKT_WIFIRX, M_wifi_rx_entry);
#endif

	set_cmd_handler(EVENT_PKT_WIFIRX, M_wifi_rx_cmdproc);

	for ( i = 0; i < MAX_WIFI_VAPS; i++ )
	{
		wifiDesc[i].VAPID = 0xFFFF;
#if defined(COMCERTO_1000)
		tx_channel[PORT_WIFI_IDX + i] = EVENT_EXPT;
#endif
		port = phy_port_get(PORT_WIFI_IDX + i);
		port->id = PORT_WIFI_IDX + i;
	}
}

int wifi_init(void)
{
	M_wifi_init_rx();

	return 0;
}

void wifi_exit(void)
{
}
#endif /* CFG_WIFI_OFFLOAD */
