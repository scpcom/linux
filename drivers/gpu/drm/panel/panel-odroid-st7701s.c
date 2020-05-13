/*----------------------------------------------------------------------------*/
/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2019, Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 *
 */
/*----------------------------------------------------------------------------*/
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/of_device.h>

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
/*----------------------------------------------------------------------------*/
/* Command2 BKx selection command */
/*----------------------------------------------------------------------------*/
#define DSI_CMD2BKX_SEL			0xFF

/*----------------------------------------------------------------------------*/
/* Command2, BK0 commands */
/*----------------------------------------------------------------------------*/
#define DSI_CMD2_BK0_PVGAMCTRL		0xB0 /* Positive Voltage Gamma Control */
#define DSI_CMD2_BK0_NVGAMCTRL		0xB1 /* Negative Voltage Gamma Control */
#define DSI_CMD2_BK0_LNESET		0xC0 /* Display Line setting */
#define DSI_CMD2_BK0_PORCTRL		0xC1 /* Porch control */
#define DSI_CMD2_BK0_INVSEL		0xC2 /* Inversion selection, Frame Rate Control */

/*----------------------------------------------------------------------------*/
/* Command2, BK1 commands */
/*----------------------------------------------------------------------------*/
#define DSI_CMD2_BK1_VRHS		0xB0 /* Vop amplitude setting */
#define DSI_CMD2_BK1_VCOM		0xB1 /* VCOM amplitude setting */
#define DSI_CMD2_BK1_VGHSS		0xB2 /* VGH Voltage setting */
#define DSI_CMD2_BK1_TESTCMD		0xB3 /* TEST Command Setting */
#define DSI_CMD2_BK1_VGLS		0xB5 /* VGL Voltage setting */
#define DSI_CMD2_BK1_PWCTLR1		0xB7 /* Power Control 1 */
#define DSI_CMD2_BK1_PWCTLR2		0xB8 /* Power Control 2 */
#define DSI_CMD2_BK1_SPD1		0xC1 /* Source pre_drive timing set1 */
#define DSI_CMD2_BK1_SPD2		0xC2 /* Source EQ2 Setting */
#define DSI_CMD2_BK1_MIPISET1		0xD0 /* MIPI Setting 1 */

/*----------------------------------------------------------------------------*/
/**
 * Command2 with BK function selection.
 *
 * BIT[4, 0]: [CN2, BKXSEL]
 * 10 = CMD2BK0, Command2 BK0
 * 11 = CMD2BK1, Command2 BK1
 * 00 = Command2 disable
 */
/*----------------------------------------------------------------------------*/
#define DSI_CMD2BK1_SEL			0x11
#define DSI_CMD2BK0_SEL			0x10
#define DSI_CMD2BKX_SEL_NONE		0x00

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
struct st7701_panel_desc {
	const struct drm_display_mode *mode;
	unsigned int lanes;
	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	unsigned int panel_sleep_delay;
};

struct st7701 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct st7701_panel_desc *desc;

	struct backlight_device *backlight;
	struct regulator *supply;
	int reset_gpio;
	unsigned int sleep_delay;
};

/*----------------------------------------------------------------------------*/
static inline struct st7701 *panel_to_st7701(struct drm_panel *panel)
{
	return container_of(panel, struct st7701, panel);
}

/*----------------------------------------------------------------------------*/
static inline int st7701_dsi_write(struct st7701 *st7701, const void *seq,
				   size_t len)
{
	#if (0)	/* for cmd debug */
	{
		int cnt, pos;
		char *cmdp = (char *)seq, cmdstr[256];

		memset(cmdstr, 0x00, sizeof(cmdstr));

		pos = sprintf(&cmdstr[0], "[cmd : %02X] ", cmdp[0]);

		for(cnt = 1; cnt < len; cnt++)
			pos += sprintf(&cmdstr[pos], "%02X ", cmdp[cnt]);

		pr_err("%s\n", cmdstr);
	}
	#endif

	return mipi_dsi_dcs_write_buffer(st7701->dsi, seq, len);
}

/*----------------------------------------------------------------------------*/
#define ST7701_DSI(st7701, seq...)				\
	{							\
		const u8 d[] = { seq };				\
		st7701_dsi_write(st7701, d, ARRAY_SIZE(d));	\
	}

/*----------------------------------------------------------------------------*/
static void st7701_init_sequence(struct st7701 *st7701)
{
	/*-----------------------ST7701S Reset Sequence-----------------------*/
	ST7701_DSI(st7701, MIPI_DCS_SOFT_RESET, 0x00);

	/* We need to wait 5ms before sending new commands */
	msleep(5);

	ST7701_DSI(st7701, MIPI_DCS_EXIT_SLEEP_MODE, 0x00);
	msleep(st7701->sleep_delay);

	/*---------------------------Bank0 Setting----------------------------*/
	/*----------------------Display Control setting-----------------------*/
	ST7701_DSI(st7701, DSI_CMD2BKX_SEL, 0x77, 0x01, 0x00, 0x00, 0x10 );
	ST7701_DSI(st7701, DSI_CMD2_BK0_LNESET, 0xE9, 0x03);
	ST7701_DSI(st7701, DSI_CMD2_BK0_PORCTRL ,0x11, 0x02);
	ST7701_DSI(st7701, DSI_CMD2_BK0_INVSEL, 0x31, 0x08);
	ST7701_DSI(st7701, 0xCC, 0x10);
	/*-----------------------Gamma Cluster Setting------------------------*/
	ST7701_DSI(st7701, DSI_CMD2_BK0_PVGAMCTRL,
		     0x00, 0x0D, 0x14, 0x0D, 0x10, 0x05, 0x02, 0x08, 0x08,
		     0x1E, 0x05, 0x13, 0x11, 0xA3, 0x29, 0x18);
	ST7701_DSI(st7701, DSI_CMD2_BK0_NVGAMCTRL,
		     0x00, 0x0C, 0x14, 0x0C, 0x10, 0x05, 0x03, 0x08, 0x07,
		     0x20, 0x05, 0x13, 0x11, 0xA4, 0x29, 0x18);
	/*-------------------------End Gamma Setting--------------------------*/
	/*--------------------End Display Control setting---------------------*/
	/*-------------------------Bank0 Setting  End-------------------------*/
	/*---------------------------Bank1 Setting----------------------------*/
	/*----------------- Power Control Registers Initial ------------------*/
	ST7701_DSI(st7701, DSI_CMD2BKX_SEL,
		     0x77, 0x01, 0x00, 0x00, DSI_CMD2BK1_SEL);
	ST7701_DSI(st7701, DSI_CMD2_BK1_VRHS, 0x6C);
	/*----------------------------Vcom Setting----------------------------*/
	ST7701_DSI(st7701, DSI_CMD2_BK1_VCOM, 0x43);
	/*--------------------------End Vcom Setting--------------------------*/
	ST7701_DSI(st7701, DSI_CMD2_BK1_VGHSS, 0x07);
	ST7701_DSI(st7701, DSI_CMD2_BK1_TESTCMD, 0x80);
	ST7701_DSI(st7701, DSI_CMD2_BK1_VGLS, 0x47);
	ST7701_DSI(st7701, DSI_CMD2_BK1_PWCTLR1, 0x85);
	ST7701_DSI(st7701, DSI_CMD2_BK1_PWCTLR2, 0x20);
	ST7701_DSI(st7701, 0xB9, 0x10);
	ST7701_DSI(st7701, DSI_CMD2_BK1_SPD1, 0x78);
	ST7701_DSI(st7701, DSI_CMD2_BK1_SPD2, 0x78);
	ST7701_DSI(st7701, DSI_CMD2_BK1_MIPISET1, 0x88);

	/*----------------End Power Control Registers Initial ----------------*/
	msleep(st7701->sleep_delay);
	/*----------------------------GIP Setting-----------------------------*/
	ST7701_DSI(st7701, 0xE0, 0x00, 0x00, 0x02);
	ST7701_DSI(st7701, 0xE1, 0x08, 0x00, 0x0A, 0x00, 0x07, 0x00, 0x09,
		     0x00, 0x00, 0x33, 0x33);
	ST7701_DSI(st7701, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		     0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701_DSI(st7701, 0xE3, 0x00, 0x00, 0x33, 0x33);
	ST7701_DSI(st7701, 0xE4, 0x44, 0x44);
	ST7701_DSI(st7701, 0xE5, 0x0E, 0x60, 0xA0, 0xA0, 0x10, 0x60, 0xA0,
		     0xA0, 0x0A, 0x60, 0xA0, 0xA0, 0x0C, 0x60, 0xA0, 0xA0);
	ST7701_DSI(st7701, 0xE6, 0x00, 0x00, 0x33, 0x33);
	ST7701_DSI(st7701, 0xE7, 0x44, 0x44);
	ST7701_DSI(st7701, 0xE8, 0x0D, 0x60, 0xA0, 0xA0, 0x0F, 0x60, 0xA0,
		     0xA0 ,0x09, 0x60, 0xA0, 0xA0, 0x0B, 0x60, 0xA0, 0xA0);
	ST7701_DSI(st7701, 0xEB, 0x02, 0x01, 0xE4, 0xE4, 0x44, 0x00, 0x40);
	ST7701_DSI(st7701, 0xEC, 0x02, 0x01);
	ST7701_DSI(st7701, 0xED, 0xAB, 0x89, 0x76, 0x54, 0x01, 0xFF, 0xFF,
		     0xFF ,0xFF, 0xFF, 0xFF, 0x10, 0x45, 0x67, 0x98, 0xBA);
	/*--------------------------End GIP Setting---------------------------*/
	/*---------------- Power Control Registers Initial End----------------*/
	/*---------------------------Bank1 Setting----------------------------*/
	ST7701_DSI(st7701, DSI_CMD2BKX_SEL,
		     0x77, 0x01, 0x00, 0x00, DSI_CMD2BKX_SEL_NONE);
}

/*----------------------------------------------------------------------------*/
static int st7701_prepare(struct drm_panel *panel)
{
	struct st7701 *st7701 = panel_to_st7701(panel);
	int ret;

	gpio_set_value(st7701->reset_gpio, 0);

	ret = regulator_enable(st7701->supply);
	if (ret < 0) {
		dev_err(panel->dev, "failed to enable supply: %d\n",
			ret);
		return ret;
	}
	msleep(20);

	gpio_set_value(st7701->reset_gpio, 1);
	msleep(150);

	st7701_init_sequence(st7701);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int st7701_enable(struct drm_panel *panel)
{
	struct st7701 *st7701 = panel_to_st7701(panel);

	ST7701_DSI(st7701, MIPI_DCS_SET_DISPLAY_ON, 0x00);
	backlight_enable(st7701->backlight);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int st7701_disable(struct drm_panel *panel)
{
	struct st7701 *st7701 = panel_to_st7701(panel);

	backlight_disable(st7701->backlight);
	ST7701_DSI(st7701, MIPI_DCS_SET_DISPLAY_OFF, 0x00);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int st7701_unprepare(struct drm_panel *panel)
{
	struct st7701 *st7701 = panel_to_st7701(panel);

	ST7701_DSI(st7701, MIPI_DCS_ENTER_SLEEP_MODE, 0x00);

	msleep(st7701->sleep_delay);

	gpio_set_value(st7701->reset_gpio, 0);

	/**
	 * During the Resetting period, the display will be blanked
	 * (The display is entering blanking sequence, which maximum
	 * time is 120 ms, when Reset Starts in Sleep Out ?mode. The
	 * display remains the blank state in Sleep In ?mode.) and
	 * then return to Default condition for Hardware Reset.
	 *
	 * So we need wait sleep_delay time to make sure reset completed.
	 */
	msleep(st7701->sleep_delay);

	regulator_disable(st7701->supply);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int st7701_get_modes(struct drm_panel *panel)
{
	struct st7701 *st7701 = panel_to_st7701(panel);
	const struct drm_display_mode *desc_mode = st7701->desc->mode;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, desc_mode);
	if (!mode) {
		dev_err(&st7701->dsi->dev, "failed to add mode %ux%ux@%u\n",
			desc_mode->hdisplay, desc_mode->vdisplay,
			desc_mode->vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = desc_mode->width_mm;
	panel->connector->display_info.height_mm = desc_mode->height_mm;

	return 1;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct drm_panel_funcs st7701_funcs = {
	.disable	= st7701_disable,
	.unprepare	= st7701_unprepare,
	.prepare	= st7701_prepare,
	.enable		= st7701_enable,
	.get_modes	= st7701_get_modes,
};

static const struct drm_display_mode st7701s_mode = {
	.clock		= 27500,

	.hdisplay	= 480,
	.hsync_start	= 480 + 38,
	.hsync_end	= 480 + 38 + 12,
	.htotal		= 480 + 38 + 12 + 12,

	.vdisplay	= 854,
	.vsync_start	= 854 + 18,
	.vsync_end	= 854 + 18 + 8,
	.vtotal		= 854 + 18 + 8 + 4,

	.width_mm	= 69,
	.height_mm	= 139,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static const struct st7701_panel_desc st7701s_desc = {
	.mode = &st7701s_mode,
	.lanes = 2,
	.flags = MIPI_DSI_MODE_VIDEO,
	.format = MIPI_DSI_FMT_RGB888,
	.panel_sleep_delay = 80, /* panel need extra 80ms for sleep out cmd */
};

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int st7701_dsi_probe(struct mipi_dsi_device *dsi)
{
	const struct st7701_panel_desc *desc;
	struct device_node *np;
	struct st7701 *st7701;
	struct device *dev = &dsi->dev;
	enum of_gpio_flags flags;
	int ret;

	st7701 = devm_kzalloc(&dsi->dev, sizeof(*st7701), GFP_KERNEL);
	if (!st7701)
		return -ENOMEM;

	desc = of_device_get_match_data(&dsi->dev);
	dsi->mode_flags = desc->flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

	st7701->supply = devm_regulator_get(dev, "vcc");
	if (IS_ERR(st7701->supply)) {
		ret = PTR_ERR(st7701->supply);
		dev_err(dev, "failed to get power regulator: %d\n", ret);
		return ret;
	}

	st7701->reset_gpio = of_get_named_gpio_flags(dev->of_node,
				"reset-gpios", 0, &flags);

	if (gpio_is_valid(st7701->reset_gpio)) {
		ret = devm_gpio_request(dev, st7701->reset_gpio,
						"reset-gpios");
		if (ret < 0) {
			DRM_DEV_ERROR(dev, "failed to request reset-gpios %d\n",
				st7701->reset_gpio);
			return ret;
		}
		ret = gpio_direction_output(st7701->reset_gpio,	0);
	}

	np = of_parse_phandle(dsi->dev.of_node, "backlight", 0);
	if (np) {
		st7701->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (!st7701->backlight)
			return -EPROBE_DEFER;
	}

	drm_panel_init(&st7701->panel);

	/* We need to wait 120ms after a sleep out command */
	st7701->sleep_delay = 120 + desc->panel_sleep_delay;
	st7701->panel.funcs = &st7701_funcs;
	st7701->panel.dev = &dsi->dev;

	ret = drm_panel_add(&st7701->panel);
	if (ret < 0)
		return ret;

	mipi_dsi_set_drvdata(dsi, st7701);
	st7701->dsi = dsi;
	st7701->desc = desc;

	return mipi_dsi_attach(dsi);
}

/*----------------------------------------------------------------------------*/
static int st7701_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct st7701 *st7701 = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&st7701->panel);

	if (st7701->backlight)
		put_device(&st7701->backlight->dev);

	return 0;
}

/*----------------------------------------------------------------------------*/
static const struct of_device_id st7701_of_match[] = {
	{ .compatible = "odroid,st7701s", .data = &st7701s_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, st7701_of_match);

static struct mipi_dsi_driver st7701_dsi_driver = {
	.probe		= st7701_dsi_probe,
	.remove		= st7701_dsi_remove,
	.driver = {
		.name		= "st7701",
		.of_match_table	= st7701_of_match,
	},
};
module_mipi_dsi_driver(st7701_dsi_driver);

MODULE_AUTHOR("Jagan Teki <jagan@amarulasolutions.com>");
MODULE_AUTHOR("Hardkernel Co.,ltd");
MODULE_DESCRIPTION("ODROID ST7701 LCD Panel Driver");
MODULE_LICENSE("GPL");

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
