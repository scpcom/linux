// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the Sitronix ST7796S LCD Controller
 *
 * Based on fb_st7789v.c and Arduino ST7796S init sequence
 *
 * Modified by Bhupendra Nagra bhupiister@gmail.com
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"
#include "fb_st7796s.h"

#define DRVNAME "fb_st7796s"

#include "init_st7796s.h"

#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_ML BIT(4) /* bitmask for top/bottom refresh */
#define MADCTL_MV BIT(5) /* bitmask for page/column order */
#define MADCTL_MX BIT(6) /* bitmask for column address order */
#define MADCTL_MY BIT(7) /* bitmask for page address order */

/**
 * init_display() - initialize the display controller
 *
 * @par: FBTFT parameter object
 *
 * ST7796S Initialisation (Adafruit default sequence)
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int init_display(struct fbtft_par *par)
{
	return init_st7796s_display(par);
}

/**
 * set_var() - apply LCD properties like rotation and BGR mode
 *
 * @par: FBTFT parameter object
 *
 * Set MADCTL rotation (same orientation rules as Arduino)
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_var(struct fbtft_par *par)
{
	u8 madctl_par = 0;

	if (par->bgr)
		madctl_par |= MADCTL_BGR;
	switch (par->info->var.rotate) {
	case 0:
		madctl_par |= MADCTL_MX;
		break;
	case 90:
		madctl_par |= MADCTL_MV;
		break;
	case 180:
		madctl_par |= MADCTL_MY | MADCTL_ML;
		break;
	case 270:
		madctl_par |= MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_ML;
		break;
	default:
		return -EINVAL;
	}
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

/**
 * blank() - blank the display
 *
 * @par: FBTFT parameter object
 * @on: whether to enable or disable blanking the display
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int blank(struct fbtft_par *par, bool on)
{
	if (on)
		write_reg(par, MIPI_DCS_SET_DISPLAY_OFF);
	else
		write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	return 0;
}

/**
 * Display structure
 */
static struct fbtft_display display = {
	.regwidth = 8,
	.width = 320,
	.height = 480,
	.gamma_num = 0,     /* ST7796S does not require gamma tables */
	.gamma_len = 0,
	.fbtftops = {
		.init_display = init_display,
		.set_var = set_var,
		.blank = blank,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7796s", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7796s");
MODULE_ALIAS("platform:st7796s");

MODULE_DESCRIPTION("FB driver for the ST7796S LCD Controller");
MODULE_AUTHOR("Custom Port");
MODULE_LICENSE("GPL");
