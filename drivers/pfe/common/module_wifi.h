/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
*/
#ifndef _MODULE_WIFI_H_
#define _MODULE_WIFI_H_

#include "types.h"
#include "modules.h"
#include "layer2.h"
#include "system.h"

#ifdef CFG_WIFI_OFFLOAD

#if defined(COMCERTO_1000) || defined(COMCERTO_2000_CONTROL)
typedef struct tWifiIfDesc
{
//	struct itf itf;
//	U16 action;
	int VAPID;
//	U8  mac_addr[6];
//	U8  ifname[12];
//	POnifDesc	phys_onif;
//	U8 if_index;
//	U8 port_id;
}WifiIfDesc, *PWifiIfDesc;

typedef struct tRX_wifi_context {
   U16 users;
   U16  enabled;
#if defined(COMCERTO_1000)
   U32 SMI_baseaddr;    //Shared memory base address
   U32 rxRing_baseaddr; //ACP to FPP Queue base address
   U16 rxToCleanIndex;
   U16  port_id;
   U32 if_count;
   int PKTTX_irqm;
   int PKTRX_irqm;
#endif
}RX_wifi_context;

typedef struct wifi_vap_query_response
{
        U16       vap_id;
        char      ifname[IF_NAME_SIZE];
        U16       phy_port_id;
}wifi_vap_query_response_t;

struct wifiCmd
{
	U16 action;
	U16 VAPID;
	U8  ifname[IF_NAME_SIZE];
	U8  mac_addr[6];
	U16 wifi_guest_flag;
};
#define WIFI_ADD_VAP       0
#define WIFI_REMOVE_VAP    1
#define WIFI_UPDATE_VAP    2


int onif_is_wifi( POnifDesc onif );
int wifi_init(void);
void wifi_exit(void);
#endif

#if defined(COMCERTO_1000)

typedef struct tTX_wifi_context {
   U32   SMI_baseaddr;   //Shared memory base address
   U32   txRing_baseaddr;
   U16   tx_index;       // points to new entry to write
   U16   tx_avail;
   U16    enabled;
   U16    if_count;
   int PKTTX_irqm;
   int PKTRX_irqm;
   int users;
}TX_wifi_context ;


typedef struct tRX_wifi_context {
   U32 SMI_baseaddr;    //Shared memory base address
   U32 rxRing_baseaddr; //ACP to FPP Queue base address
   U16 rxToCleanIndex;
   U8  port_id;
   U8  enabled;
   U32 if_count;
   int PKTTX_irqm;
   int PKTRX_irqm;
   int users;
  }RX_wifi_context;

#define  SMI_WIFI_BASE 		        0x0A000480
#define  FPP_SMI_WIFI_CTRL		0x00
#define  FPP_SMI_WIFI_EXPT_RXBASE       0x04
#define  FPP_SMI_WIFI_EXPT_TXBASE       0x08
#define  FPP_SMI_WIFI_TXEXTBASE	        0x0C
#define  FPP_SMI_WIFI_RXBASE		0x10
#define  FPP_SMI_WIFI_TXBASE		0x14

#define  FPP_SMI_WIFI_RX_OFFSET		0x18


#define WIFI_RX_EN           (1UL<<31)      /* Enable transmit circuits */
#define WIFI_TX_EN           (1UL<<30)      /* Enable receive circuits */

typedef struct tWiFiRXdesc {
  U32 rx_data;
  U32 rx_status0;
  U32 rx_status1;
  U32 pad;
} __attribute__((aligned(8))) WiFiRXdesc;

#define WIFIRX_USED_MASK         (1UL<<31)
#define WIFIRX_WRAP              (1UL<<30)
#define WIFIRX_IE                (1UL<<29)
#define WIFIRX_OFFSET_MASK       0xff0000
#define WIFIRX_OFFSET_SHIFT      16
#define WIFIRX_LENGTH_MASK       0xfff
#define WIFIRX_LENGTH_SHIFT      0
#define WIFIRX_LENGTH_MAX        0xfff

#define WIFIRX_PKTTYPE_MASK       0xff00
#define WIFIRX_PKTTYPE_SHIFT      8

#define WIFIRX_VAP_INDX_MASK     0xf
#define WIFIRX_VAP_SHIFT      0

#define WIFI_PKTTYPE_OWN             0x01
#define WIFI_PKTTYPE_MC              0x02
#define WIFI_PKTTYPE_IPV4            0x04
#define WIFI_PKTTYPE_IPV6            0x08
#define WIFI_PKTTYPE_VLAN	     0x10
#define WIFI_PKTTYPE_PPPOE	     0x20



#define WIFITX_WRAP 	(1UL<<28)
#define WIFITX_OWN	(1UL<<15)

#define RX_STA_WIFI_EXPT_SHIFT 8
#define RX_STA_WIFI_EXPT_MASK 0x100

void M_wifi_rx_entry(void);
BOOL M_wifi_init_rx(PModuleDesc pModule );
#elif defined(COMCERTO_2000)
void wifi_tx_generate_csum(struct tMetadata *mtd);
void wifi_rx_validate_csum(struct tMetadata *mtd);
#endif
#endif /* CFG_WIFI_OFFLOAD */

#endif /* _MODULE_WIFI_H_ */
