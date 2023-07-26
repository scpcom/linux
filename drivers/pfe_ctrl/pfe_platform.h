/*
 *  drivers/pfe_ctrl/pfe_platform.h
 *  Based on:
 *  arch/arm/mach-comcerto/include/mach/comcerto-2000.h
 *
 *  Copyright (C) 2008 Mindspeed Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LS1024A_PFE_H__
#define __LS1024A_PFE_H__

#define CONFIG_COMCERTO_GEMAC		1

#define CONFIG_COMCERTO_USE_MII		1
#define CONFIG_COMCERTO_USE_RMII		2
#define CONFIG_COMCERTO_USE_GMII		4
#define CONFIG_COMCERTO_USE_RGMII	8
#define CONFIG_COMCERTO_USE_SGMII	0x10

#define GEMAC_SW_CONF			(1 << 8) | (1 << 11)	// GEMAC configured by SW
#define GEMAC_PHY_CONF		0			// GEMAC configured by phy lines (not for MII/GMII)
#define GEMAC_SW_FULL_DUPLEX	(1 << 9)
#define GEMAC_SW_SPEED_10M	(0 << 12)
#define GEMAC_SW_SPEED_100M	(1 << 12)
#define GEMAC_SW_SPEED_1G		(2 << 12)

#define GEMAC_NO_PHY			(1 << 0)		// set if no phy connected to MAC (ex ethernet switch). In this case use MAC fixed configuration
#define GEMAC_PHY_RGMII_ADD_DELAY	(1 << 1)

/* gemac to interface name assignment */
#define GEM0_ITF_NAME "eth0"
#define GEM1_ITF_NAME "eth1"
//#define GEM2_ITF_NAME "eth3"

#define GEM0_MAC { 0x00, 0xED, 0xCD, 0xEF, 0xAA, 0xCC }
#define GEM1_MAC { 0x00, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E }
//#define GEM2_MAC { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 }

struct comcerto_eth_platform_data {
	/* device specific information */
	u32 device_flags;
	char name[16];


	/* board specific information */
	u32 mii_config;
	u32 gemac_mode;
	u32 phy_flags;
	u32 gem_id;
	u32 bus_id;
	u32 phy_id;
	u8 *mac_addr;
};

struct comcerto_mdio_platform_data {
	int enabled;
	int irq[32];
	u32 phy_mask;
	int mdc_div;
};

struct comcerto_pfe_platform_data
{
	struct comcerto_eth_platform_data comcerto_eth_pdata[3];
	struct comcerto_mdio_platform_data comcerto_mdio_pdata[3];
};

/* Number of gemacs supported in comcerto 2000 */
#define NUM_GEMAC_SUPPORT	2

#endif
