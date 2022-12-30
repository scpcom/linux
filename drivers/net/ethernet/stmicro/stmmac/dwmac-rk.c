// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * DOC: dwmac-rk.c - Rockchip RK3288 DWMAC specific glue layer
 *
 * Copyright (C) 2014 Chen-Zhi (Roger Chen)
 *
 * Chen-Zhi (Roger Chen)  <roger.chen@rock-chips.com>
 */

#include <linux/stmmac.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/phy.h>
#include <linux/phy/phy.h>
#include <linux/of_net.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include <linux/soc/rockchip/rk_vendor_storage.h>
#include "stmmac_platform.h"
#include "dwmac-rk-tool.h"

#define MAX_ETH		2

struct rk_priv_data;
struct rk_gmac_ops {
	void (*set_to_rgmii)(struct rk_priv_data *bsp_priv,
			     int tx_delay, int rx_delay);
	void (*set_to_rmii)(struct rk_priv_data *bsp_priv);
	void (*set_to_sgmii)(struct rk_priv_data *bsp_priv);
	void (*set_to_qsgmii)(struct rk_priv_data *bsp_priv);
	void (*set_rgmii_speed)(struct rk_priv_data *bsp_priv, int speed);
	void (*set_rmii_speed)(struct rk_priv_data *bsp_priv, int speed);
	void (*set_clock_selection)(struct rk_priv_data *bsp_priv, bool input,
				    bool enable);
	void (*integrated_phy_power)(struct rk_priv_data *bsp_priv, bool up);
};

struct rk_priv_data {
	struct platform_device *pdev;
	phy_interface_t phy_iface;
	int bus_id;
	struct regulator *regulator;
	bool suspended;
	const struct rk_gmac_ops *ops;

	bool clk_enabled;
	bool clock_input;
	bool integrated_phy;

	struct clk *clk_mac;
	struct clk *gmac_clkin;
	struct clk *mac_clk_rx;
	struct clk *mac_clk_tx;
	struct clk *clk_mac_ref;
	struct clk *clk_mac_refout;
	struct clk *clk_mac_speed;
	struct clk *aclk_mac;
	struct clk *pclk_mac;
	struct clk *clk_phy;
	struct clk *pclk_xpcs;

	struct reset_control *phy_reset;

	int tx_delay;
	int rx_delay;

	struct regmap *grf;
	struct regmap *php_grf;
	struct regmap *xpcs;

	unsigned char otp_data[4];
	int hk_mac_rule;
};

/* XPCS */
#define XPCS_APB_INCREMENT		(0x4)
#define XPCS_APB_MASK			GENMASK_ULL(20, 0)

#define SR_MII_BASE			(0x1F0000)
#define SR_MII1_BASE			(0x1A0000)

#define VR_MII_DIG_CTRL1		(0x8000)
#define VR_MII_AN_CTRL			(0x8001)
#define VR_MII_AN_INTR_STS		(0x8002)
#define VR_MII_LINK_TIMER_CTRL		(0x800A)

#define SR_MII_CTRL_AN_ENABLE		\
	(BMCR_ANENABLE | BMCR_ANRESTART | BMCR_FULLDPLX | BMCR_SPEED1000)
#define MII_MAC_AUTO_SW			(0x0200)
#define PCS_MODE_OFFSET			(0x1)
#define MII_AN_INTR_EN			(0x1)
#define PCS_SGMII_MODE			(0x2 << PCS_MODE_OFFSET)
#define PCS_QSGMII_MODE			(0X3 << PCS_MODE_OFFSET)
#define VR_MII_CTRL_SGMII_AN_EN		(PCS_SGMII_MODE | MII_AN_INTR_EN)
#define VR_MII_CTRL_QSGMII_AN_EN	(PCS_QSGMII_MODE | MII_AN_INTR_EN)

#define SR_MII_OFFSET(_x) ({		\
	typeof(_x) (x) = (_x); \
	(((x) == 0) ? SR_MII_BASE : (SR_MII1_BASE + ((x) - 1) * 0x10000)); \
}) \

static int xpcs_read(void *priv, int reg)
{
	struct rk_priv_data *bsp_priv = (struct rk_priv_data *)priv;
	int ret, val;

	ret = regmap_read(bsp_priv->xpcs,
			  (u32)(reg * XPCS_APB_INCREMENT) & XPCS_APB_MASK,
			  &val);
	if (ret)
		return ret;

	return val;
}

static int xpcs_write(void *priv, int reg, u16 value)
{
	struct rk_priv_data *bsp_priv = (struct rk_priv_data *)priv;

	return regmap_write(bsp_priv->xpcs,
			    (reg * XPCS_APB_INCREMENT) & XPCS_APB_MASK, value);
}

static int xpcs_poll_reset(struct rk_priv_data *bsp_priv, int dev)
{
	/* Poll until the reset bit clears (50ms per retry == 0.6 sec) */
	unsigned int retries = 12;
	int ret;

	do {
		msleep(50);
		ret = xpcs_read(bsp_priv, SR_MII_OFFSET(dev) + MDIO_CTRL1);
		if (ret < 0)
			return ret;
	} while (ret & MDIO_CTRL1_RESET && --retries);

	return (ret & MDIO_CTRL1_RESET) ? -ETIMEDOUT : 0;
}

static int xpcs_soft_reset(struct rk_priv_data *bsp_priv, int dev)
{
	int ret;

	ret = xpcs_write(bsp_priv, SR_MII_OFFSET(dev) + MDIO_CTRL1,
			 MDIO_CTRL1_RESET);
	if (ret < 0)
		return ret;

	return xpcs_poll_reset(bsp_priv, dev);
}

static int xpcs_setup(struct rk_priv_data *bsp_priv, int mode)
{
	int ret, i, id = bsp_priv->bus_id;
	u32 val;

	if (mode == PHY_INTERFACE_MODE_QSGMII && id > 0)
		return 0;

	ret = xpcs_soft_reset(bsp_priv, id);
	if (ret) {
		dev_err(&bsp_priv->pdev->dev, "xpcs_soft_reset fail %d\n", ret);
		return ret;
	}

	xpcs_write(bsp_priv, SR_MII_OFFSET(0) + VR_MII_AN_INTR_STS, 0x0);
	xpcs_write(bsp_priv, SR_MII_OFFSET(0) + VR_MII_LINK_TIMER_CTRL, 0x1);

	if (mode == PHY_INTERFACE_MODE_SGMII)
		xpcs_write(bsp_priv, SR_MII_OFFSET(0) + VR_MII_AN_CTRL,
			   VR_MII_CTRL_SGMII_AN_EN);
	else
		xpcs_write(bsp_priv, SR_MII_OFFSET(0) + VR_MII_AN_CTRL,
			   VR_MII_CTRL_QSGMII_AN_EN);

	if (mode == PHY_INTERFACE_MODE_QSGMII) {
		for (i = 0; i < 4; i++) {
			val = xpcs_read(bsp_priv,
					SR_MII_OFFSET(i) + VR_MII_DIG_CTRL1);
			xpcs_write(bsp_priv,
				   SR_MII_OFFSET(i) + VR_MII_DIG_CTRL1,
				   val | MII_MAC_AUTO_SW);
			xpcs_write(bsp_priv, SR_MII_OFFSET(i) + MII_BMCR,
				   SR_MII_CTRL_AN_ENABLE);
		}
	} else {
		val = xpcs_read(bsp_priv, SR_MII_OFFSET(id) + VR_MII_DIG_CTRL1);
		xpcs_write(bsp_priv, SR_MII_OFFSET(id) + VR_MII_DIG_CTRL1,
			   val | MII_MAC_AUTO_SW);
		xpcs_write(bsp_priv, SR_MII_OFFSET(id) + MII_BMCR,
			   SR_MII_CTRL_AN_ENABLE);
	}

	return ret;
}

#define HIWORD_UPDATE(val, mask, shift) \
		((val) << (shift) | (mask) << ((shift) + 16))

#define GRF_BIT(nr)	(BIT(nr) | BIT(nr+16))
#define GRF_CLR_BIT(nr)	(BIT(nr+16))

#define DELAY_ENABLE(soc, tx, rx) \
	((((tx) >= 0) ? soc##_GMAC_TXCLK_DLY_ENABLE : soc##_GMAC_TXCLK_DLY_DISABLE) | \
	 (((rx) >= 0) ? soc##_GMAC_RXCLK_DLY_ENABLE : soc##_GMAC_RXCLK_DLY_DISABLE))

#define DELAY_ENABLE_BY_ID(soc, tx, rx, id) \
	((((tx) >= 0) ? soc##_GMAC_TXCLK_DLY_ENABLE(id) : soc##_GMAC_TXCLK_DLY_DISABLE(id)) | \
	 (((rx) >= 0) ? soc##_GMAC_RXCLK_DLY_ENABLE(id) : soc##_GMAC_RXCLK_DLY_DISABLE(id)))

#define DELAY_VALUE(soc, tx, rx) \
	((((tx) >= 0) ? soc##_GMAC_CLK_TX_DL_CFG(tx) : 0) | \
	 (((rx) >= 0) ? soc##_GMAC_CLK_RX_DL_CFG(rx) : 0))

/* Integrated EPHY */

#define RK_GRF_MACPHY_CON0		0xb00
#define RK_GRF_MACPHY_CON1		0xb04
#define RK_GRF_MACPHY_CON2		0xb08
#define RK_GRF_MACPHY_CON3		0xb0c

#define RK_MACPHY_ENABLE		GRF_BIT(0)
#define RK_MACPHY_DISABLE		GRF_CLR_BIT(0)
#define RK_MACPHY_CFG_CLK_50M		GRF_BIT(14)
#define RK_GMAC2PHY_RMII_MODE		(GRF_BIT(6) | GRF_CLR_BIT(7))
#define RK_GRF_CON2_MACPHY_ID		HIWORD_UPDATE(0x1234, 0xffff, 0)
#define RK_GRF_CON3_MACPHY_ID		HIWORD_UPDATE(0x35, 0x3f, 0)

static void rk_gmac_integrated_ephy_powerup(struct rk_priv_data *priv)
{
	regmap_write(priv->grf, RK_GRF_MACPHY_CON0, RK_MACPHY_CFG_CLK_50M);
	regmap_write(priv->grf, RK_GRF_MACPHY_CON0, RK_GMAC2PHY_RMII_MODE);

	regmap_write(priv->grf, RK_GRF_MACPHY_CON2, RK_GRF_CON2_MACPHY_ID);
	regmap_write(priv->grf, RK_GRF_MACPHY_CON3, RK_GRF_CON3_MACPHY_ID);

	if (priv->phy_reset) {
		/* PHY needs to be disabled before trying to reset it */
		regmap_write(priv->grf, RK_GRF_MACPHY_CON0, RK_MACPHY_DISABLE);
		if (priv->phy_reset)
			reset_control_assert(priv->phy_reset);
		usleep_range(10, 20);
		if (priv->phy_reset)
			reset_control_deassert(priv->phy_reset);
		usleep_range(10, 20);
		regmap_write(priv->grf, RK_GRF_MACPHY_CON0, RK_MACPHY_ENABLE);
		msleep(30);
	}
}

static void rk_gmac_integrated_ephy_powerdown(struct rk_priv_data *priv)
{
	regmap_write(priv->grf, RK_GRF_MACPHY_CON0, RK_MACPHY_DISABLE);
	if (priv->phy_reset)
		reset_control_assert(priv->phy_reset);
}

#define PX30_GRF_GMAC_CON1		0x0904

/* PX30_GRF_GMAC_CON1 */
#define PX30_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | \
					 GRF_BIT(6))
#define PX30_GMAC_SPEED_10M		GRF_CLR_BIT(2)
#define PX30_GMAC_SPEED_100M		GRF_BIT(2)

static void px30_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, PX30_GRF_GMAC_CON1,
		     PX30_GMAC_PHY_INTF_SEL_RMII);
}

static void px30_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	int ret;

	if (IS_ERR(bsp_priv->clk_mac_speed)) {
		dev_err(dev, "%s: Missing clk_mac_speed clock\n", __func__);
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, PX30_GRF_GMAC_CON1,
			     PX30_GMAC_SPEED_10M);

		ret = clk_set_rate(bsp_priv->clk_mac_speed, 2500000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 2500000 failed: %d\n",
				__func__, ret);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, PX30_GRF_GMAC_CON1,
			     PX30_GMAC_SPEED_100M);

		ret = clk_set_rate(bsp_priv->clk_mac_speed, 25000000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 25000000 failed: %d\n",
				__func__, ret);

	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops px30_ops = {
	.set_to_rmii = px30_set_to_rmii,
	.set_rmii_speed = px30_set_rmii_speed,
};

#define RK1808_GRF_GMAC_CON0		0X0900
#define RK1808_GRF_GMAC_CON1		0X0904

/* RK1808_GRF_GMAC_CON0 */
#define RK1808_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RK1808_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

/* RK1808_GRF_GMAC_CON1 */
#define RK1808_GMAC_PHY_INTF_SEL_RGMII	\
		(GRF_BIT(4) | GRF_CLR_BIT(5) | GRF_CLR_BIT(6))
#define RK1808_GMAC_PHY_INTF_SEL_RMII	\
		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | GRF_BIT(6))
#define RK1808_GMAC_FLOW_CTRL		GRF_BIT(3)
#define RK1808_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(3)
#define RK1808_GMAC_SPEED_10M		GRF_CLR_BIT(2)
#define RK1808_GMAC_SPEED_100M		GRF_BIT(2)
#define RK1808_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(1)
#define RK1808_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(1)
#define RK1808_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(0)
#define RK1808_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(0)

static void rk1808_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK1808_GRF_GMAC_CON1,
		     RK1808_GMAC_PHY_INTF_SEL_RGMII |
		     DELAY_ENABLE(RK1808, tx_delay, rx_delay));

	regmap_write(bsp_priv->grf, RK1808_GRF_GMAC_CON0,
		     DELAY_VALUE(RK1808, tx_delay, rx_delay));
}

static void rk1808_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK1808_GRF_GMAC_CON1,
		     RK1808_GMAC_PHY_INTF_SEL_RMII);
}

static void rk1808_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	int ret;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10) {
		ret = clk_set_rate(bsp_priv->clk_mac_speed, 2500000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 2500000 failed: %d\n",
				__func__, ret);
	} else if (speed == 100) {
		ret = clk_set_rate(bsp_priv->clk_mac_speed, 25000000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 25000000 failed: %d\n",
				__func__, ret);
	} else if (speed == 1000) {
		ret = clk_set_rate(bsp_priv->clk_mac_speed, 125000000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 125000000 failed: %d\n",
				__func__, ret);
	} else {
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
	}
}

static void rk1808_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	int ret;

	if (IS_ERR(bsp_priv->clk_mac_speed)) {
		dev_err(dev, "%s: Missing clk_mac_speed clock\n", __func__);
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RK1808_GRF_GMAC_CON1,
			     RK1808_GMAC_SPEED_10M);

		ret = clk_set_rate(bsp_priv->clk_mac_speed, 2500000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 2500000 failed: %d\n",
				__func__, ret);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RK1808_GRF_GMAC_CON1,
			     RK1808_GMAC_SPEED_100M);

		ret = clk_set_rate(bsp_priv->clk_mac_speed, 25000000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 25000000 failed: %d\n",
				__func__, ret);

	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rk1808_ops = {
	.set_to_rgmii = rk1808_set_to_rgmii,
	.set_to_rmii = rk1808_set_to_rmii,
	.set_rgmii_speed = rk1808_set_rgmii_speed,
	.set_rmii_speed = rk1808_set_rmii_speed,
};

#define RK3128_GRF_MAC_CON0	0x0168
#define RK3128_GRF_MAC_CON1	0x016c

/* RK3128_GRF_MAC_CON0 */
#define RK3128_GMAC_TXCLK_DLY_ENABLE   GRF_BIT(14)
#define RK3128_GMAC_TXCLK_DLY_DISABLE  GRF_CLR_BIT(14)
#define RK3128_GMAC_RXCLK_DLY_ENABLE   GRF_BIT(15)
#define RK3128_GMAC_RXCLK_DLY_DISABLE  GRF_CLR_BIT(15)
#define RK3128_GMAC_CLK_RX_DL_CFG(val) HIWORD_UPDATE(val, 0x7F, 7)
#define RK3128_GMAC_CLK_TX_DL_CFG(val) HIWORD_UPDATE(val, 0x7F, 0)

/* RK3128_GRF_MAC_CON1 */
#define RK3128_GMAC_PHY_INTF_SEL_RGMII	\
		(GRF_BIT(6) | GRF_CLR_BIT(7) | GRF_CLR_BIT(8))
#define RK3128_GMAC_PHY_INTF_SEL_RMII	\
		(GRF_CLR_BIT(6) | GRF_CLR_BIT(7) | GRF_BIT(8))
#define RK3128_GMAC_FLOW_CTRL          GRF_BIT(9)
#define RK3128_GMAC_FLOW_CTRL_CLR      GRF_CLR_BIT(9)
#define RK3128_GMAC_SPEED_10M          GRF_CLR_BIT(10)
#define RK3128_GMAC_SPEED_100M         GRF_BIT(10)
#define RK3128_GMAC_RMII_CLK_25M       GRF_BIT(11)
#define RK3128_GMAC_RMII_CLK_2_5M      GRF_CLR_BIT(11)
#define RK3128_GMAC_CLK_125M           (GRF_CLR_BIT(12) | GRF_CLR_BIT(13))
#define RK3128_GMAC_CLK_25M            (GRF_BIT(12) | GRF_BIT(13))
#define RK3128_GMAC_CLK_2_5M           (GRF_CLR_BIT(12) | GRF_BIT(13))
#define RK3128_GMAC_RMII_MODE          GRF_BIT(14)
#define RK3128_GMAC_RMII_MODE_CLR      GRF_CLR_BIT(14)

static void rk3128_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON1,
		     RK3128_GMAC_PHY_INTF_SEL_RGMII |
		     RK3128_GMAC_RMII_MODE_CLR);
	regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON0,
		     DELAY_ENABLE(RK3128, tx_delay, rx_delay) |
		     DELAY_VALUE(RK3128, tx_delay, rx_delay));
}

static void rk3128_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON1,
		     RK3128_GMAC_PHY_INTF_SEL_RMII | RK3128_GMAC_RMII_MODE);
}

static void rk3128_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON1,
			     RK3128_GMAC_CLK_2_5M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON1,
			     RK3128_GMAC_CLK_25M);
	else if (speed == 1000)
		regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON1,
			     RK3128_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3128_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON1,
			     RK3128_GMAC_RMII_CLK_2_5M |
			     RK3128_GMAC_SPEED_10M);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RK3128_GRF_MAC_CON1,
			     RK3128_GMAC_RMII_CLK_25M |
			     RK3128_GMAC_SPEED_100M);
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rk3128_ops = {
	.set_to_rgmii = rk3128_set_to_rgmii,
	.set_to_rmii = rk3128_set_to_rmii,
	.set_rgmii_speed = rk3128_set_rgmii_speed,
	.set_rmii_speed = rk3128_set_rmii_speed,
};

#define RK3228_GRF_MAC_CON0	0x0900
#define RK3228_GRF_MAC_CON1	0x0904

#define RK3228_GRF_CON_MUX	0x50

/* RK3228_GRF_MAC_CON0 */
#define RK3228_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 7)
#define RK3228_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

/* RK3228_GRF_MAC_CON1 */
#define RK3228_GMAC_PHY_INTF_SEL_RGMII	\
		(GRF_BIT(4) | GRF_CLR_BIT(5) | GRF_CLR_BIT(6))
#define RK3228_GMAC_PHY_INTF_SEL_RMII	\
		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | GRF_BIT(6))
#define RK3228_GMAC_FLOW_CTRL		GRF_BIT(3)
#define RK3228_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(3)
#define RK3228_GMAC_SPEED_10M		GRF_CLR_BIT(2)
#define RK3228_GMAC_SPEED_100M		GRF_BIT(2)
#define RK3228_GMAC_RMII_CLK_25M	GRF_BIT(7)
#define RK3228_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(7)
#define RK3228_GMAC_CLK_125M		(GRF_CLR_BIT(8) | GRF_CLR_BIT(9))
#define RK3228_GMAC_CLK_25M		(GRF_BIT(8) | GRF_BIT(9))
#define RK3228_GMAC_CLK_2_5M		(GRF_CLR_BIT(8) | GRF_BIT(9))
#define RK3228_GMAC_RMII_MODE		GRF_BIT(10)
#define RK3228_GMAC_RMII_MODE_CLR	GRF_CLR_BIT(10)
#define RK3228_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(0)
#define RK3228_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(0)
#define RK3228_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(1)
#define RK3228_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(1)

/* RK3228_GRF_COM_MUX */
#define RK3228_GRF_CON_MUX_GMAC_INTEGRATED_PHY	GRF_BIT(15)

static void rk3228_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1,
		     RK3228_GMAC_PHY_INTF_SEL_RGMII |
		     RK3228_GMAC_RMII_MODE_CLR |
		     DELAY_ENABLE(RK3228, tx_delay, rx_delay));

	regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON0,
		     DELAY_VALUE(RK3128, tx_delay, rx_delay));
}

static void rk3228_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1,
		     RK3228_GMAC_PHY_INTF_SEL_RMII |
		     RK3228_GMAC_RMII_MODE);

	/* set MAC to RMII mode */
	regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1, GRF_BIT(11));
}

static void rk3228_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1,
			     RK3228_GMAC_CLK_2_5M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1,
			     RK3228_GMAC_CLK_25M);
	else if (speed == 1000)
		regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1,
			     RK3228_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3228_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1,
			     RK3228_GMAC_RMII_CLK_2_5M |
			     RK3228_GMAC_SPEED_10M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3228_GRF_MAC_CON1,
			     RK3228_GMAC_RMII_CLK_25M |
			     RK3228_GMAC_SPEED_100M);
	else
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
}

static void rk3228_integrated_phy_power(struct rk_priv_data *priv, bool up)
{
	if (up) {
		regmap_write(priv->grf, RK3228_GRF_CON_MUX,
			     RK3228_GRF_CON_MUX_GMAC_INTEGRATED_PHY);

		rk_gmac_integrated_ephy_powerup(priv);
	} else {
		rk_gmac_integrated_ephy_powerdown(priv);
	}
}

static const struct rk_gmac_ops rk3228_ops = {
	.set_to_rgmii = rk3228_set_to_rgmii,
	.set_to_rmii = rk3228_set_to_rmii,
	.set_rgmii_speed = rk3228_set_rgmii_speed,
	.set_rmii_speed = rk3228_set_rmii_speed,
	.integrated_phy_power =  rk3228_integrated_phy_power,
};

#define RK3288_GRF_SOC_CON1	0x0248
#define RK3288_GRF_SOC_CON3	0x0250

/*RK3288_GRF_SOC_CON1*/
#define RK3288_GMAC_PHY_INTF_SEL_RGMII	(GRF_BIT(6) | GRF_CLR_BIT(7) | \
					 GRF_CLR_BIT(8))
#define RK3288_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(6) | GRF_CLR_BIT(7) | \
					 GRF_BIT(8))
#define RK3288_GMAC_FLOW_CTRL		GRF_BIT(9)
#define RK3288_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(9)
#define RK3288_GMAC_SPEED_10M		GRF_CLR_BIT(10)
#define RK3288_GMAC_SPEED_100M		GRF_BIT(10)
#define RK3288_GMAC_RMII_CLK_25M	GRF_BIT(11)
#define RK3288_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(11)
#define RK3288_GMAC_CLK_125M		(GRF_CLR_BIT(12) | GRF_CLR_BIT(13))
#define RK3288_GMAC_CLK_25M		(GRF_BIT(12) | GRF_BIT(13))
#define RK3288_GMAC_CLK_2_5M		(GRF_CLR_BIT(12) | GRF_BIT(13))
#define RK3288_GMAC_RMII_MODE		GRF_BIT(14)
#define RK3288_GMAC_RMII_MODE_CLR	GRF_CLR_BIT(14)

/*RK3288_GRF_SOC_CON3*/
#define RK3288_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(14)
#define RK3288_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(14)
#define RK3288_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(15)
#define RK3288_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(15)
#define RK3288_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 7)
#define RK3288_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

static void rk3288_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON1,
		     RK3288_GMAC_PHY_INTF_SEL_RGMII |
		     RK3288_GMAC_RMII_MODE_CLR);
	regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON3,
		     DELAY_ENABLE(RK3288, tx_delay, rx_delay) |
		     DELAY_VALUE(RK3288, tx_delay, rx_delay));
}

static void rk3288_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON1,
		     RK3288_GMAC_PHY_INTF_SEL_RMII | RK3288_GMAC_RMII_MODE);
}

static void rk3288_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON1,
			     RK3288_GMAC_CLK_2_5M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON1,
			     RK3288_GMAC_CLK_25M);
	else if (speed == 1000)
		regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON1,
			     RK3288_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3288_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON1,
			     RK3288_GMAC_RMII_CLK_2_5M |
			     RK3288_GMAC_SPEED_10M);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RK3288_GRF_SOC_CON1,
			     RK3288_GMAC_RMII_CLK_25M |
			     RK3288_GMAC_SPEED_100M);
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rk3288_ops = {
	.set_to_rgmii = rk3288_set_to_rgmii,
	.set_to_rmii = rk3288_set_to_rmii,
	.set_rgmii_speed = rk3288_set_rgmii_speed,
	.set_rmii_speed = rk3288_set_rmii_speed,
};

#define RK3308_GRF_MAC_CON0		0x04a0

/* Rk3308_GRF_MAC_CON1 */
#define RK3308_MAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(2) | GRF_CLR_BIT(3) | \
					GRF_BIT(4))
#define RK3308_MAC_SPEED_10M		GRF_CLR_BIT(0)
#define Rk3308_MAC_SPEED_100M		GRF_BIT(0)

static void rk3308_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK3308_GRF_MAC_CON0,
		     RK3308_MAC_PHY_INTF_SEL_RMII);
}

static void rk3308_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	int ret;

	if (IS_ERR(bsp_priv->clk_mac_speed)) {
		dev_err(dev, "%s: Missing clk_mac_speed clock\n", __func__);
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RK3308_GRF_MAC_CON0,
			     RK3308_MAC_SPEED_10M);

		ret = clk_set_rate(bsp_priv->clk_mac_speed, 2500000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 2500000 failed: %d\n",
				__func__, ret);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RK3308_GRF_MAC_CON0,
			     Rk3308_MAC_SPEED_100M);

		ret = clk_set_rate(bsp_priv->clk_mac_speed, 25000000);
		if (ret)
			dev_err(dev, "%s: set clk_mac_speed rate 25000000 failed: %d\n",
				__func__, ret);

	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rk3308_ops = {
	.set_to_rmii = rk3308_set_to_rmii,
	.set_rmii_speed = rk3308_set_rmii_speed,
};

#define RK3328_GRF_MAC_CON0	0x0900
#define RK3328_GRF_MAC_CON1	0x0904
#define RK3328_GRF_MAC_CON2	0x0908
#define RK3328_GRF_MACPHY_CON1	0xb04

/* RK3328_GRF_MAC_CON0 */
#define RK3328_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 7)
#define RK3328_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

/* RK3328_GRF_MAC_CON1 */
#define RK3328_GMAC_PHY_INTF_SEL_RGMII	\
		(GRF_BIT(4) | GRF_CLR_BIT(5) | GRF_CLR_BIT(6))
#define RK3328_GMAC_PHY_INTF_SEL_RMII	\
		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | GRF_BIT(6))
#define RK3328_GMAC_FLOW_CTRL		GRF_BIT(3)
#define RK3328_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(3)
#define RK3328_GMAC_SPEED_10M		GRF_CLR_BIT(2)
#define RK3328_GMAC_SPEED_100M		GRF_BIT(2)
#define RK3328_GMAC_RMII_CLK_25M	GRF_BIT(7)
#define RK3328_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(7)
#define RK3328_GMAC_CLK_125M		(GRF_CLR_BIT(11) | GRF_CLR_BIT(12))
#define RK3328_GMAC_CLK_25M		(GRF_BIT(11) | GRF_BIT(12))
#define RK3328_GMAC_CLK_2_5M		(GRF_CLR_BIT(11) | GRF_BIT(12))
#define RK3328_GMAC_RMII_MODE		GRF_BIT(9)
#define RK3328_GMAC_RMII_MODE_CLR	GRF_CLR_BIT(9)
#define RK3328_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(0)
#define RK3328_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(0)
#define RK3328_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(1)
#define RK3328_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(0)

/* RK3328_GRF_MACPHY_CON1 */
#define RK3328_MACPHY_RMII_MODE		GRF_BIT(9)

static void rk3328_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RK3328_GRF_MAC_CON1,
		     RK3328_GMAC_PHY_INTF_SEL_RGMII |
		     RK3328_GMAC_RMII_MODE_CLR |
		     DELAY_ENABLE(RK3328, tx_delay, rx_delay));

	regmap_write(bsp_priv->grf, RK3328_GRF_MAC_CON0,
		     DELAY_VALUE(RK3328, tx_delay, rx_delay));
}

static void rk3328_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned int reg;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	reg = bsp_priv->integrated_phy ? RK3328_GRF_MAC_CON2 :
		  RK3328_GRF_MAC_CON1;

	regmap_write(bsp_priv->grf, reg,
		     RK3328_GMAC_PHY_INTF_SEL_RMII |
		     RK3328_GMAC_RMII_MODE);
}

static void rk3328_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3328_GRF_MAC_CON1,
			     RK3328_GMAC_CLK_2_5M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3328_GRF_MAC_CON1,
			     RK3328_GMAC_CLK_25M);
	else if (speed == 1000)
		regmap_write(bsp_priv->grf, RK3328_GRF_MAC_CON1,
			     RK3328_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3328_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned int reg;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	reg = bsp_priv->integrated_phy ? RK3328_GRF_MAC_CON2 :
		  RK3328_GRF_MAC_CON1;

	if (speed == 10)
		regmap_write(bsp_priv->grf, reg,
			     RK3328_GMAC_RMII_CLK_2_5M |
			     RK3328_GMAC_SPEED_10M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, reg,
			     RK3328_GMAC_RMII_CLK_25M |
			     RK3328_GMAC_SPEED_100M);
	else
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
}

static void rk3328_integrated_phy_power(struct rk_priv_data *priv, bool up)
{
	if (up) {
		regmap_write(priv->grf, RK3328_GRF_MACPHY_CON1,
			     RK3328_MACPHY_RMII_MODE);

		rk_gmac_integrated_ephy_powerup(priv);
	} else {
		rk_gmac_integrated_ephy_powerdown(priv);
	}
}

static const struct rk_gmac_ops rk3328_ops = {
	.set_to_rgmii = rk3328_set_to_rgmii,
	.set_to_rmii = rk3328_set_to_rmii,
	.set_rgmii_speed = rk3328_set_rgmii_speed,
	.set_rmii_speed = rk3328_set_rmii_speed,
	.integrated_phy_power =  rk3328_integrated_phy_power,
};

#define RK3366_GRF_SOC_CON6	0x0418
#define RK3366_GRF_SOC_CON7	0x041c

/* RK3366_GRF_SOC_CON6 */
#define RK3366_GMAC_PHY_INTF_SEL_RGMII	(GRF_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_CLR_BIT(11))
#define RK3366_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_BIT(11))
#define RK3366_GMAC_FLOW_CTRL		GRF_BIT(8)
#define RK3366_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(8)
#define RK3366_GMAC_SPEED_10M		GRF_CLR_BIT(7)
#define RK3366_GMAC_SPEED_100M		GRF_BIT(7)
#define RK3366_GMAC_RMII_CLK_25M	GRF_BIT(3)
#define RK3366_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(3)
#define RK3366_GMAC_CLK_125M		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5))
#define RK3366_GMAC_CLK_25M		(GRF_BIT(4) | GRF_BIT(5))
#define RK3366_GMAC_CLK_2_5M		(GRF_CLR_BIT(4) | GRF_BIT(5))
#define RK3366_GMAC_RMII_MODE		GRF_BIT(6)
#define RK3366_GMAC_RMII_MODE_CLR	GRF_CLR_BIT(6)

/* RK3366_GRF_SOC_CON7 */
#define RK3366_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(7)
#define RK3366_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(7)
#define RK3366_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(15)
#define RK3366_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(15)
#define RK3366_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RK3366_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

static void rk3366_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON6,
		     RK3366_GMAC_PHY_INTF_SEL_RGMII |
		     RK3366_GMAC_RMII_MODE_CLR);
	regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON7,
		     DELAY_ENABLE(RK3366, tx_delay, rx_delay) |
		     DELAY_VALUE(RK3366, tx_delay, rx_delay));
}

static void rk3366_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON6,
		     RK3366_GMAC_PHY_INTF_SEL_RMII | RK3366_GMAC_RMII_MODE);
}

static void rk3366_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON6,
			     RK3366_GMAC_CLK_2_5M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON6,
			     RK3366_GMAC_CLK_25M);
	else if (speed == 1000)
		regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON6,
			     RK3366_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3366_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON6,
			     RK3366_GMAC_RMII_CLK_2_5M |
			     RK3366_GMAC_SPEED_10M);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RK3366_GRF_SOC_CON6,
			     RK3366_GMAC_RMII_CLK_25M |
			     RK3366_GMAC_SPEED_100M);
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rk3366_ops = {
	.set_to_rgmii = rk3366_set_to_rgmii,
	.set_to_rmii = rk3366_set_to_rmii,
	.set_rgmii_speed = rk3366_set_rgmii_speed,
	.set_rmii_speed = rk3366_set_rmii_speed,
};

#define RK3368_GRF_SOC_CON15	0x043c
#define RK3368_GRF_SOC_CON16	0x0440

/* RK3368_GRF_SOC_CON15 */
#define RK3368_GMAC_PHY_INTF_SEL_RGMII	(GRF_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_CLR_BIT(11))
#define RK3368_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_BIT(11))
#define RK3368_GMAC_FLOW_CTRL		GRF_BIT(8)
#define RK3368_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(8)
#define RK3368_GMAC_SPEED_10M		GRF_CLR_BIT(7)
#define RK3368_GMAC_SPEED_100M		GRF_BIT(7)
#define RK3368_GMAC_RMII_CLK_25M	GRF_BIT(3)
#define RK3368_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(3)
#define RK3368_GMAC_CLK_125M		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5))
#define RK3368_GMAC_CLK_25M		(GRF_BIT(4) | GRF_BIT(5))
#define RK3368_GMAC_CLK_2_5M		(GRF_CLR_BIT(4) | GRF_BIT(5))
#define RK3368_GMAC_RMII_MODE		GRF_BIT(6)
#define RK3368_GMAC_RMII_MODE_CLR	GRF_CLR_BIT(6)

/* RK3368_GRF_SOC_CON16 */
#define RK3368_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(7)
#define RK3368_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(7)
#define RK3368_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(15)
#define RK3368_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(15)
#define RK3368_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RK3368_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

static void rk3368_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON15,
		     RK3368_GMAC_PHY_INTF_SEL_RGMII |
		     RK3368_GMAC_RMII_MODE_CLR);
	regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON16,
		     DELAY_ENABLE(RK3368, tx_delay, rx_delay) |
		     DELAY_VALUE(RK3368, tx_delay, rx_delay));
}

static void rk3368_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON15,
		     RK3368_GMAC_PHY_INTF_SEL_RMII | RK3368_GMAC_RMII_MODE);
}

static void rk3368_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON15,
			     RK3368_GMAC_CLK_2_5M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON15,
			     RK3368_GMAC_CLK_25M);
	else if (speed == 1000)
		regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON15,
			     RK3368_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3368_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON15,
			     RK3368_GMAC_RMII_CLK_2_5M |
			     RK3368_GMAC_SPEED_10M);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RK3368_GRF_SOC_CON15,
			     RK3368_GMAC_RMII_CLK_25M |
			     RK3368_GMAC_SPEED_100M);
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rk3368_ops = {
	.set_to_rgmii = rk3368_set_to_rgmii,
	.set_to_rmii = rk3368_set_to_rmii,
	.set_rgmii_speed = rk3368_set_rgmii_speed,
	.set_rmii_speed = rk3368_set_rmii_speed,
};

#define RK3399_GRF_SOC_CON5	0xc214
#define RK3399_GRF_SOC_CON6	0xc218

/* RK3399_GRF_SOC_CON5 */
#define RK3399_GMAC_PHY_INTF_SEL_RGMII	(GRF_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_CLR_BIT(11))
#define RK3399_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(9) | GRF_CLR_BIT(10) | \
					 GRF_BIT(11))
#define RK3399_GMAC_FLOW_CTRL		GRF_BIT(8)
#define RK3399_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(8)
#define RK3399_GMAC_SPEED_10M		GRF_CLR_BIT(7)
#define RK3399_GMAC_SPEED_100M		GRF_BIT(7)
#define RK3399_GMAC_RMII_CLK_25M	GRF_BIT(3)
#define RK3399_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(3)
#define RK3399_GMAC_CLK_125M		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5))
#define RK3399_GMAC_CLK_25M		(GRF_BIT(4) | GRF_BIT(5))
#define RK3399_GMAC_CLK_2_5M		(GRF_CLR_BIT(4) | GRF_BIT(5))
#define RK3399_GMAC_RMII_MODE		GRF_BIT(6)
#define RK3399_GMAC_RMII_MODE_CLR	GRF_CLR_BIT(6)

/* RK3399_GRF_SOC_CON6 */
#define RK3399_GMAC_TXCLK_DLY_ENABLE	GRF_BIT(7)
#define RK3399_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(7)
#define RK3399_GMAC_RXCLK_DLY_ENABLE	GRF_BIT(15)
#define RK3399_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(15)
#define RK3399_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RK3399_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

static void rk3399_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON5,
		     RK3399_GMAC_PHY_INTF_SEL_RGMII |
		     RK3399_GMAC_RMII_MODE_CLR);
	regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON6,
		     DELAY_ENABLE(RK3399, tx_delay, rx_delay) |
		     DELAY_VALUE(RK3399, tx_delay, rx_delay));
}

static void rk3399_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON5,
		     RK3399_GMAC_PHY_INTF_SEL_RMII | RK3399_GMAC_RMII_MODE);
}

static void rk3399_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10)
		regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_CLK_2_5M);
	else if (speed == 100)
		regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_CLK_25M);
	else if (speed == 1000)
		regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_CLK_125M);
	else
		dev_err(dev, "unknown speed value for RGMII! speed=%d", speed);
}

static void rk3399_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_RMII_CLK_2_5M |
			     RK3399_GMAC_SPEED_10M);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RK3399_GRF_SOC_CON5,
			     RK3399_GMAC_RMII_CLK_25M |
			     RK3399_GMAC_SPEED_100M);
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rk3399_ops = {
	.set_to_rgmii = rk3399_set_to_rgmii,
	.set_to_rmii = rk3399_set_to_rmii,
	.set_rgmii_speed = rk3399_set_rgmii_speed,
	.set_rmii_speed = rk3399_set_rmii_speed,
};

#define RK3568_GRF_GMAC0_CON0		0X0380
#define RK3568_GRF_GMAC0_CON1		0X0384
#define RK3568_GRF_GMAC1_CON0		0X0388
#define RK3568_GRF_GMAC1_CON1		0X038c

/* RK3568_GRF_GMAC0_CON1 && RK3568_GRF_GMAC1_CON1 */
#define RK3568_GMAC_GMII_MODE			GRF_BIT(7)
#define RK3568_GMAC_PHY_INTF_SEL_RGMII	\
		(GRF_BIT(4) | GRF_CLR_BIT(5) | GRF_CLR_BIT(6))
#define RK3568_GMAC_PHY_INTF_SEL_RMII	\
		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | GRF_BIT(6))
#define RK3568_GMAC_FLOW_CTRL			GRF_BIT(3)
#define RK3568_GMAC_FLOW_CTRL_CLR		GRF_CLR_BIT(3)
#define RK3568_GMAC_RXCLK_DLY_ENABLE		GRF_BIT(1)
#define RK3568_GMAC_RXCLK_DLY_DISABLE		GRF_CLR_BIT(1)
#define RK3568_GMAC_TXCLK_DLY_ENABLE		GRF_BIT(0)
#define RK3568_GMAC_TXCLK_DLY_DISABLE		GRF_CLR_BIT(0)

/* RK3568_GRF_GMAC0_CON0 && RK3568_GRF_GMAC1_CON0 */
#define RK3568_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RK3568_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

#define RK3568_PIPE_GRF_XPCS_CON0	0X0040

#define RK3568_PIPE_GRF_XPCS_QGMII_MAC_SEL	GRF_BIT(0)
#define RK3568_PIPE_GRF_XPCS_SGMII_MAC_SEL	GRF_BIT(1)
#define RK3568_PIPE_GRF_XPCS_PHY_READY		GRF_BIT(2)

static void rk3568_set_to_sgmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;
	u32 offset_con1;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grfs property\n", __func__);
		return;
	}

	offset_con1 = bsp_priv->bus_id == 1 ? RK3568_GRF_GMAC1_CON1 :
					      RK3568_GRF_GMAC0_CON1;
	regmap_write(bsp_priv->grf, offset_con1, RK3568_GMAC_GMII_MODE);

	xpcs_setup(bsp_priv, PHY_INTERFACE_MODE_SGMII);
}

static void rk3568_set_to_qsgmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;
	u32 offset_con1;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grfs property\n", __func__);
		return;
	}

	offset_con1 = bsp_priv->bus_id == 1 ? RK3568_GRF_GMAC1_CON1 :
					      RK3568_GRF_GMAC0_CON1;
	regmap_write(bsp_priv->grf, offset_con1, RK3568_GMAC_GMII_MODE);

	xpcs_setup(bsp_priv, PHY_INTERFACE_MODE_QSGMII);
}

static void rk3568_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;
	u32 offset_con0, offset_con1;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	offset_con0 = (bsp_priv->bus_id == 1) ? RK3568_GRF_GMAC1_CON0 :
						RK3568_GRF_GMAC0_CON0;
	offset_con1 = (bsp_priv->bus_id == 1) ? RK3568_GRF_GMAC1_CON1 :
						RK3568_GRF_GMAC0_CON1;

	regmap_write(bsp_priv->grf, offset_con1,
		     RK3568_GMAC_PHY_INTF_SEL_RGMII |
		     DELAY_ENABLE(RK3568, tx_delay, rx_delay));

	regmap_write(bsp_priv->grf, offset_con0,
		     DELAY_VALUE(RK3568, tx_delay, rx_delay));
}

static void rk3568_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;
	u32 offset_con1;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	offset_con1 = (bsp_priv->bus_id == 1) ? RK3568_GRF_GMAC1_CON1 :
						RK3568_GRF_GMAC0_CON1;

	regmap_write(bsp_priv->grf, offset_con1, RK3568_GMAC_PHY_INTF_SEL_RMII);
}

static void rk3568_set_gmac_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned long rate;
	int ret;

	switch (speed) {
	case 10:
		rate = 2500000;
		break;
	case 100:
		rate = 25000000;
		break;
	case 1000:
		rate = 125000000;
		break;
	default:
		dev_err(dev, "unknown speed value for GMAC speed=%d", speed);
		return;
	}

	ret = clk_set_rate(bsp_priv->clk_mac_speed, rate);
	if (ret)
		dev_err(dev, "%s: set clk_mac_speed rate %ld failed %d\n",
			__func__, rate, ret);
}

static const struct rk_gmac_ops rk3568_ops = {
	.set_to_rgmii = rk3568_set_to_rgmii,
	.set_to_rmii = rk3568_set_to_rmii,
	.set_to_sgmii = rk3568_set_to_sgmii,
	.set_to_qsgmii = rk3568_set_to_qsgmii,
	.set_rgmii_speed = rk3568_set_gmac_speed,
	.set_rmii_speed = rk3568_set_gmac_speed,
};

/* sys_grf */
#define RK3588_GRF_GMAC_CON7			0X031c
#define RK3588_GRF_GMAC_CON8			0X0320
#define RK3588_GRF_GMAC_CON9			0X0324

#define RK3588_GMAC_RXCLK_DLY_ENABLE(id)	GRF_BIT(2 * (id) + 3)
#define RK3588_GMAC_RXCLK_DLY_DISABLE(id)	GRF_CLR_BIT(2 * (id) + 3)
#define RK3588_GMAC_TXCLK_DLY_ENABLE(id)	GRF_BIT(2 * (id) + 2)
#define RK3588_GMAC_TXCLK_DLY_DISABLE(id)	GRF_CLR_BIT(2 * (id) + 2)

#define RK3588_GMAC_CLK_RX_DL_CFG(val)		HIWORD_UPDATE(val, 0xFF, 8)
#define RK3588_GMAC_CLK_TX_DL_CFG(val)		HIWORD_UPDATE(val, 0xFF, 0)

/* php_grf */
#define RK3588_GRF_GMAC_CON0			0X0008
#define RK3588_GRF_CLK_CON1			0X0070

#define RK3588_GMAC_PHY_INTF_SEL_RGMII(id)	\
	(GRF_BIT(3 + (id) * 6) | GRF_CLR_BIT(4 + (id) * 6) | GRF_CLR_BIT(5 + (id) * 6))
#define RK3588_GMAC_PHY_INTF_SEL_RMII(id)	\
	(GRF_CLR_BIT(3 + (id) * 6) | GRF_CLR_BIT(4 + (id) * 6) | GRF_BIT(5 + (id) * 6))

#define RK3588_GMAC_CLK_RMII_MODE(id)		GRF_BIT(5 * (id))
#define RK3588_GMAC_CLK_RGMII_MODE(id)		GRF_CLR_BIT(5 * (id))

#define RK3588_GMAC_CLK_SELET_CRU(id)		GRF_BIT(5 * (id) + 4)
#define RK3588_GMAC_CLK_SELET_IO(id)		GRF_CLR_BIT(5 * (id) + 4)

#define RK3588_GMA_CLK_RMII_DIV2(id)		GRF_BIT(5 * (id) + 2)
#define RK3588_GMA_CLK_RMII_DIV20(id)		GRF_CLR_BIT(5 * (id) + 2)

#define RK3588_GMAC_CLK_RGMII_DIV1(id)		\
			(GRF_CLR_BIT(5 * (id) + 2) | GRF_CLR_BIT(5 * (id) + 3))
#define RK3588_GMAC_CLK_RGMII_DIV5(id)		\
			(GRF_BIT(5 * (id) + 2) | GRF_BIT(5 * (id) + 3))
#define RK3588_GMAC_CLK_RGMII_DIV50(id)		\
			(GRF_CLR_BIT(5 * (id) + 2) | GRF_BIT(5 * (id) + 3))

#define RK3588_GMAC_CLK_RMII_GATE(id)		GRF_BIT(5 * (id) + 1)
#define RK3588_GMAC_CLK_RMII_NOGATE(id)		GRF_CLR_BIT(5 * (id) + 1)

static void rk3588_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;
	u32 offset_con, id = bsp_priv->bus_id;

	if (IS_ERR(bsp_priv->grf) || IS_ERR(bsp_priv->php_grf)) {
		dev_err(dev, "Missing rockchip,grf or rockchip,php_grf property\n");
		return;
	}

	offset_con = bsp_priv->bus_id == 1 ? RK3588_GRF_GMAC_CON9 :
					     RK3588_GRF_GMAC_CON8;

	regmap_write(bsp_priv->php_grf, RK3588_GRF_GMAC_CON0,
		     RK3588_GMAC_PHY_INTF_SEL_RGMII(id));

	regmap_write(bsp_priv->php_grf, RK3588_GRF_CLK_CON1,
		     RK3588_GMAC_CLK_RGMII_MODE(id));

	regmap_write(bsp_priv->grf, RK3588_GRF_GMAC_CON7,
		     DELAY_ENABLE_BY_ID(RK3588, tx_delay, rx_delay, id));

	regmap_write(bsp_priv->grf, offset_con,
		     DELAY_VALUE(RK3588, tx_delay, rx_delay));
}

static void rk3588_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->php_grf)) {
		dev_err(dev, "%s: Missing rockchip,php_grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->php_grf, RK3588_GRF_GMAC_CON0,
		     RK3588_GMAC_PHY_INTF_SEL_RMII(bsp_priv->bus_id));

	regmap_write(bsp_priv->php_grf, RK3588_GRF_CLK_CON1,
		     RK3588_GMAC_CLK_RMII_MODE(bsp_priv->bus_id));
}

static void rk3588_set_gmac_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned int val = 0, id = bsp_priv->bus_id;

	switch (speed) {
	case 10:
		if (bsp_priv->phy_iface == PHY_INTERFACE_MODE_RMII)
			val = RK3588_GMA_CLK_RMII_DIV20(id);
		else
			val = RK3588_GMAC_CLK_RGMII_DIV50(id);
		break;
	case 100:
		if (bsp_priv->phy_iface == PHY_INTERFACE_MODE_RMII)
			val = RK3588_GMA_CLK_RMII_DIV2(id);
		else
			val = RK3588_GMAC_CLK_RGMII_DIV5(id);
		break;
	case 1000:
		if (bsp_priv->phy_iface != PHY_INTERFACE_MODE_RMII)
			val = RK3588_GMAC_CLK_RGMII_DIV1(id);
		else
			goto err;
		break;
	default:
		goto err;
	}

	regmap_write(bsp_priv->php_grf, RK3588_GRF_CLK_CON1, val);

	return;
err:
	dev_err(dev, "unknown speed value for GMAC speed=%d", speed);
}

static void rk3588_set_clock_selection(struct rk_priv_data *bsp_priv, bool input,
				       bool enable)
{
	unsigned int val = input ? RK3588_GMAC_CLK_SELET_IO(bsp_priv->bus_id) :
				   RK3588_GMAC_CLK_SELET_CRU(bsp_priv->bus_id);

	val |= enable ? RK3588_GMAC_CLK_RMII_NOGATE(bsp_priv->bus_id) :
			RK3588_GMAC_CLK_RMII_GATE(bsp_priv->bus_id);

	regmap_write(bsp_priv->php_grf, RK3588_GRF_CLK_CON1, val);
}

static const struct rk_gmac_ops rk3588_ops = {
	.set_to_rgmii = rk3588_set_to_rgmii,
	.set_to_rmii = rk3588_set_to_rmii,
	.set_rgmii_speed = rk3588_set_gmac_speed,
	.set_rmii_speed = rk3588_set_gmac_speed,
	.set_clock_selection = rk3588_set_clock_selection,
};

#define RV1106_VOGRF_GMAC_CLK_CON		0X60004

#define RV1106_VOGRF_MACPHY_RMII_MODE		GRF_BIT(0)
#define RV1106_VOGRF_GMAC_CLK_RMII_DIV2		GRF_BIT(2)
#define RV1106_VOGRF_GMAC_CLK_RMII_DIV20	GRF_CLR_BIT(2)

#define RV1106_VOGRF_MACPHY_CON0		0X60028

#define RV1106_VOGRF_MACPHY_SHUTDOWN		GRF_BIT(1)
#define RV1106_VOGRF_MACPHY_POWERUP		GRF_CLR_BIT(1)
#define RV1106_VOGRF_MACPHY_INTERNAL_RMII_SEL	GRF_BIT(6)
#define RV1106_VOGRF_MACPHY_24M_CLK_SEL		(GRF_BIT(8) | GRF_BIT(9))
#define RV1106_VOGRF_MACPHY_PHY_ID		GRF_BIT(11)

#define RV1106_VOGRF_MACPHY_CON1		0X6002C

#define RV1106_VOGRF_MACPHY_BGS			HIWORD_UPDATE(0x0, 0xf, 0)

static void rv1106_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RV1106_VOGRF_GMAC_CLK_CON,
		     RV1106_VOGRF_MACPHY_RMII_MODE);
}

static void rv1106_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned int val = 0;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10) {
		val = RV1106_VOGRF_GMAC_CLK_RMII_DIV20;
	} else if (speed == 100) {
		val = RV1106_VOGRF_GMAC_CLK_RMII_DIV2;
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
		return;
	}

	regmap_write(bsp_priv->grf, RV1106_VOGRF_GMAC_CLK_CON, val);
}

static void rv1106_integrated_sphy_power(struct rk_priv_data *priv, bool up)
{
	struct device *dev = &priv->pdev->dev;

	if (IS_ERR(priv->grf) || !priv->phy_reset) {
		dev_err(dev, "%s: Missing rockchip,grf or phy_reset property\n",
			__func__);
		return;
	}

	if (up) {
		unsigned int bgs = RV1106_VOGRF_MACPHY_BGS;

		reset_control_assert(priv->phy_reset);
		udelay(20);
		regmap_write(priv->grf, RV1106_VOGRF_MACPHY_CON0,
			     RV1106_VOGRF_MACPHY_POWERUP |
			     RV1106_VOGRF_MACPHY_INTERNAL_RMII_SEL |
			     RV1106_VOGRF_MACPHY_24M_CLK_SEL |
			     RV1106_VOGRF_MACPHY_PHY_ID);

		if (priv->otp_data[0] > 0)
			bgs = HIWORD_UPDATE(priv->otp_data[0], 0xf, 0);

		regmap_write(priv->grf, RV1106_VOGRF_MACPHY_CON1, bgs);
		usleep_range(10 * 1000, 12 * 1000);
		reset_control_deassert(priv->phy_reset);
		usleep_range(50 * 1000, 60 * 1000);
	} else {
		regmap_write(priv->grf, RV1106_VOGRF_MACPHY_CON0,
			     RV1106_VOGRF_MACPHY_SHUTDOWN);
	}
}

static const struct rk_gmac_ops rv1106_ops = {
	.set_to_rmii = rv1106_set_to_rmii,
	.set_rmii_speed = rv1106_set_rmii_speed,
	.integrated_phy_power = rv1106_integrated_sphy_power,
};

#define RV1108_GRF_GMAC_CON0		0X0900

/* RV1108_GRF_GMAC_CON0 */
#define RV1108_GMAC_PHY_INTF_SEL_RMII	(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | \
					GRF_BIT(6))
#define RV1108_GMAC_FLOW_CTRL		GRF_BIT(3)
#define RV1108_GMAC_FLOW_CTRL_CLR	GRF_CLR_BIT(3)
#define RV1108_GMAC_SPEED_10M		GRF_CLR_BIT(2)
#define RV1108_GMAC_SPEED_100M		GRF_BIT(2)
#define RV1108_GMAC_RMII_CLK_25M	GRF_BIT(7)
#define RV1108_GMAC_RMII_CLK_2_5M	GRF_CLR_BIT(7)

static void rv1108_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RV1108_GRF_GMAC_CON0,
		     RV1108_GMAC_PHY_INTF_SEL_RMII);
}

static void rv1108_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	if (speed == 10) {
		regmap_write(bsp_priv->grf, RV1108_GRF_GMAC_CON0,
			     RV1108_GMAC_RMII_CLK_2_5M |
			     RV1108_GMAC_SPEED_10M);
	} else if (speed == 100) {
		regmap_write(bsp_priv->grf, RV1108_GRF_GMAC_CON0,
			     RV1108_GMAC_RMII_CLK_25M |
			     RV1108_GMAC_SPEED_100M);
	} else {
		dev_err(dev, "unknown speed value for RMII! speed=%d", speed);
	}
}

static const struct rk_gmac_ops rv1108_ops = {
	.set_to_rmii = rv1108_set_to_rmii,
	.set_rmii_speed = rv1108_set_rmii_speed,
};

#define RV1126_GRF_GMAC_CON0		0X0070
#define RV1126_GRF_GMAC_CON1		0X0074
#define RV1126_GRF_GMAC_CON2		0X0078

/* RV1126_GRF_GMAC_CON0 */
#define RV1126_GMAC_PHY_INTF_SEL_RGMII	\
		(GRF_BIT(4) | GRF_CLR_BIT(5) | GRF_CLR_BIT(6))
#define RV1126_GMAC_PHY_INTF_SEL_RMII	\
		(GRF_CLR_BIT(4) | GRF_CLR_BIT(5) | GRF_BIT(6))
#define RV1126_GMAC_FLOW_CTRL			GRF_BIT(7)
#define RV1126_GMAC_FLOW_CTRL_CLR		GRF_CLR_BIT(7)
#define RV1126_M0_GMAC_RXCLK_DLY_ENABLE		GRF_BIT(1)
#define RV1126_M0_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(1)
#define RV1126_M0_GMAC_TXCLK_DLY_ENABLE		GRF_BIT(0)
#define RV1126_M0_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(0)
#define RV1126_M1_GMAC_RXCLK_DLY_ENABLE		GRF_BIT(3)
#define RV1126_M1_GMAC_RXCLK_DLY_DISABLE	GRF_CLR_BIT(3)
#define RV1126_M1_GMAC_TXCLK_DLY_ENABLE		GRF_BIT(2)
#define RV1126_M1_GMAC_TXCLK_DLY_DISABLE	GRF_CLR_BIT(2)

/* RV1126_GRF_GMAC_CON1 && RV1126_GRF_GMAC_CON2 */
#define RV1126_GMAC_CLK_RX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 8)
#define RV1126_GMAC_CLK_TX_DL_CFG(val)	HIWORD_UPDATE(val, 0x7F, 0)

static void rv1126_set_to_rgmii(struct rk_priv_data *bsp_priv,
				int tx_delay, int rx_delay)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "Missing rockchip,grf property\n");
		return;
	}

	regmap_write(bsp_priv->grf, RV1126_GRF_GMAC_CON0,
		     RV1126_GMAC_PHY_INTF_SEL_RGMII |
		     DELAY_ENABLE(RV1126_M0, tx_delay, rx_delay) |
		     DELAY_ENABLE(RV1126_M1, tx_delay, rx_delay));

	regmap_write(bsp_priv->grf, RV1126_GRF_GMAC_CON1,
		     DELAY_VALUE(RV1126, tx_delay, rx_delay));

	regmap_write(bsp_priv->grf, RV1126_GRF_GMAC_CON2,
		     DELAY_VALUE(RV1126, tx_delay, rx_delay));
}

static void rv1126_set_to_rmii(struct rk_priv_data *bsp_priv)
{
	struct device *dev = &bsp_priv->pdev->dev;

	if (IS_ERR(bsp_priv->grf)) {
		dev_err(dev, "%s: Missing rockchip,grf property\n", __func__);
		return;
	}

	regmap_write(bsp_priv->grf, RV1126_GRF_GMAC_CON0,
		     RV1126_GMAC_PHY_INTF_SEL_RMII);
}

static void rv1126_set_rgmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned long rate;
	int ret;

	switch (speed) {
	case 10:
		rate = 2500000;
		break;
	case 100:
		rate = 25000000;
		break;
	case 1000:
		rate = 125000000;
		break;
	default:
		dev_err(dev, "unknown speed value for RGMII speed=%d", speed);
		return;
	}

	ret = clk_set_rate(bsp_priv->clk_mac_speed, rate);
	if (ret)
		dev_err(dev, "%s: set clk_mac_speed rate %ld failed %d\n",
			__func__, rate, ret);
}

static void rv1126_set_rmii_speed(struct rk_priv_data *bsp_priv, int speed)
{
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned long rate;
	int ret;

	switch (speed) {
	case 10:
		rate = 2500000;
		break;
	case 100:
		rate = 25000000;
		break;
	default:
		dev_err(dev, "unknown speed value for RGMII speed=%d", speed);
		return;
	}

	ret = clk_set_rate(bsp_priv->clk_mac_speed, rate);
	if (ret)
		dev_err(dev, "%s: set clk_mac_speed rate %ld failed %d\n",
			__func__, rate, ret);
}

static const struct rk_gmac_ops rv1126_ops = {
	.set_to_rgmii = rv1126_set_to_rgmii,
	.set_to_rmii = rv1126_set_to_rmii,
	.set_rgmii_speed = rv1126_set_rgmii_speed,
	.set_rmii_speed = rv1126_set_rmii_speed,
};

static int rk_gmac_clk_init(struct plat_stmmacenet_data *plat)
{
	struct rk_priv_data *bsp_priv = plat->bsp_priv;
	struct device *dev = &bsp_priv->pdev->dev;
	int ret;

	bsp_priv->clk_enabled = false;

	bsp_priv->mac_clk_rx = devm_clk_get(dev, "mac_clk_rx");
	if (IS_ERR(bsp_priv->mac_clk_rx))
		dev_err(dev, "cannot get clock %s\n",
			"mac_clk_rx");

	bsp_priv->mac_clk_tx = devm_clk_get(dev, "mac_clk_tx");
	if (IS_ERR(bsp_priv->mac_clk_tx))
		dev_err(dev, "cannot get clock %s\n",
			"mac_clk_tx");

	bsp_priv->aclk_mac = devm_clk_get(dev, "aclk_mac");
	if (IS_ERR(bsp_priv->aclk_mac))
		dev_err(dev, "cannot get clock %s\n",
			"aclk_mac");

	bsp_priv->pclk_mac = devm_clk_get(dev, "pclk_mac");
	if (IS_ERR(bsp_priv->pclk_mac))
		dev_err(dev, "cannot get clock %s\n",
			"pclk_mac");

	bsp_priv->clk_mac = devm_clk_get(dev, "stmmaceth");
	if (IS_ERR(bsp_priv->clk_mac))
		dev_err(dev, "cannot get clock %s\n",
			"stmmaceth");

	if (bsp_priv->phy_iface == PHY_INTERFACE_MODE_RMII) {
		bsp_priv->clk_mac_ref = devm_clk_get(dev, "clk_mac_ref");
		if (IS_ERR(bsp_priv->clk_mac_ref))
			dev_err(dev, "cannot get clock %s\n",
				"clk_mac_ref");

		if (!bsp_priv->clock_input) {
			bsp_priv->clk_mac_refout =
				devm_clk_get(dev, "clk_mac_refout");
			if (IS_ERR(bsp_priv->clk_mac_refout))
				dev_err(dev, "cannot get clock %s\n",
					"clk_mac_refout");
		}
	} else if (bsp_priv->phy_iface == PHY_INTERFACE_MODE_SGMII ||
		   bsp_priv->phy_iface == PHY_INTERFACE_MODE_QSGMII) {
		bsp_priv->pclk_xpcs = devm_clk_get(dev, "pclk_xpcs");
		if (IS_ERR(bsp_priv->pclk_xpcs))
			dev_err(dev, "cannot get clock %s\n",
				"pclk_xpcs");
	}

	bsp_priv->clk_mac_speed = devm_clk_get(dev, "clk_mac_speed");
	if (IS_ERR(bsp_priv->clk_mac_speed))
		dev_err(dev, "cannot get clock %s\n", "clk_mac_speed");

	if (bsp_priv->clock_input) {
		dev_info(dev, "clock input from PHY\n");
	} else {
		if (bsp_priv->phy_iface == PHY_INTERFACE_MODE_RMII)
			clk_set_rate(bsp_priv->clk_mac, 50000000);
	}

	if (plat->phy_node) {
		bsp_priv->clk_phy = of_clk_get(plat->phy_node, 0);
		/* If it is not integrated_phy, clk_phy is optional */
		if (bsp_priv->integrated_phy) {
			if (IS_ERR(bsp_priv->clk_phy)) {
				ret = PTR_ERR(bsp_priv->clk_phy);
				dev_err(dev, "Cannot get PHY clock: %d\n", ret);
				return -EINVAL;
			}
			clk_set_rate(bsp_priv->clk_phy, 50000000);
		}
	}

	return 0;
}

static int gmac_clk_enable(struct rk_priv_data *bsp_priv, bool enable)
{
	int phy_iface = bsp_priv->phy_iface;

	if (enable) {
		if (!bsp_priv->clk_enabled) {
			if (phy_iface == PHY_INTERFACE_MODE_RMII) {
				if (!IS_ERR(bsp_priv->mac_clk_rx))
					clk_prepare_enable(
						bsp_priv->mac_clk_rx);

				if (!IS_ERR(bsp_priv->clk_mac_ref))
					clk_prepare_enable(
						bsp_priv->clk_mac_ref);

				if (!IS_ERR(bsp_priv->clk_mac_refout))
					clk_prepare_enable(
						bsp_priv->clk_mac_refout);
			}

			if (!IS_ERR(bsp_priv->clk_phy))
				clk_prepare_enable(bsp_priv->clk_phy);

			if (!IS_ERR(bsp_priv->aclk_mac))
				clk_prepare_enable(bsp_priv->aclk_mac);

			if (!IS_ERR(bsp_priv->pclk_mac))
				clk_prepare_enable(bsp_priv->pclk_mac);

			if (!IS_ERR(bsp_priv->mac_clk_tx))
				clk_prepare_enable(bsp_priv->mac_clk_tx);

			if (!IS_ERR(bsp_priv->clk_mac_speed))
				clk_prepare_enable(bsp_priv->clk_mac_speed);

			if (!IS_ERR(bsp_priv->pclk_xpcs))
				clk_prepare_enable(bsp_priv->pclk_xpcs);

			if (bsp_priv->ops && bsp_priv->ops->set_clock_selection)
				bsp_priv->ops->set_clock_selection(bsp_priv,
					       bsp_priv->clock_input, true);

			/**
			 * if (!IS_ERR(bsp_priv->clk_mac))
			 *	clk_prepare_enable(bsp_priv->clk_mac);
			 */
			usleep_range(100, 200);
			bsp_priv->clk_enabled = true;
		}
	} else {
		if (bsp_priv->clk_enabled) {
			if (bsp_priv->ops && bsp_priv->ops->set_clock_selection)
				bsp_priv->ops->set_clock_selection(bsp_priv,
					      bsp_priv->clock_input, false);

			if (phy_iface == PHY_INTERFACE_MODE_RMII) {
				clk_disable_unprepare(bsp_priv->mac_clk_rx);

				clk_disable_unprepare(bsp_priv->clk_mac_ref);

				clk_disable_unprepare(bsp_priv->clk_mac_refout);
			}

			clk_disable_unprepare(bsp_priv->clk_phy);

			clk_disable_unprepare(bsp_priv->aclk_mac);

			clk_disable_unprepare(bsp_priv->pclk_mac);

			clk_disable_unprepare(bsp_priv->mac_clk_tx);

			clk_disable_unprepare(bsp_priv->clk_mac_speed);

			clk_disable_unprepare(bsp_priv->pclk_xpcs);

			/**
			 * if (!IS_ERR(bsp_priv->clk_mac))
			 *	clk_disable_unprepare(bsp_priv->clk_mac);
			 */
			bsp_priv->clk_enabled = false;
		}
	}

	return 0;
}

static int rk_gmac_phy_power_on(struct rk_priv_data *bsp_priv, bool enable)
{
	struct regulator *ldo = bsp_priv->regulator;
	int ret;
	struct device *dev = &bsp_priv->pdev->dev;

	if (!ldo)
		return 0;

	if (enable) {
		ret = regulator_enable(ldo);
		if (ret)
			dev_err(dev, "fail to enable phy-supply\n");
	} else {
		ret = regulator_disable(ldo);
		if (ret)
			dev_err(dev, "fail to disable phy-supply\n");
	}

	return 0;
}

static struct rk_priv_data *rk_gmac_setup(struct platform_device *pdev,
					  struct plat_stmmacenet_data *plat,
					  const struct rk_gmac_ops *ops)
{
	struct rk_priv_data *bsp_priv;
	struct device *dev = &pdev->dev;
	int ret;
	const char *strings = NULL;
	int value;

	bsp_priv = devm_kzalloc(dev, sizeof(*bsp_priv), GFP_KERNEL);
	if (!bsp_priv)
		return ERR_PTR(-ENOMEM);

	of_get_phy_mode(dev->of_node, &bsp_priv->phy_iface);
	bsp_priv->ops = ops;
	bsp_priv->bus_id = plat->bus_id;

	bsp_priv->regulator = devm_regulator_get_optional(dev, "phy");
	if (IS_ERR(bsp_priv->regulator)) {
		if (PTR_ERR(bsp_priv->regulator) == -EPROBE_DEFER) {
			dev_err(dev, "phy regulator is not available yet, deferred probing\n");
			return ERR_PTR(-EPROBE_DEFER);
		}
		dev_err(dev, "no regulator found\n");
		bsp_priv->regulator = NULL;
	}

	ret = of_property_read_string(dev->of_node, "clock_in_out", &strings);
	if (ret) {
		dev_err(dev, "Can not read property: clock_in_out.\n");
		bsp_priv->clock_input = true;
	} else {
		dev_info(dev, "clock input or output? (%s).\n",
			 strings);
		if (!strcmp(strings, "input"))
			bsp_priv->clock_input = true;
		else
			bsp_priv->clock_input = false;
	}

	ret = of_property_read_u32(dev->of_node, "tx_delay", &value);
	if (ret) {
		bsp_priv->tx_delay = -1;
		dev_err(dev, "Can not read property: tx_delay.");
		dev_err(dev, "set tx_delay to 0x%x\n",
			bsp_priv->tx_delay);
	} else {
		dev_info(dev, "TX delay(0x%x).\n", value);
		bsp_priv->tx_delay = value;
	}

	ret = of_property_read_u32(dev->of_node, "rx_delay", &value);
	if (ret) {
		bsp_priv->rx_delay = -1;
		dev_err(dev, "Can not read property: rx_delay.");
		dev_err(dev, "set rx_delay to 0x%x\n",
			bsp_priv->rx_delay);
	} else {
		dev_info(dev, "RX delay(0x%x).\n", value);
		bsp_priv->rx_delay = value;
	}

	bsp_priv->grf = syscon_regmap_lookup_by_phandle(dev->of_node,
							"rockchip,grf");
	bsp_priv->php_grf = syscon_regmap_lookup_by_phandle(dev->of_node,
							    "rockchip,php_grf");
	bsp_priv->xpcs = syscon_regmap_lookup_by_phandle(dev->of_node,
							 "rockchip,xpcs");
	if (!IS_ERR(bsp_priv->xpcs)) {
		struct phy *comphy;

		comphy = devm_of_phy_get(&pdev->dev, dev->of_node, NULL);
		if (IS_ERR(comphy))
			dev_err(dev, "devm_of_phy_get error\n");
		ret = phy_init(comphy);
		if (ret)
			dev_err(dev, "phy_init error\n");
	}

	if (plat->phy_node) {
		bsp_priv->integrated_phy = of_property_read_bool(plat->phy_node,
								 "phy-is-integrated");
		if (bsp_priv->integrated_phy) {
			unsigned char *efuse_buf;
			struct nvmem_cell *cell;
			size_t len;

			bsp_priv->phy_reset = of_reset_control_get(plat->phy_node, NULL);
			if (IS_ERR(bsp_priv->phy_reset)) {
				dev_err(&pdev->dev, "No PHY reset control found.\n");
				bsp_priv->phy_reset = NULL;
			}

			/* Read bgs from OTP if it exists */
			cell = nvmem_cell_get(dev, "bgs");
			if (IS_ERR(cell)) {
				dev_info(dev, "failed to get bgs cell: %ld, use default\n",
					 PTR_ERR(cell));
			} else {
				efuse_buf = nvmem_cell_read(cell, &len);
				nvmem_cell_put(cell);
				if (!IS_ERR(efuse_buf)) {
					if (len == 1)
						bsp_priv->otp_data[0] = efuse_buf[0];
					kfree(efuse_buf);
				} else {
					dev_err(dev, "failed to get efuse buf, use default\n");
				}
			}
		}
	}
	dev_info(dev, "integrated PHY? (%s).\n",
		 bsp_priv->integrated_phy ? "yes" : "no");

	ret = of_property_read_u32(dev->of_node, "hardkernel,mac-rule", &value);
	if (ret)
		bsp_priv->hk_mac_rule = device_property_read_bool(dev, "hardkernel,mac-rule");
	else
		bsp_priv->hk_mac_rule = value;

	bsp_priv->pdev = pdev;

	return bsp_priv;
}

static int rk_gmac_powerup(struct rk_priv_data *bsp_priv)
{
	int ret;
	struct device *dev = &bsp_priv->pdev->dev;

	ret = gmac_clk_enable(bsp_priv, true);
	if (ret)
		return ret;

	/*rmii or rgmii*/
	switch (bsp_priv->phy_iface) {
	case PHY_INTERFACE_MODE_RGMII:
		dev_info(dev, "init for RGMII\n");
		if (bsp_priv->ops && bsp_priv->ops->set_to_rgmii)
			bsp_priv->ops->set_to_rgmii(bsp_priv, bsp_priv->tx_delay,
						    bsp_priv->rx_delay);
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		dev_info(dev, "init for RGMII_ID\n");
		if (bsp_priv->ops && bsp_priv->ops->set_to_rgmii)
			bsp_priv->ops->set_to_rgmii(bsp_priv, -1, -1);
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		dev_info(dev, "init for RGMII_RXID\n");
		if (bsp_priv->ops && bsp_priv->ops->set_to_rgmii)
			bsp_priv->ops->set_to_rgmii(bsp_priv, bsp_priv->tx_delay, -1);
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		dev_info(dev, "init for RGMII_TXID\n");
		if (bsp_priv->ops && bsp_priv->ops->set_to_rgmii)
			bsp_priv->ops->set_to_rgmii(bsp_priv, -1, bsp_priv->rx_delay);
		break;
	case PHY_INTERFACE_MODE_RMII:
		dev_info(dev, "init for RMII\n");
		if (bsp_priv->ops && bsp_priv->ops->set_to_rmii)
			bsp_priv->ops->set_to_rmii(bsp_priv);
		break;
	case PHY_INTERFACE_MODE_SGMII:
		dev_info(dev, "init for SGMII\n");
		if (bsp_priv->ops && bsp_priv->ops->set_to_sgmii)
			bsp_priv->ops->set_to_sgmii(bsp_priv);
		break;
	case PHY_INTERFACE_MODE_QSGMII:
		dev_info(dev, "init for QSGMII\n");
		if (bsp_priv->ops && bsp_priv->ops->set_to_qsgmii)
			bsp_priv->ops->set_to_qsgmii(bsp_priv);
		break;
	default:
		dev_err(dev, "NO interface defined!\n");
	}

	ret = rk_gmac_phy_power_on(bsp_priv, true);
	if (ret) {
		gmac_clk_enable(bsp_priv, false);
		return ret;
	}

	pm_runtime_get_sync(dev);

	return 0;
}

static void rk_gmac_powerdown(struct rk_priv_data *gmac)
{
	pm_runtime_put_sync(&gmac->pdev->dev);

	rk_gmac_phy_power_on(gmac, false);
	gmac_clk_enable(gmac, false);
}

static void rk_fix_speed(void *priv, unsigned int speed)
{
	struct rk_priv_data *bsp_priv = priv;
	struct device *dev = &bsp_priv->pdev->dev;

	switch (bsp_priv->phy_iface) {
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		if (bsp_priv->ops && bsp_priv->ops->set_rgmii_speed)
			bsp_priv->ops->set_rgmii_speed(bsp_priv, speed);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (bsp_priv->ops && bsp_priv->ops->set_rmii_speed)
			bsp_priv->ops->set_rmii_speed(bsp_priv, speed);
		break;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_QSGMII:
		break;
	default:
		dev_err(dev, "unsupported interface %d", bsp_priv->phy_iface);
	}
}

static int rk_integrated_phy_power(void *priv, bool up)
{
	struct rk_priv_data *bsp_priv = priv;

	if (!bsp_priv->integrated_phy || !bsp_priv->ops ||
	    !bsp_priv->ops->integrated_phy_power)
		return 0;

	bsp_priv->ops->integrated_phy_power(bsp_priv, up);

	return 0;
}

void dwmac_rk_set_rgmii_delayline(struct stmmac_priv *priv,
				  int tx_delay, int rx_delay)
{
	struct rk_priv_data *bsp_priv = priv->plat->bsp_priv;

	if (bsp_priv->ops->set_to_rgmii) {
		bsp_priv->ops->set_to_rgmii(bsp_priv, tx_delay, rx_delay);
		bsp_priv->tx_delay = tx_delay;
		bsp_priv->rx_delay = rx_delay;
	}
}
EXPORT_SYMBOL(dwmac_rk_set_rgmii_delayline);

void dwmac_rk_get_rgmii_delayline(struct stmmac_priv *priv,
				  int *tx_delay, int *rx_delay)
{
	struct rk_priv_data *bsp_priv = priv->plat->bsp_priv;

	if (!bsp_priv->ops->set_to_rgmii)
		return;

	*tx_delay = bsp_priv->tx_delay;
	*rx_delay = bsp_priv->rx_delay;
}
EXPORT_SYMBOL(dwmac_rk_get_rgmii_delayline);

int dwmac_rk_get_phy_interface(struct stmmac_priv *priv)
{
	struct rk_priv_data *bsp_priv = priv->plat->bsp_priv;

	return bsp_priv->phy_iface;
}
EXPORT_SYMBOL(dwmac_rk_get_phy_interface);

static unsigned char mac_addr[6];
#define CPUID_SIZE	16

/* Table of CRC constants - implements x^16+x^12+x^5+1 */
static const uint16_t crc16_tab[] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
};

uint16_t crc16_ccitt(uint16_t crc_start, unsigned char *buf, int len)
{
	int i;
	uint16_t cksum;

	cksum = crc_start;
	for (i = 0;  i < len;  i++)
		cksum = crc16_tab[((cksum>>8) ^ *buf++) & 0xff] ^ (cksum << 8);

	return cksum;
}

#define POLY	(0x1070U << 3)
static unsigned char _crc8(unsigned short data)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (data & 0x8000)
			data = data ^ POLY;
		data = data << 1;
	}

	return (unsigned char)(data >> 8);
}

unsigned int crc8(unsigned int crc, const unsigned char *vptr, int len)
{
	int i;

	for (i = 0; i < len; i++)
		crc = _crc8((crc ^ vptr[i]) << 8);

	return crc;
}

void get_rockchip_cpuid(unsigned char *buf);

void rk_setup_mac_addr(unsigned char *addr)
{
	unsigned char cpuid[CPUID_SIZE];
	unsigned char low[CPUID_SIZE/2];
	unsigned char high[CPUID_SIZE/2];
	unsigned char i;
	unsigned int temp;

	/* get cpuid from soc rockchip driver */
	get_rockchip_cpuid(cpuid);

	/* rearrange cpuid as 8bytes unit */
	for (i = 0; i < (CPUID_SIZE / 2); i++) {
		low[i] = cpuid[1 + (i << 1)];
		high[i] = cpuid[i << 1];
	}

	/* calculate crc16 using low : 8byte input -> 2byte output */
	temp = crc16_ccitt(0, low, 8);

	/* calculate crc8 using high : 8byte input -> 1byte output */
	temp |= (u64)crc8(temp, high, 8) << 16;

	/* fixed pattern */
	addr[0] = 0x00;
	addr[1] = 0x1e;
	addr[2] = 0x06;

	/* unique pattern */
	addr[3] = (char)(0xff & (temp >> 16));
	addr[4] = (char)(0xff & (temp >> 8));
	addr[5] = (char)(0xff & temp);
}

static void rk_get_eth_addr(void *priv, unsigned char *addr)
{
	struct rk_priv_data *bsp_priv = priv;
	struct device *dev = &bsp_priv->pdev->dev;
	unsigned char ethaddr[ETH_ALEN * MAX_ETH] = {0};
	int ret, id = bsp_priv->bus_id;

	/*
	 * we don't consider that saving and getting information
	 * using boot storage based on rockchip vendor storage solution
	 * because eMMC and SD card are removable and replaceable
	 * on ODROID-N1 HW
	 */
	/* check mac address from kernel command (boot.ini) */
	memcpy(addr, mac_addr, sizeof(mac_addr));
	if (is_valid_ether_addr(addr) && !is_zero_ether_addr(addr))
		goto out;

	/* if this condition comes, run initial set-up with Hardkernel rule */
	if (bsp_priv->hk_mac_rule == 1) {
		rk_setup_mac_addr(addr);
		if (is_zero_ether_addr(addr)) {
			dev_err(dev, "%s: rk_vendor_read eth mac address failed (%d)",
					__func__, ret);
			random_ether_addr(addr);
			dev_err(dev, "%s: generate random eth mac address: %02x:%02x:%02x:%02x:%02x:%02x",
					__func__, addr[0], addr[1], addr[2],
					addr[3], addr[4], addr[5]);
			ret = rk_vendor_write(LAN_MAC_ID, addr, 6);
			if (ret != 0)
				dev_err(dev, "%s: rk_vendor_write eth mac address failed (%d)",
						__func__, ret);
		}
		goto out;
	}

	if (is_valid_ether_addr(addr))
		goto out;

	if (id < 0 || id >= MAX_ETH) {
		dev_err(dev, "%s: Invalid ethernet bus id %d\n", __func__, id);
		return;
	}

	if (bsp_priv->hk_mac_rule == 2)
		goto out;

	ret = rk_vendor_read(LAN_MAC_ID, ethaddr, ETH_ALEN * MAX_ETH);
	if (ret <= 0 ||
	    !is_valid_ether_addr(&ethaddr[id * ETH_ALEN])) {
		dev_err(dev, "%s: rk_vendor_read eth mac address failed (%d)\n",
			__func__, ret);
		random_ether_addr(&ethaddr[id * ETH_ALEN]);
		memcpy(addr, &ethaddr[id * ETH_ALEN], ETH_ALEN);
		dev_err(dev, "%s: generate random eth mac address: %pM\n", __func__, addr);

		ret = rk_vendor_write(LAN_MAC_ID, ethaddr, ETH_ALEN * MAX_ETH);
		if (ret != 0)
			dev_err(dev, "%s: rk_vendor_write eth mac address failed (%d)\n",
				__func__, ret);

		ret = rk_vendor_read(LAN_MAC_ID, ethaddr, ETH_ALEN * MAX_ETH);
		if (ret != ETH_ALEN * MAX_ETH)
			dev_err(dev, "%s: id: %d rk_vendor_read eth mac address failed (%d)\n",
				__func__, id, ret);
	} else {
		memcpy(addr, &ethaddr[id * ETH_ALEN], ETH_ALEN);
	}

out:

	dev_err(dev, "%s: mac address: %pM\n", __func__, addr);
}

static int __init setup_mac_addr(char *str)
{
	char *opt;
	int i = 0;

	if (!str)
		return -EINVAL;

	while ((opt = strsep(&str, ":")) != NULL) {
		if (kstrtoint(opt, 16, (int *)&mac_addr[i]))
			return -EINVAL;
		i++;
	}

	return 1;
}
__setup("ethaddr=", setup_mac_addr);

static int rk_gmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	const struct rk_gmac_ops *data;
	int ret;

	data = of_device_get_match_data(&pdev->dev);
	if (!data) {
		dev_err(&pdev->dev, "no of match data provided\n");
		return -EINVAL;
	}

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	if (!of_device_is_compatible(pdev->dev.of_node, "snps,dwmac-4.20a"))
		plat_dat->has_gmac = true;

	plat_dat->sph_disable = true;
	plat_dat->fix_mac_speed = rk_fix_speed;
	plat_dat->get_eth_addr = rk_get_eth_addr;
	plat_dat->integrated_phy_power = rk_integrated_phy_power;

	plat_dat->bsp_priv = rk_gmac_setup(pdev, plat_dat, data);
	if (IS_ERR(plat_dat->bsp_priv)) {
		ret = PTR_ERR(plat_dat->bsp_priv);
		goto err_remove_config_dt;
	}

	ret = rk_gmac_clk_init(plat_dat);
	if (ret)
		goto err_remove_config_dt;

	ret = rk_gmac_powerup(plat_dat->bsp_priv);
	if (ret)
		goto err_remove_config_dt;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_gmac_powerdown;

	ret = dwmac_rk_create_loopback_sysfs(&pdev->dev);
	if (ret)
		goto err_gmac_powerdown;

	return 0;

err_gmac_powerdown:
	rk_gmac_powerdown(plat_dat->bsp_priv);
err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static int rk_gmac_remove(struct platform_device *pdev)
{
	struct rk_priv_data *bsp_priv = get_stmmac_bsp_priv(&pdev->dev);
	int ret = stmmac_dvr_remove(&pdev->dev);

	rk_gmac_powerdown(bsp_priv);
	dwmac_rk_remove_loopback_sysfs(&pdev->dev);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int rk_gmac_suspend(struct device *dev)
{
	struct rk_priv_data *bsp_priv = get_stmmac_bsp_priv(dev);
	int ret = stmmac_suspend(dev);

	/* Keep the PHY up if we use Wake-on-Lan. */
	if (!device_may_wakeup(dev)) {
		rk_gmac_powerdown(bsp_priv);
		bsp_priv->suspended = true;
	}

	return ret;
}

static int rk_gmac_resume(struct device *dev)
{
	struct rk_priv_data *bsp_priv = get_stmmac_bsp_priv(dev);

	/* The PHY was up for Wake-on-Lan. */
	if (bsp_priv->suspended) {
		rk_gmac_powerup(bsp_priv);
		bsp_priv->suspended = false;
	}

	return stmmac_resume(dev);
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(rk_gmac_pm_ops, rk_gmac_suspend, rk_gmac_resume);

static const struct of_device_id rk_gmac_dwmac_match[] = {
#ifdef CONFIG_CPU_PX30
	{ .compatible = "rockchip,px30-gmac",	.data = &px30_ops   },
#endif
#ifdef CONFIG_CPU_RK1808
	{ .compatible = "rockchip,rk1808-gmac", .data = &rk1808_ops },
#endif
#ifdef CONFIG_CPU_RK312X
	{ .compatible = "rockchip,rk3128-gmac", .data = &rk3128_ops },
#endif
#ifdef CONFIG_CPU_RK322X
	{ .compatible = "rockchip,rk3228-gmac", .data = &rk3228_ops },
#endif
#ifdef CONFIG_CPU_RK3288
	{ .compatible = "rockchip,rk3288-gmac", .data = &rk3288_ops },
#endif
#ifdef CONFIG_CPU_RK3308
	{ .compatible = "rockchip,rk3308-mac",  .data = &rk3308_ops },
#endif
#ifdef CONFIG_CPU_RK3328
	{ .compatible = "rockchip,rk3328-gmac", .data = &rk3328_ops },
#endif
#ifdef CONFIG_CPU_RK3366
	{ .compatible = "rockchip,rk3366-gmac", .data = &rk3366_ops },
#endif
#ifdef CONFIG_CPU_RK3368
	{ .compatible = "rockchip,rk3368-gmac", .data = &rk3368_ops },
#endif
#ifdef CONFIG_CPU_RK3399
	{ .compatible = "rockchip,rk3399-gmac", .data = &rk3399_ops },
#endif
#ifdef CONFIG_CPU_RK3568
	{ .compatible = "rockchip,rk3568-gmac", .data = &rk3568_ops },
#endif
#ifdef CONFIG_CPU_RK3588
	{ .compatible = "rockchip,rk3588-gmac", .data = &rk3588_ops },
#endif
#ifdef CONFIG_CPU_RV1106
	{ .compatible = "rockchip,rv1106-gmac", .data = &rv1106_ops },
#endif
#ifdef CONFIG_CPU_RV1108
	{ .compatible = "rockchip,rv1108-gmac", .data = &rv1108_ops },
#endif
#ifdef CONFIG_CPU_RV1126
	{ .compatible = "rockchip,rv1126-gmac", .data = &rv1126_ops },
#endif
	{ }
};
MODULE_DEVICE_TABLE(of, rk_gmac_dwmac_match);

static struct platform_driver rk_gmac_dwmac_driver = {
	.probe  = rk_gmac_probe,
	.remove = rk_gmac_remove,
	.driver = {
		.name           = "rk_gmac-dwmac",
		.pm		= &rk_gmac_pm_ops,
		.of_match_table = rk_gmac_dwmac_match,
	},
};

#ifdef MODULE
module_platform_driver(rk_gmac_dwmac_driver);
#else
static int __init rk_gmac_init(void)
{
	return platform_driver_register(&rk_gmac_dwmac_driver);
}

static void __exit rk_gmac_exit(void)
{
	platform_driver_unregister(&rk_gmac_dwmac_driver);
}

late_initcall(rk_gmac_init)
module_exit(rk_gmac_exit)
#endif

MODULE_AUTHOR("Chen-Zhi (Roger Chen) <roger.chen@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip RK3288 DWMAC specific glue layer");
MODULE_LICENSE("GPL");
