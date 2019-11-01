// SPDX-License-Identifier: GPL-2.0-only
/*
 * MFD core driver for Rockchip RK8XX
 *
 * Copyright (c) 2014-2018, Fuzhou Rockchip Electronics Co., Ltd
 * Copyright (C) 2016 PHYTEC Messtechnik GmbH
 *
 * Author: Chris Zhong <zyw@rock-chips.com>
 * Author: Zhang Qing <zhangqing@rock-chips.com>
 * Author: Wadim Egorov <w.egorov@phytec.de>
 */

#include <linux/interrupt.h>
#include <linux/mfd/rk808.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/reboot.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/devinfo.h>

struct rk808_reg_data {
	int addr;
	int mask;
	int value;
};

static const struct resource rtc_resources[] = {
	DEFINE_RES_IRQ(RK808_IRQ_RTC_ALARM),
};

static const struct resource rk816_rtc_resources[] = {
	DEFINE_RES_IRQ(RK816_IRQ_RTC_ALARM),
};

static const struct resource rk817_rtc_resources[] = {
	DEFINE_RES_IRQ(RK817_IRQ_RTC_ALARM),
};

static const struct resource rk805_key_resources[] = {
	DEFINE_RES_IRQ(RK805_IRQ_PWRON_RISE),
	DEFINE_RES_IRQ(RK805_IRQ_PWRON_FALL),
};

static struct resource rk806_pwrkey_resources[] = {
	DEFINE_RES_IRQ(RK806_IRQ_PWRON_FALL),
	DEFINE_RES_IRQ(RK806_IRQ_PWRON_RISE),
};

static const struct resource rk817_pwrkey_resources[] = {
	DEFINE_RES_IRQ(RK817_IRQ_PWRON_FALL),
	DEFINE_RES_IRQ(RK817_IRQ_PWRON_RISE),
};

static const struct resource rk817_charger_resources[] = {
	DEFINE_RES_IRQ(RK817_IRQ_PLUG_IN),
	DEFINE_RES_IRQ(RK817_IRQ_PLUG_OUT),
};

static const struct mfd_cell rk805s[] = {
	{ .name = "rk808-clkout", },
	{ .name = "rk808-regulator", },
	{ .name = "rk805-pinctrl", },
	{
		.name = "rk808-rtc",
		.num_resources = ARRAY_SIZE(rtc_resources),
		.resources = &rtc_resources[0],
	},
	{	.name = "rk805-pwrkey",
		.num_resources = ARRAY_SIZE(rk805_key_resources),
		.resources = &rk805_key_resources[0],
	},
};

static const struct mfd_cell rk806s[] = {
	{ .name = "rk805-pinctrl", },
	{ .name = "rk808-regulator", },
	{
		.name = "rk805-pwrkey",
		.resources = rk806_pwrkey_resources,
		.num_resources = ARRAY_SIZE(rk806_pwrkey_resources),
	},
};

static const struct mfd_cell rk808s[] = {
	{ .name = "rk808-clkout", },
	{ .name = "rk808-regulator", },
	{
		.name = "rk808-rtc",
		.num_resources = ARRAY_SIZE(rtc_resources),
		.resources = rtc_resources,
	},
};

static const struct mfd_cell rk816s[] = {
	{ .name = "rk805-pinctrl", },
	{ .name = "rk808-clkout", },
	{ .name = "rk808-regulator", },
	{ .name = "rk816-battery", .of_compatible = "rk816-battery", },
	{
		.name = "rk805-pwrkey",
		.num_resources = ARRAY_SIZE(rk805_key_resources),
		.resources = rk805_key_resources,
	},
	{
		.name = "rk808-rtc",
		.num_resources = ARRAY_SIZE(rk816_rtc_resources),
		.resources = rk816_rtc_resources,
	},
};

static const struct mfd_cell rk817s[] = {
	{ .name = "rk808-clkout", },
	{ .name = "rk808-regulator", },
	{ .name = "rk817-battery", .of_compatible = "rk817,battery", },
	{ .name = "rk817-charger", .of_compatible = "rk817,charger", },
	{
		.name = "rk805-pwrkey",
		.num_resources = ARRAY_SIZE(rk817_pwrkey_resources),
		.resources = &rk817_pwrkey_resources[0],
	},
	{
		.name = "rk808-rtc",
		.num_resources = ARRAY_SIZE(rk817_rtc_resources),
		.resources = &rk817_rtc_resources[0],
	},
	{
		.name = "rk817-codec",
		.of_compatible = "rockchip,rk817-codec",
	},
	{
		.name = "rk817-charger",
		.num_resources = ARRAY_SIZE(rk817_charger_resources),
		.resources = &rk817_charger_resources[0],
	},
};

static const struct mfd_cell rk818s[] = {
	{ .name = "rk808-clkout", },
	{ .name = "rk808-regulator", },
	{ .name = "rk818-battery", .of_compatible = "rk818-battery", },
	{ .name = "rk818-charger", },
	{
		.name = "rk808-rtc",
		.num_resources = ARRAY_SIZE(rtc_resources),
		.resources = rtc_resources,
	},
};

static const struct rk808_reg_data rk805_pre_init_reg[] = {
	{RK805_BUCK4_CONFIG_REG, BUCK_ILMIN_MASK, BUCK_ILMIN_400MA},
	{RK805_GPIO_IO_POL_REG, SLP_SD_MSK, SLEEP_FUN},
	{RK805_THERMAL_REG, TEMP_HOTDIE_MSK, TEMP115C},
	{RK808_RTC_CTRL_REG, RTC_STOP, RTC_STOP},
};

static const struct rk808_reg_data rk806_pre_init_reg[] = {
	{ RK806_GPIO_INT_CONFIG, RK806_INT_POL_MSK, RK806_INT_POL_L },
	{ RK806_SYS_CFG3, RK806_SLAVE_RESTART_FUN_MSK, RK806_SLAVE_RESTART_FUN_EN },
	{ RK806_SYS_OPTION, RK806_SYS_ENB2_2M_MSK, RK806_SYS_ENB2_2M_EN },
};

static struct rk808_reg_data rk805_suspend_reg[] = {
	{RK805_BUCK3_CONFIG_REG, PWM_MODE_MSK, AUTO_PWM_MODE},
};

static struct rk808_reg_data rk805_resume_reg[] = {
	{RK805_BUCK3_CONFIG_REG, PWM_MODE_MSK, FPWM_MODE},
};

static const struct rk808_reg_data rk808_pre_init_reg[] = {
	{ RK808_BUCK3_CONFIG_REG, BUCK_ILMIN_MASK,  BUCK_ILMIN_150MA },
	{ RK808_BUCK4_CONFIG_REG, BUCK_ILMIN_MASK,  BUCK_ILMIN_200MA },
	{ RK808_BOOST_CONFIG_REG, BOOST_ILMIN_MASK, BOOST_ILMIN_100MA },
	{ RK808_BUCK1_CONFIG_REG, BUCK1_RATE_MASK,  BUCK_ILMIN_200MA },
	{ RK808_BUCK2_CONFIG_REG, BUCK2_RATE_MASK,  BUCK_ILMIN_200MA },
	{ RK808_DCDC_UV_ACT_REG,  BUCK_UV_ACT_MASK, BUCK_UV_ACT_DISABLE},
	{ RK808_RTC_CTRL_REG, RTC_STOP, RTC_STOP},
	{ RK808_VB_MON_REG,       MASK_ALL,         VB_LO_ACT |
						    VB_LO_SEL_3500MV },
};

static const struct rk808_reg_data rk816_pre_init_reg[] = {
	/* buck4 Max ILMIT*/
	{ RK816_BUCK4_CONFIG_REG, REG_WRITE_MSK, BUCK4_MAX_ILIMIT },
	/* hotdie temperature: 105c*/
	{ RK816_THERMAL_REG, REG_WRITE_MSK, TEMP105C },
	/* set buck 12.5mv/us */
	{ RK816_BUCK1_CONFIG_REG, BUCK_RATE_MSK, BUCK_RATE_12_5MV_US },
	{ RK816_BUCK2_CONFIG_REG, BUCK_RATE_MSK, BUCK_RATE_12_5MV_US },
	/* enable RTC_PERIOD & RTC_ALARM int */
	{ RK816_INT_STS_MSK_REG2, REG_WRITE_MSK, RTC_PERIOD_ALARM_INT_EN },
	/* set bat 3.0 low and act shutdown */
	{ RK816_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK816_VBAT_LOW_3V0 | EN_VABT_LOW_SHUT_DOWN },
	/* enable PWRON rising/faling int */
	{ RK816_INT_STS_MSK_REG1, REG_WRITE_MSK, RK816_PWRON_FALL_RISE_INT_EN },
	/* enable PLUG IN/OUT int */
	{ RK816_INT_STS_MSK_REG3, REG_WRITE_MSK, PLUGIN_OUT_INT_EN },
	/* clear int flags */
	{ RK816_INT_STS_REG1, REG_WRITE_MSK, ALL_INT_FLAGS_ST },
	{ RK816_INT_STS_REG2, REG_WRITE_MSK, ALL_INT_FLAGS_ST },
	{ RK816_INT_STS_REG3, REG_WRITE_MSK, ALL_INT_FLAGS_ST },
	{ RK816_DCDC_EN_REG2, BOOST_EN_MASK, BOOST_DISABLE },
	/* set write mask bit 1, otherwise 'is_enabled()' get wrong status */
	{ RK816_LDO_EN_REG1, REGS_WMSK, REGS_WMSK },
	{ RK816_LDO_EN_REG2, REGS_WMSK, REGS_WMSK },
};

static const struct rk808_reg_data rk817_pre_init_reg[] = {
	{RK817_SYS_CFG(3), RK817_SLPPOL_MSK, RK817_SLPPOL_L},
	{RK817_RTC_CTRL_REG, RTC_STOP, RTC_STOP},
	/* Codec specific registers */
	{ RK817_CODEC_DTOP_VUCTL, MASK_ALL, 0x03 },
	{ RK817_CODEC_DTOP_VUCTIME, MASK_ALL, 0x00 },
	{ RK817_CODEC_DTOP_LPT_SRST, MASK_ALL, 0x00 },
	{ RK817_CODEC_DTOP_DIGEN_CLKE, MASK_ALL, 0x00 },
	/* from vendor driver, CODEC_AREF_RTCFG0 not defined in data sheet */
	{ RK817_CODEC_AREF_RTCFG0, MASK_ALL, 0x00 },
	{ RK817_CODEC_AREF_RTCFG1, MASK_ALL, 0x06 },
	{ RK817_CODEC_AADC_CFG0, MASK_ALL, 0xc8 },
	/* from vendor driver, CODEC_AADC_CFG1 not defined in data sheet */
	{ RK817_CODEC_AADC_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_VOLL, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_VOLR, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_SR_ACL0, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_ALC1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_ALC2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_NG, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_HPF, MASK_ALL, 0x00 },
	{ RK817_CODEC_DADC_RVOLL, MASK_ALL, 0xff },
	{ RK817_CODEC_DADC_RVOLR, MASK_ALL, 0xff },
	{ RK817_CODEC_AMIC_CFG0, MASK_ALL, 0x70 },
	{ RK817_CODEC_AMIC_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_PGA_GAIN, MASK_ALL, 0x66 },
	{ RK817_CODEC_DMIC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG2, MASK_ALL, 0x00 },
	/* from vendor driver, CODEC_ADAC_CFG0 not defined in data sheet */
	{ RK817_CODEC_ADAC_CFG0, MASK_ALL, 0x00 },
	{ RK817_CODEC_ADAC_CFG1, MASK_ALL, 0x07 },
	{ RK817_CODEC_DDAC_POPD_DACST, MASK_ALL, 0x82 },
	{ RK817_CODEC_DDAC_VOLL, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_VOLR, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_SR_LMT0, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_MUTE_MIXCTL, MASK_ALL, 0xa0 },
	{ RK817_CODEC_DDAC_RVOLL, MASK_ALL, 0xff },
	{ RK817_CODEC_DADC_RVOLR, MASK_ALL, 0xff },
	{ RK817_CODEC_AMIC_CFG0, MASK_ALL, 0x70 },
	{ RK817_CODEC_AMIC_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_PGA_GAIN, MASK_ALL, 0x66 },
	{ RK817_CODEC_DMIC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DMIC_NG2, MASK_ALL, 0x00 },
	/* from vendor driver, CODEC_ADAC_CFG0 not defined in data sheet */
	{ RK817_CODEC_ADAC_CFG0, MASK_ALL, 0x00 },
	{ RK817_CODEC_ADAC_CFG1, MASK_ALL, 0x07 },
	{ RK817_CODEC_DDAC_POPD_DACST, MASK_ALL, 0x82 },
	{ RK817_CODEC_DDAC_VOLL, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_VOLR, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_SR_LMT0, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_LMT2, MASK_ALL, 0x00 },
	{ RK817_CODEC_DDAC_MUTE_MIXCTL, MASK_ALL, 0xa0 },
	{ RK817_CODEC_DDAC_RVOLL, MASK_ALL, 0xff },
	{ RK817_CODEC_DDAC_RVOLR, MASK_ALL, 0xff },
	{ RK817_CODEC_AHP_ANTI0, MASK_ALL, 0x00 },
	{ RK817_CODEC_AHP_ANTI1, MASK_ALL, 0x00 },
	{ RK817_CODEC_AHP_CFG0, MASK_ALL, 0xe0 },
	{ RK817_CODEC_AHP_CFG1, MASK_ALL, 0x1f },
	{ RK817_CODEC_AHP_CP, MASK_ALL, 0x09 },
	{ RK817_CODEC_ACLASSD_CFG1, MASK_ALL, 0x69 },
	{ RK817_CODEC_ACLASSD_CFG2, MASK_ALL, 0x44 },
	{ RK817_CODEC_APLL_CFG0, MASK_ALL, 0x04 },
	{ RK817_CODEC_APLL_CFG1, MASK_ALL, 0x00 },
	{ RK817_CODEC_APLL_CFG2, MASK_ALL, 0x30 },
	{ RK817_CODEC_APLL_CFG3, MASK_ALL, 0x19 },
	{ RK817_CODEC_APLL_CFG4, MASK_ALL, 0x65 },
	{ RK817_CODEC_APLL_CFG5, MASK_ALL, 0x01 },
	{ RK817_CODEC_DI2S_CKM, MASK_ALL, 0x01 },
	{ RK817_CODEC_DI2S_RSD, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_RXCR1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_RXCR2, MASK_ALL, 0x17 },
	{ RK817_CODEC_DI2S_RXCMD_TSD, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_TXCR1, MASK_ALL, 0x00 },
	{ RK817_CODEC_DI2S_TXCR2, MASK_ALL, 0x17 },
	{ RK817_CODEC_DI2S_TXCR3_TXCMD, MASK_ALL, 0x00 },
	{RK817_GPIO_INT_CFG, RK817_INT_POL_MSK, RK817_INT_POL_L},
	{RK817_SYS_CFG(1), RK817_HOTDIE_TEMP_MSK | RK817_TSD_TEMP_MSK,
					   RK817_HOTDIE_105 | RK817_TSD_140},
};

static const struct rk808_reg_data rk818_pre_init_reg[] = {
	/* improve efficiency */
	{ RK818_BUCK2_CONFIG_REG, BUCK2_RATE_MASK,  BUCK_ILMIN_250MA },
	{ RK818_BUCK4_CONFIG_REG, BUCK_ILMIN_MASK,  BUCK_ILMIN_250MA },
	{ RK818_BOOST_CONFIG_REG, BOOST_ILMIN_MASK, BOOST_ILMIN_100MA },
	{ RK818_USB_CTRL_REG,	  RK818_USB_ILIM_SEL_MASK,
						    RK818_USB_ILMIN_2000MA },
	/* close charger when usb lower then 3.4V */
	{ RK818_USB_CTRL_REG,	  RK818_USB_CHG_SD_VSEL_MASK,
						    (0x7 << 4) },
	/* no action when vref */
	{ RK818_H5V_EN_REG,	  BIT(1),	    RK818_REF_RDY_CTRL },
	/* enable HDMI 5V */
	{ RK818_H5V_EN_REG,	  BIT(0),	    RK818_H5V_EN },
	{ RK808_RTC_CTRL_REG, RTC_STOP, RTC_STOP},
	{ RK808_VB_MON_REG,	  MASK_ALL,	    VB_LO_ACT |
						    VB_LO_SEL_3500MV },
	{RK808_CLK32OUT_REG, CLK32KOUT2_FUNC_MASK, CLK32KOUT2_FUNC},
};

static const struct regmap_irq rk805_irqs[] = {
	[RK805_IRQ_PWRON_RISE] = {
		.mask = RK805_IRQ_PWRON_RISE_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_VB_LOW] = {
		.mask = RK805_IRQ_VB_LOW_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_PWRON] = {
		.mask = RK805_IRQ_PWRON_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_PWRON_LP] = {
		.mask = RK805_IRQ_PWRON_LP_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_HOTDIE] = {
		.mask = RK805_IRQ_HOTDIE_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_RTC_ALARM] = {
		.mask = RK805_IRQ_RTC_ALARM_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_RTC_PERIOD] = {
		.mask = RK805_IRQ_RTC_PERIOD_MSK,
		.reg_offset = 0,
	},
	[RK805_IRQ_PWRON_FALL] = {
		.mask = RK805_IRQ_PWRON_FALL_MSK,
		.reg_offset = 0,
	},
};

static const struct regmap_irq rk806_irqs[] = {
	/* INT_STS0 IRQs */
	REGMAP_IRQ_REG(RK806_IRQ_PWRON_FALL, 0, RK806_INT_STS_PWRON_FALL),
	REGMAP_IRQ_REG(RK806_IRQ_PWRON_RISE, 0, RK806_INT_STS_PWRON_RISE),
	REGMAP_IRQ_REG(RK806_IRQ_PWRON, 0, RK806_INT_STS_PWRON),
	REGMAP_IRQ_REG(RK806_IRQ_PWRON_LP, 0, RK806_INT_STS_PWRON_LP),
	REGMAP_IRQ_REG(RK806_IRQ_HOTDIE, 0, RK806_INT_STS_HOTDIE),
	REGMAP_IRQ_REG(RK806_IRQ_VDC_RISE, 0, RK806_INT_STS_VDC_RISE),
	REGMAP_IRQ_REG(RK806_IRQ_VDC_FALL, 0, RK806_INT_STS_VDC_FALL),
	REGMAP_IRQ_REG(RK806_IRQ_VB_LO, 0, RK806_INT_STS_VB_LO),
	/* INT_STS1 IRQs */
	REGMAP_IRQ_REG(RK806_IRQ_REV0, 1, RK806_INT_STS_REV0),
	REGMAP_IRQ_REG(RK806_IRQ_REV1, 1, RK806_INT_STS_REV1),
	REGMAP_IRQ_REG(RK806_IRQ_REV2, 1, RK806_INT_STS_REV2),
	REGMAP_IRQ_REG(RK806_IRQ_CRC_ERROR, 1, RK806_INT_STS_CRC_ERROR),
	REGMAP_IRQ_REG(RK806_IRQ_SLP3_GPIO, 1, RK806_INT_STS_SLP3_GPIO),
	REGMAP_IRQ_REG(RK806_IRQ_SLP2_GPIO, 1, RK806_INT_STS_SLP2_GPIO),
	REGMAP_IRQ_REG(RK806_IRQ_SLP1_GPIO, 1, RK806_INT_STS_SLP1_GPIO),
	REGMAP_IRQ_REG(RK806_IRQ_WDT, 1, RK806_INT_STS_WDT),
};

static const struct regmap_irq rk808_irqs[] = {
	/* INT_STS */
	[RK808_IRQ_VOUT_LO] = {
		.mask = RK808_IRQ_VOUT_LO_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_VB_LO] = {
		.mask = RK808_IRQ_VB_LO_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_PWRON] = {
		.mask = RK808_IRQ_PWRON_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_PWRON_LP] = {
		.mask = RK808_IRQ_PWRON_LP_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_HOTDIE] = {
		.mask = RK808_IRQ_HOTDIE_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_RTC_ALARM] = {
		.mask = RK808_IRQ_RTC_ALARM_MSK,
		.reg_offset = 0,
	},
	[RK808_IRQ_RTC_PERIOD] = {
		.mask = RK808_IRQ_RTC_PERIOD_MSK,
		.reg_offset = 0,
	},

	/* INT_STS2 */
	[RK808_IRQ_PLUG_IN_INT] = {
		.mask = RK808_IRQ_PLUG_IN_INT_MSK,
		.reg_offset = 1,
	},
	[RK808_IRQ_PLUG_OUT_INT] = {
		.mask = RK808_IRQ_PLUG_OUT_INT_MSK,
		.reg_offset = 1,
	},
};

static const unsigned int rk816_irq_status_offsets[] = {
	RK816_IRQ_STS_OFFSET(RK816_INT_STS_REG1),
	RK816_IRQ_STS_OFFSET(RK816_INT_STS_REG2),
	RK816_IRQ_STS_OFFSET(RK816_INT_STS_REG3),
};

static const unsigned int rk816_irq_mask_offsets[] = {
	RK816_IRQ_MSK_OFFSET(RK816_INT_STS_MSK_REG1),
	RK816_IRQ_MSK_OFFSET(RK816_INT_STS_MSK_REG2),
	RK816_IRQ_MSK_OFFSET(RK816_INT_STS_MSK_REG3),
};

static struct rk808_reg_data rk816_suspend_reg[] = {
	/* set bat 3.4v low and act irq */
	{ RK816_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK816_VBAT_LOW_3V4 | EN_VBAT_LOW_IRQ },
};

static struct rk808_reg_data rk816_resume_reg[] = {
	/* set bat 3.0v low and act shutdown */
	{ RK816_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK816_VBAT_LOW_3V0 | EN_VABT_LOW_SHUT_DOWN },
};

static unsigned int rk816_get_irq_reg(struct regmap_irq_chip_data *data,
				      unsigned int base, int index)
{
	unsigned int irq_reg = base;

	switch (base) {
	case RK816_INT_STS_REG1:
		irq_reg += rk816_irq_status_offsets[index];
		break;
	case RK816_INT_STS_MSK_REG1:
		irq_reg += rk816_irq_mask_offsets[index];
		break;
	}

	return irq_reg;
};

static const struct regmap_irq rk816_irqs[] = {
	/* INT_STS_REG1 IRQs */
	REGMAP_IRQ_REG(RK816_IRQ_PWRON_FALL, 0, RK816_INT_STS_PWRON_FALL),
	REGMAP_IRQ_REG(RK816_IRQ_PWRON_RISE, 0, RK816_INT_STS_PWRON_RISE),

	/* INT_STS_REG2 IRQs  */
	REGMAP_IRQ_REG(RK816_IRQ_VB_LOW, 1, RK816_INT_STS_VB_LOW),
	REGMAP_IRQ_REG(RK816_IRQ_PWRON, 1, RK816_INT_STS_PWRON),
	REGMAP_IRQ_REG(RK816_IRQ_PWRON_LP, 1, RK816_INT_STS_PWRON_LP),
	REGMAP_IRQ_REG(RK816_IRQ_HOTDIE, 1, RK816_INT_STS_HOTDIE),
	REGMAP_IRQ_REG(RK816_IRQ_RTC_ALARM, 1, RK816_INT_STS_RTC_ALARM),
	REGMAP_IRQ_REG(RK816_IRQ_RTC_PERIOD, 1, RK816_INT_STS_RTC_PERIOD),
	REGMAP_IRQ_REG(RK816_IRQ_USB_OV, 1, RK816_INT_STS_USB_OV),

	/* INT_STS3 IRQs */
	REGMAP_IRQ_REG(RK816_IRQ_PLUG_IN, 2, RK816_INT_STS_PLUG_IN),
	REGMAP_IRQ_REG(RK816_IRQ_PLUG_OUT, 2, RK816_INT_STS_PLUG_OUT),
	REGMAP_IRQ_REG(RK816_IRQ_CHG_OK, 2, RK816_INT_STS_CHG_OK),
	REGMAP_IRQ_REG(RK816_IRQ_CHG_TE, 2, RK816_INT_STS_CHG_TE),
	REGMAP_IRQ_REG(RK816_IRQ_CHG_TS, 2, RK816_INT_STS_CHG_TS),
	REGMAP_IRQ_REG(RK816_IRQ_CHG_CVTLIM, 2, RK816_INT_STS_CHG_CVTLIM),
	REGMAP_IRQ_REG(RK816_IRQ_DISCHG_ILIM, 2, RK816_INT_STS_DISCHG_ILIM),
};

static struct rk808_reg_data rk818_suspend_reg[] = {
	/* set bat 3.4v low and act irq */
	{ RK808_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK808_VBAT_LOW_3V4 | EN_VBAT_LOW_IRQ },
};

static struct rk808_reg_data rk818_resume_reg[] = {
	/* set bat 3.0v low and act shutdown */
	{ RK808_VB_MON_REG, VBAT_LOW_VOL_MASK | VBAT_LOW_ACT_MASK,
	  RK808_VBAT_LOW_3V0 | EN_VABT_LOW_SHUT_DOWN },
};

static const struct regmap_irq rk818_irqs[] = {
	/* INT_STS */
	[RK818_IRQ_VOUT_LO] = {
		.mask = RK818_IRQ_VOUT_LO_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_VB_LO] = {
		.mask = RK818_IRQ_VB_LO_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_PWRON] = {
		.mask = RK818_IRQ_PWRON_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_PWRON_LP] = {
		.mask = RK818_IRQ_PWRON_LP_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_HOTDIE] = {
		.mask = RK818_IRQ_HOTDIE_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_RTC_ALARM] = {
		.mask = RK818_IRQ_RTC_ALARM_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_RTC_PERIOD] = {
		.mask = RK818_IRQ_RTC_PERIOD_MSK,
		.reg_offset = 0,
	},
	[RK818_IRQ_USB_OV] = {
		.mask = RK818_IRQ_USB_OV_MSK,
		.reg_offset = 0,
	},

	/* INT_STS2 */
	[RK818_IRQ_PLUG_IN] = {
		.mask = RK818_IRQ_PLUG_IN_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_PLUG_OUT] = {
		.mask = RK818_IRQ_PLUG_OUT_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_OK] = {
		.mask = RK818_IRQ_CHG_OK_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_TE] = {
		.mask = RK818_IRQ_CHG_TE_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_TS1] = {
		.mask = RK818_IRQ_CHG_TS1_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_TS2] = {
		.mask = RK818_IRQ_TS2_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_CHG_CVTLIM] = {
		.mask = RK818_IRQ_CHG_CVTLIM_MSK,
		.reg_offset = 1,
	},
	[RK818_IRQ_DISCHG_ILIM] = {
		.mask = RK818_IRQ_DISCHG_ILIM_MSK,
		.reg_offset = 1,
	},
};

static const struct regmap_irq rk817_irqs[RK817_IRQ_END] = {
	REGMAP_IRQ_REG_LINE(0, 8),
	REGMAP_IRQ_REG_LINE(1, 8),
	REGMAP_IRQ_REG_LINE(2, 8),
	REGMAP_IRQ_REG_LINE(3, 8),
	REGMAP_IRQ_REG_LINE(4, 8),
	REGMAP_IRQ_REG_LINE(5, 8),
	REGMAP_IRQ_REG_LINE(6, 8),
	REGMAP_IRQ_REG_LINE(7, 8),
	REGMAP_IRQ_REG_LINE(8, 8),
	REGMAP_IRQ_REG_LINE(9, 8),
	REGMAP_IRQ_REG_LINE(10, 8),
	REGMAP_IRQ_REG_LINE(11, 8),
	REGMAP_IRQ_REG_LINE(12, 8),
	REGMAP_IRQ_REG_LINE(13, 8),
	REGMAP_IRQ_REG_LINE(14, 8),
	REGMAP_IRQ_REG_LINE(15, 8),
	REGMAP_IRQ_REG_LINE(16, 8),
	REGMAP_IRQ_REG_LINE(17, 8),
	REGMAP_IRQ_REG_LINE(18, 8),
	REGMAP_IRQ_REG_LINE(19, 8),
	REGMAP_IRQ_REG_LINE(20, 8),
	REGMAP_IRQ_REG_LINE(21, 8),
	REGMAP_IRQ_REG_LINE(22, 8),
	REGMAP_IRQ_REG_LINE(23, 8)
};

static const struct regmap_irq_chip rk805_irq_chip = {
	.name = "rk805",
	.irqs = rk805_irqs,
	.num_irqs = ARRAY_SIZE(rk805_irqs),
	.num_regs = 1,
	.status_base = RK805_INT_STS_REG,
	.mask_base = RK805_INT_STS_MSK_REG,
	.ack_base = RK805_INT_STS_REG,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk806_irq_chip = {
	.name = "rk806",
	.irqs = rk806_irqs,
	.num_irqs = ARRAY_SIZE(rk806_irqs),
	.num_regs = 2,
	.irq_reg_stride = 2,
	.mask_base = RK806_INT_MSK0,
	.status_base = RK806_INT_STS0,
	.ack_base = RK806_INT_STS0,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk808_irq_chip = {
	.name = "rk808",
	.irqs = rk808_irqs,
	.num_irqs = ARRAY_SIZE(rk808_irqs),
	.num_regs = 2,
	.irq_reg_stride = 2,
	.status_base = RK808_INT_STS_REG1,
	.mask_base = RK808_INT_STS_MSK_REG1,
	.ack_base = RK808_INT_STS_REG1,
	.init_ack_masked = true,
};

static const struct regmap_irq rk816_battery_irqs[] = {
	/* INT_STS */
	[RK816_IRQ_PLUG_IN] = {
		.mask = RK816_IRQ_PLUG_IN_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_PLUG_OUT] = {
		.mask = RK816_IRQ_PLUG_OUT_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_OK] = {
		.mask = RK816_IRQ_CHG_OK_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_TE] = {
		.mask = RK816_IRQ_CHG_TE_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_TS] = {
		.mask = RK816_IRQ_CHG_TS_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_CHG_CVTLIM] = {
		.mask = RK816_IRQ_CHG_CVTLIM_MSK,
		.reg_offset = 0,
	},
	[RK816_IRQ_DISCHG_ILIM] = {
		.mask = RK816_IRQ_DISCHG_ILIM_MSK,
		.reg_offset = 0,
	},
};

static const struct regmap_irq_chip rk816_irq_chip = {
	.name = "rk816",
	.irqs = rk816_irqs,
	.num_irqs = ARRAY_SIZE(rk816_irqs),
	.num_regs = 3,
	.get_irq_reg = rk816_get_irq_reg,
	.status_base = RK816_INT_STS_REG1,
	.mask_base = RK816_INT_STS_MSK_REG1,
	.ack_base = RK816_INT_STS_REG1,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk816_battery_irq_chip = {
	.name = "rk816_battery",
	.irqs = rk816_battery_irqs,
	.num_irqs = ARRAY_SIZE(rk816_battery_irqs),
	.num_regs = 1,
	.status_base = RK816_INT_STS_REG3,
	.mask_base = RK816_INT_STS_MSK_REG3,
	.ack_base = RK816_INT_STS_REG3,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk817_irq_chip = {
	.name = "rk817",
	.irqs = rk817_irqs,
	.num_irqs = ARRAY_SIZE(rk817_irqs),
	.num_regs = 3,
	.irq_reg_stride = 2,
	.status_base = RK817_INT_STS_REG0,
	.mask_base = RK817_INT_STS_MSK_REG0,
	.ack_base = RK817_INT_STS_REG0,
	.init_ack_masked = true,
};

static const struct regmap_irq_chip rk818_irq_chip = {
	.name = "rk818",
	.irqs = rk818_irqs,
	.num_irqs = ARRAY_SIZE(rk818_irqs),
	.num_regs = 2,
	.irq_reg_stride = 2,
	.status_base = RK818_INT_STS_REG1,
	.mask_base = RK818_INT_STS_MSK_REG1,
	.ack_base = RK818_INT_STS_REG1,
	.init_ack_masked = true,
};

static struct device *rk8xx_dev;
static struct rk808_reg_data *suspend_reg, *resume_reg;
static int suspend_reg_num, resume_reg_num;

static int rk808_power_off(struct sys_off_data *data)
{
	struct rk808 *rk808 = data->cb_data;
	int ret;
	unsigned int reg, bit;

	switch (rk808->variant) {
	case RK805_ID:
		reg = RK805_DEV_CTRL_REG;
		bit = DEV_OFF;
		break;
	case RK806_ID:
		reg = RK806_SYS_CFG3;
		bit = DEV_OFF;
		break;
	case RK808_ID:
		reg = RK808_DEVCTRL_REG,
		bit = DEV_OFF_RST;
		break;
	case RK816_ID:
		reg = RK816_DEV_CTRL_REG,
		bit = DEV_OFF;
		break;
	case RK809_ID:
	case RK817_ID:
		reg = RK817_SYS_CFG(3);
		bit = DEV_OFF;
		break;
	case RK816_ID:
	case RK818_ID:
		reg = RK818_DEVCTRL_REG;
		bit = DEV_OFF;
		break;
	default:
		return NOTIFY_DONE;
	}
	ret = regmap_update_bits(rk808->regmap, reg, bit, bit);
	if (ret)
		dev_err(rk808->dev, "Failed to shutdown device!\n");

	return NOTIFY_DONE;
}

static int rk808_restart(struct sys_off_data *data)
{
	struct rk808 *rk808 = data->cb_data;
	unsigned int reg, bit;
	int ret;

	switch (rk808->variant) {
	case RK809_ID:
	case RK817_ID:
		reg = RK817_SYS_CFG(3);
		bit = DEV_RST;
		break;

	default:
		return NOTIFY_DONE;
	}
	ret = regmap_update_bits(rk808->regmap, reg, bit, bit);
	if (ret)
		dev_err(rk808->dev, "Failed to restart device!\n");

	return NOTIFY_DONE;
}

static void rk817_shutdown_prepare(struct device *dev)
{
	int ret;
	struct rk808 *rk808 = dev_get_drvdata(dev);

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK817_INT_STS_MSK_REG0,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK817_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	if (rk808->pins && rk808->pins->p && rk808->pins->power_off) {
		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPIN_FUNC_MSK,
					 SLPPIN_NULL_FUN);
		if (ret)
			pr_err("shutdown: config SLPPIN_NULL_FUN error!\n");

		ret = regmap_update_bits(rk808->regmap,
					 RK817_SYS_CFG(3),
					 RK817_SLPPOL_MSK,
					 RK817_SLPPOL_H);
		if (ret)
			pr_err("shutdown: config RK817_SLPPOL_H error!\n");

		ret = pinctrl_select_state(rk808->pins->p,
					   rk808->pins->power_off);
		if (ret)
			pr_info("%s:failed to activate pwroff state\n",
				__func__);
	}

	/* pmic sleep shutdown function */
	ret = regmap_update_bits(rk808->regmap,
				 RK817_SYS_CFG(3),
				 RK817_SLPPIN_FUNC_MSK, SLPPIN_DN_FUN);
	if (ret)
		dev_err(dev, "Failed to shutdown device!\n");
	/* pmic need the SCL clock to synchronize register */
	mdelay(2);
}

void rk8xx_shutdown(struct device *dev)
{
	struct rk808 *rk808 = dev_get_drvdata(dev);
	int ret = 0;

	if (!rk808) {
		dev_warn(dev,
			 "have no rk808, so do nothing here\n");
		return;
	}

	/* close rtc int when power off */
	regmap_update_bits(rk808->regmap,
			   RK808_INT_STS_MSK_REG1,
			   (0x3 << 5), (0x3 << 5));
	regmap_update_bits(rk808->regmap,
			   RK808_RTC_INT_REG,
			   (0x3 << 2), (0x0 << 2));

	switch (rk808->variant) {
	case RK805_ID:
		ret = regmap_update_bits(rk808->regmap,
					 RK805_GPIO_IO_POL_REG,
					 SLP_SD_MSK,
					 SHUTDOWN_FUN);
		break;
	case RK809_ID:
	case RK817_ID:
		rk817_shutdown_prepare(dev);
		break;
	default:
		return;
	}
	if (ret)
		dev_warn(dev,
			 "Cannot switch to power down function\n");
}
EXPORT_SYMBOL_GPL(rk8xx_shutdown);

static int rk817_pinctrl_init(struct device *dev, struct rk808 *rk808)
{
	int ret;
	struct platform_device	*pinctrl_dev;
	struct pinctrl_state *default_st;

	pinctrl_dev = platform_device_alloc("rk805-pinctrl", -1);
	if (!pinctrl_dev) {
		dev_err(dev, "Alloc pinctrl dev failed!\n");
		return -ENOMEM;
	}

	pinctrl_dev->dev.parent = dev;

	ret = platform_device_add(pinctrl_dev);

	if (ret) {
		platform_device_put(pinctrl_dev);
		dev_err(dev, "Add rk805-pinctrl dev failed!\n");
		return ret;
	}
	if (dev->pins && !IS_ERR(dev->pins->p)) {
		dev_info(dev, "had get a pinctrl!\n");
		return 0;
	}

	rk808->pins = devm_kzalloc(dev, sizeof(struct rk808_pin_info),
				   GFP_KERNEL);
	if (!rk808->pins)
		return -ENOMEM;

	rk808->pins->p = devm_pinctrl_get(dev);
	if (IS_ERR(rk808->pins->p)) {
		rk808->pins->p = NULL;
		dev_err(dev, "no pinctrl handle\n");
		return 0;
	}

	default_st = pinctrl_lookup_state(rk808->pins->p,
					  PINCTRL_STATE_DEFAULT);

	if (IS_ERR(default_st)) {
		dev_dbg(dev, "no default pinctrl state\n");
			return -EINVAL;
	}

	ret = pinctrl_select_state(rk808->pins->p, default_st);
	if (ret) {
		dev_dbg(dev, "failed to activate default pinctrl state\n");
		return -EINVAL;
	}

	rk808->pins->power_off = pinctrl_lookup_state(rk808->pins->p,
						      "pmic-power-off");
	if (IS_ERR(rk808->pins->power_off)) {
		rk808->pins->power_off = NULL;
		dev_dbg(dev, "no power-off pinctrl state\n");
	}

	rk808->pins->sleep = pinctrl_lookup_state(rk808->pins->p,
						  "pmic-sleep");
	if (IS_ERR(rk808->pins->sleep)) {
		rk808->pins->sleep = NULL;
		dev_dbg(dev, "no sleep-setting state\n");
	}

	rk808->pins->reset = pinctrl_lookup_state(rk808->pins->p,
						  "pmic-reset");
	if (IS_ERR(rk808->pins->reset)) {
		rk808->pins->reset = NULL;
		dev_dbg(dev, "no reset-setting pinctrl state\n");
		return 0;
	}

	ret = pinctrl_select_state(rk808->pins->p, rk808->pins->reset);

	if (ret)
		dev_dbg(dev, "failed to activate reset-setting pinctrl state\n");

	return 0;
}

struct rk817_reboot_data_t {
	struct rk808 *rk808;
	struct notifier_block reboot_notifier;
};

static struct rk817_reboot_data_t rk817_reboot_data;

static int rk817_reboot_notifier_handler(struct notifier_block *nb,
					 unsigned long action, void *cmd)
{
	struct rk817_reboot_data_t *data;
	struct device *dev;
	int value, power_en_active0, power_en_active1;
	int ret, i;
	static const char * const pmic_rst_reg_only_cmd[] = {
		"loader", "bootloader", "fastboot", "recovery",
		"ums", "panic", "watchdog", "charge",
	};

	data = container_of(nb, struct rk817_reboot_data_t, reboot_notifier);
	dev = data->rk808->dev;

	regmap_read(data->rk808->regmap, RK817_POWER_EN_SAVE0,
		    &power_en_active0);
	if (power_en_active0 != 0) {
		regmap_read(data->rk808->regmap, RK817_POWER_EN_SAVE1,
			    &power_en_active1);
		value = power_en_active0 & 0x0f;
		regmap_write(data->rk808->regmap,
			     RK817_POWER_EN_REG(0),
			     value | 0xf0);
		value = (power_en_active0 & 0xf0) >> 4;
		regmap_write(data->rk808->regmap,
			     RK817_POWER_EN_REG(1),
			     value | 0xf0);
		value = power_en_active1 & 0x0f;
		regmap_write(data->rk808->regmap,
			     RK817_POWER_EN_REG(2),
			     value | 0xf0);
		value = (power_en_active1 & 0xf0) >> 4;
		regmap_write(data->rk808->regmap,
			     RK817_POWER_EN_REG(3),
			     value | 0xf0);
	} else {
		dev_info(dev, "reboot: not restore POWER_EN\n");
	}

	if (action != SYS_RESTART || !cmd)
		return NOTIFY_OK;

	/*
	 * When system restart, there are two rst actions of PMIC sleep if
	 * board hardware support:
	 *
	 *	0b'00: reset the PMIC itself completely.
	 *	0b'01: reset the 'RST' related register only.
	 *
	 * In the case of 0b'00, PMIC reset itself which triggers SoC NPOR-reset
	 * at the same time, so the command: reboot load/bootload/recovery, etc
	 * is not effect any more.
	 *
	 * Here we check if this reboot cmd is what we expect for 0b'01.
	 */
	for (i = 0; i < ARRAY_SIZE(pmic_rst_reg_only_cmd); i++) {
		if (!strcmp(cmd, pmic_rst_reg_only_cmd[i])) {
			ret = regmap_update_bits(data->rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_RST_FUNC_MSK,
						 RK817_RST_FUNC_REG);
			if (ret)
				dev_err(dev, "reboot: force RK817_RST_FUNC_REG error!\n");
			else
				dev_info(dev, "reboot: force RK817_RST_FUNC_REG ok!\n");
			break;
		}
	}

	return NOTIFY_OK;
}

static void rk817_of_property_prepare(struct rk808 *rk808, struct device *dev)
{
	u32 inner;
	int ret, func, msk, val;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32_index(np, "fb-inner-reg-idxs", 0, &inner);
	if (!ret && inner == RK817_ID_DCDC3)
		regmap_update_bits(rk808->regmap, RK817_POWER_CONFIG,
				   RK817_BUCK3_FB_RES_MSK,
				   RK817_BUCK3_FB_RES_INTER);
	else
		regmap_update_bits(rk808->regmap, RK817_POWER_CONFIG,
				   RK817_BUCK3_FB_RES_MSK,
				   RK817_BUCK3_FB_RES_EXT);
	dev_info(dev, "support dcdc3 fb mode:%d, %d\n", ret, inner);

	ret = of_property_read_u32(np, "pmic-reset-func", &func);

	msk = RK817_SLPPIN_FUNC_MSK | RK817_RST_FUNC_MSK;
	val = SLPPIN_NULL_FUN;

	if (!ret && func < RK817_RST_FUNC_CNT) {
		val |= RK817_RST_FUNC_MSK &
		       (func << RK817_RST_FUNC_SFT);
	} else {
		val |= RK817_RST_FUNC_REG;
	}

	regmap_update_bits(rk808->regmap, RK817_SYS_CFG(3), msk, val);

	dev_info(dev, "support pmic reset mode:%d,%d\n", ret, func);

	rk817_reboot_data.rk808 = rk808;
	rk817_reboot_data.reboot_notifier.notifier_call =
		rk817_reboot_notifier_handler;
	ret = register_reboot_notifier(&rk817_reboot_data.reboot_notifier);
	if (ret)
		dev_err(dev, "failed to register reboot nb\n");
}

static ssize_t rk8xx_dbg_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;
	char cmd;
	u32 input[2], addr, data;
	struct rk808 *rk808 = dev_get_drvdata(rk8xx_dev);

	ret = sscanf(buf, "%c ", &cmd);
	if (ret != 1) {
		pr_err("Unknown command\n");
		goto out;
	}
	switch (cmd) {
	case 'w':
		ret = sscanf(buf, "%c %x %x ", &cmd, &input[0], &input[1]);
		if (ret != 3) {
			pr_err("error! cmd format: echo w [addr] [value]\n");
			goto out;
		};
		addr = input[0] & 0xff;
		data = input[1] & 0xff;
		pr_info("cmd : %c %x %x\n\n", cmd, input[0], input[1]);
		regmap_write(rk808->regmap, addr, data);
		regmap_read(rk808->regmap, addr, &data);
		pr_info("new: %x %x\n", addr, data);
		break;
	case 'r':
		ret = sscanf(buf, "%c %x ", &cmd, &input[0]);
		if (ret != 2) {
			pr_err("error! cmd format: echo r [addr]\n");
			goto out;
		};
		pr_info("cmd : %c %x\n\n", cmd, input[0]);
		addr = input[0] & 0xff;
		regmap_read(rk808->regmap, addr, &data);
		pr_info("%x %x\n", input[0], data);
		break;
	default:
		pr_err("Unknown command\n");
		break;
	}

out:
	return count;
}

static struct kobject *rk8xx_kobj;
static struct device_attribute rk8xx_attrs =
		__ATTR(rk8xx_dbg, 0200, NULL, rk8xx_dbg_store);

int rk8xx_probe(struct device *dev, int variant, unsigned int irq, struct regmap *regmap)
{
	struct rk808 *rk808;
	const struct rk808_reg_data *pre_init_reg;
	const struct regmap_irq_chip *battery_irq_chip = NULL;
	const struct mfd_cell *cells;
	u8 on_source = 0, off_source = 0;
	unsigned int on, off;
	int dual_support = 0;
	int nr_pre_init_regs;
	int nr_cells;

	int ret;
	int i;
	void (*of_property_prepare_fn)(struct rk808 *rk808,
				       struct device *dev) = NULL;
	int (*pinctrl_init)(struct device *dev, struct rk808 *rk808) = NULL;

	rk808 = devm_kzalloc(dev, sizeof(*rk808), GFP_KERNEL);
	if (!rk808)
		return -ENOMEM;
	rk8xx_dev = dev;
	rk808->dev = dev;
	rk808->variant = variant;
	rk808->regmap = regmap;
	dev_set_drvdata(dev, rk808);

	switch (rk808->variant) {
	case RK805_ID:
		rk808->regmap_irq_chip = &rk805_irq_chip;
		pre_init_reg = rk805_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk805_pre_init_reg);
		cells = rk805s;
		nr_cells = ARRAY_SIZE(rk805s);
		on_source = RK805_ON_SOURCE_REG;
		off_source = RK805_OFF_SOURCE_REG;
		suspend_reg = rk805_suspend_reg;
		suspend_reg_num = ARRAY_SIZE(rk805_suspend_reg);
		resume_reg = rk805_resume_reg;
		resume_reg_num = ARRAY_SIZE(rk805_resume_reg);
		break;
	case RK806_ID:
		rk808->regmap_irq_chip = &rk806_irq_chip;
		pre_init_reg = rk806_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk806_pre_init_reg);
		cells = rk806s;
		nr_cells = ARRAY_SIZE(rk806s);
		dual_support = IRQF_SHARED;
		break;
	case RK808_ID:
		rk808->regmap_irq_chip = &rk808_irq_chip;
		pre_init_reg = rk808_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk808_pre_init_reg);
		cells = rk808s;
		nr_cells = ARRAY_SIZE(rk808s);
		break;
	case RK816_ID:
		rk808->regmap_irq_chip = &rk816_irq_chip;
		battery_irq_chip = &rk816_battery_irq_chip;
		pre_init_reg = rk816_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk816_pre_init_reg);
		cells = rk816s;
		nr_cells = ARRAY_SIZE(rk816s);
		on_source = RK816_ON_SOURCE_REG;
		off_source = RK816_OFF_SOURCE_REG;
		suspend_reg = rk816_suspend_reg;
		suspend_reg_num = ARRAY_SIZE(rk816_suspend_reg);
		resume_reg = rk816_resume_reg;
		resume_reg_num = ARRAY_SIZE(rk816_resume_reg);
		break;
	case RK818_ID:
		rk808->regmap_irq_chip = &rk818_irq_chip;
		pre_init_reg = rk818_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk818_pre_init_reg);
		cells = rk818s;
		nr_cells = ARRAY_SIZE(rk818s);
		on_source = RK818_ON_SOURCE_REG;
		off_source = RK818_OFF_SOURCE_REG;
		suspend_reg = rk818_suspend_reg;
		suspend_reg_num = ARRAY_SIZE(rk818_suspend_reg);
		resume_reg = rk818_resume_reg;
		resume_reg_num = ARRAY_SIZE(rk818_resume_reg);
		break;
	case RK809_ID:
	case RK817_ID:
		rk808->regmap_irq_chip = &rk817_irq_chip;
		pre_init_reg = rk817_pre_init_reg;
		nr_pre_init_regs = ARRAY_SIZE(rk817_pre_init_reg);
		cells = rk817s;
		nr_cells = ARRAY_SIZE(rk817s);
		on_source = RK817_ON_SOURCE_REG;
		off_source = RK817_OFF_SOURCE_REG;
		of_property_prepare_fn = rk817_of_property_prepare;
		pinctrl_init = rk817_pinctrl_init;
		break;
	default:
		dev_err(dev, "Unsupported RK8XX ID %lu\n", rk808->variant);
		return -EINVAL;
	}

	if (on_source && off_source) {
		ret = regmap_read(rk808->regmap, on_source, &on);
		if (ret) {
			dev_err(dev, "read 0x%x failed\n", on_source);
			return ret;
		}

		ret = regmap_read(rk808->regmap, off_source, &off);
		if (ret) {
			dev_err(dev, "read 0x%x failed\n", off_source);
			return ret;
		}

		dev_info(dev, "source: on=0x%02x, off=0x%02x\n",
			 on, off);
	}

	if (!irq)
		return dev_err_probe(dev, -EINVAL, "No interrupt support, no core IRQ\n");

	if (of_property_prepare_fn)
		of_property_prepare_fn(rk808, dev);

	for (i = 0; i < nr_pre_init_regs; i++) {
		ret = regmap_update_bits(rk808->regmap,
					pre_init_reg[i].addr,
					pre_init_reg[i].mask,
					pre_init_reg[i].value);
		if (ret) {
			dev_err(dev,
				"0x%x write err\n",
				pre_init_reg[i].addr);
			return ret;
		}
	}

	if (pinctrl_init) {
		ret = pinctrl_init(dev, rk808);
		if (ret)
			return ret;
	}

	ret = devm_regmap_add_irq_chip(dev, rk808->regmap, irq,
				       IRQF_ONESHOT | dual_support, -1,
				       rk808->regmap_irq_chip, &rk808->irq_data);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add irq_chip\n");

	if (battery_irq_chip) {
		ret = devm_regmap_add_irq_chip(dev, rk808->regmap, irq,
					  IRQF_ONESHOT | IRQF_SHARED, -1,
					  battery_irq_chip,
					  &rk808->battery_irq_data);
		if (ret) {
			dev_err(dev,
				"Failed to add batterry irq_chip %d\n", ret);
			regmap_del_irq_chip(irq, rk808->irq_data);
			return ret;
		}
	}

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO, cells, nr_cells, NULL, 0,
			      regmap_irq_get_domain(rk808->irq_data));
	if (ret)
		return dev_err_probe(dev, ret, "failed to add MFD devices\n");

	if (device_property_read_bool(dev, "rockchip,system-power-controller") ||
	    device_property_read_bool(dev, "system-power-controller")) {
		ret = devm_register_sys_off_handler(dev,
				    SYS_OFF_MODE_POWER_OFF_PREPARE, SYS_OFF_PRIO_HIGH,
				    &rk808_power_off, rk808);
		if (ret)
			return dev_err_probe(dev, ret,
					     "failed to register poweroff handler\n");

		switch (rk808->variant) {
		case RK809_ID:
		case RK817_ID:
			ret = devm_register_sys_off_handler(dev,
							    SYS_OFF_MODE_RESTART, SYS_OFF_PRIO_HIGH,
							    &rk808_restart, rk808);
			if (ret)
				dev_warn(dev, "failed to register rst handler, %d\n", ret);
			break;
		default:
			dev_dbg(dev, "pmic controlled board reset not supported\n");
			break;
		}
	}

	rk8xx_kobj = kobject_create_and_add("rk8xx", NULL);
	if (rk8xx_kobj) {
		ret = sysfs_create_file(rk8xx_kobj, &rk8xx_attrs.attr);
		if (ret)
			dev_err(dev, "create rk8xx sysfs error\n");
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rk8xx_probe);

int rk8xx_suspend(struct device *dev)
{
	struct rk808 *rk808 = dev_get_drvdata(dev);
	int i, ret = 0;
	int value;

	for (i = 0; i < suspend_reg_num; i++) {
		ret = regmap_update_bits(rk808->regmap,
					 suspend_reg[i].addr,
					 suspend_reg[i].mask,
					 suspend_reg[i].value);
		if (ret) {
			dev_err(dev, "0x%x write err\n",
				suspend_reg[i].addr);
			return ret;
		}
	}

	switch (rk808->variant) {
	case RK805_ID:
		ret = regmap_update_bits(rk808->regmap,
					 RK805_GPIO_IO_POL_REG,
					 SLP_SD_MSK,
					 SLEEP_FUN);
		break;
	case RK809_ID:
	case RK817_ID:
		if (rk808->pins && rk808->pins->p && rk808->pins->sleep) {
			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPIN_FUNC_MSK,
						 SLPPIN_NULL_FUN);
			if (ret) {
				dev_err(dev, "suspend: config SLPPIN_NULL_FUN error!\n");
				return ret;
			}

			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPOL_MSK,
						 RK817_SLPPOL_H);
			if (ret) {
				dev_err(dev, "suspend: config RK817_SLPPOL_H error!\n");
				return ret;
			}

			/* pmic need the SCL clock to synchronize register */
			regmap_read(rk808->regmap, RK817_SYS_STS, &value);
			mdelay(2);
			ret = pinctrl_select_state(rk808->pins->p, rk808->pins->sleep);
			if (ret) {
				dev_err(dev, "failed to act slp pinctrl state\n");
				return ret;
			}
		}
		break;
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(rk8xx_suspend);

int rk8xx_resume(struct device *dev)
{
	struct rk808 *rk808 = dev_get_drvdata(dev);
	int value;
	int i, ret = 0;

	for (i = 0; i < resume_reg_num; i++) {
		ret = regmap_update_bits(rk808->regmap,
					 resume_reg[i].addr,
					 resume_reg[i].mask,
					 resume_reg[i].value);
		if (ret) {
			dev_err(dev, "0x%x write err\n",
				resume_reg[i].addr);
			return ret;
		}
	}

	switch (rk808->variant) {
	case RK809_ID:
	case RK817_ID:
		if (rk808->pins && rk808->pins->p && rk808->pins->reset) {
			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPIN_FUNC_MSK,
						 SLPPIN_NULL_FUN);
			if (ret) {
				dev_err(dev, "resume: config SLPPIN_NULL_FUN error!\n");
				return ret;
			}

			ret = regmap_update_bits(rk808->regmap,
						 RK817_SYS_CFG(3),
						 RK817_SLPPOL_MSK,
						 RK817_SLPPOL_L);
			if (ret) {
				dev_err(dev, "resume: config RK817_SLPPOL_L error!\n");
				return ret;
			}

			/* pmic need the SCL clock to synchronize register */
			regmap_read(rk808->regmap, RK817_SYS_STS, &value);
			mdelay(2);
			ret = pinctrl_select_state(rk808->pins->p, rk808->pins->reset);
			if (ret)
				dev_dbg(dev, "failed to act reset pinctrl state\n");
		}
		break;
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(rk8xx_resume);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Zhong <zyw@rock-chips.com>");
MODULE_AUTHOR("Zhang Qing <zhangqing@rock-chips.com>");
MODULE_AUTHOR("Wadim Egorov <w.egorov@phytec.de>");
MODULE_DESCRIPTION("RK8xx PMIC core");
