// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7789V LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
 */

#ifndef __LINUX_INIT_MILKV_ST7789V_H
#define __LINUX_INIT_MILKV_ST7789V_H

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"
#include "fb_st7789v.h"

/**
 * init_milkv_st7789v_display() - initialize the display controller
 *
 * @par: FBTFT parameter object
 *
 * Most of the commands in this init function set their parameters to the
 * same default values which are already in place after the display has been
 * powered up. (The main exception to this rule is the pixel format which
 * would default to 18 instead of 16 bit per pixel.)
 * Nonetheless, this sequence can be used as a template for concrete
 * displays which usually need some adjustments.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static inline int init_milkv_st7789v_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);
	mdelay(120);

	/* turn off sleep mode */
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	/* set pixel format to RGB-565 */
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, MIPI_DCS_PIXEL_FMT_16BIT);

	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, 0x00);

	write_reg(par, PORCTRL, 0x0C,0x0C,0x00,0x33,0x33);

	/*
	 * VGH = 13.26V
	 * VGL = -10.43V
	 */
	write_reg(par, GCTRL, 0x35);

	/* VCOM = 0.725V */
	write_reg(par, VCOMS, 0x19);

	/* LCM = Default */
	write_reg(par, LCMCTRL, 0x2C);

	/*
	 * VDV and VRH register values come from command write
	 * (instead of NVM)
	 */
	write_reg(par, VDVVRHEN, 0x01);

	/*
	 * VAP =  4.45V + (VCOM + VCOM offset + 0.5 * VDV)
	 * VAN = -4.45V + (VCOM + VCOM offset + 0.5 * VDV)
	 */
	write_reg(par, VRHS, 0x12);

	/* VDV = 0V */
	write_reg(par, VDVS, 0x20);

	/* FR = 60Hz */
	write_reg(par, FRCTRL2, 0x0F);

	/*
	 * AVDD = 6.8V
	 * AVCL = -4.8V
	 * VDS = 2.3V
	 */
	write_reg(par, PWCTRL1, 0xA4, 0xA1);

	write_reg(par, PVGAMCTRL, 0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23);
	write_reg(par, NVGAMCTRL, 0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23);

	write_reg(par, MIPI_DCS_ENTER_INVERT_MODE);

	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	mdelay(200);

	return 0;
}

#endif
