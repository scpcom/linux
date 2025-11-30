// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7789 LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
 */

#ifndef __LINUX_INIT_ST7789_H
#define __LINUX_INIT_ST7789_H

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"
#include "fb_st7789v.h"

/**
 * init_st7789_display() - initialize the display controller
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
static int init_st7789_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);
	mdelay(120);

	/* turn off sleep mode */
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, 0x00);

	/* set control interface to 18bit/pixel and pixel format to RGB-565 */
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, (MIPI_DCS_PIXEL_FMT_18BIT << 4) | MIPI_DCS_PIXEL_FMT_16BIT);

	write_reg(par, PORCTRL, 0x0C, 0x0C, 0x00, 0x33, 0x33);

	/*
	 * VGH = 13.26V
	 * VGL = -10.43V
	 */
	write_reg(par, GCTRL, 0x35);

	/* VCOM = 1.125V */
	write_reg(par, VCOMS, 0x29);

	/*
	 * VDV and VRH register values come from command write
	 * (instead of NVM)
	 */
	write_reg(par, VDVVRHEN, 0x01);

	/*
	 * VAP =  4.8V + (VCOM + VCOM offset + 0.5 * VDV)
	 * VAN = -4.8V + (VCOM + VCOM offset + 0.5 * VDV)
	 */
	write_reg(par, VRHS, 0x19);

	/* VDV = 0V */
	write_reg(par, VDVS, 0x20);

	/* VCOM offset = -0.15V */
	write_reg(par, VCMOFSET, 0x1A);

	/* FR = 39Hz */
	write_reg(par, FRCTRL2, 0x1F);

	/*
	 * AVDD = 6.8V
	 * AVCL = -4.8V
	 * VDS = 2.3V
	 */
	write_reg(par, PWCTRL1, 0xA4, 0xA1);

	write_reg(par, PVGAMCTRL, 0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34);
	write_reg(par, NVGAMCTRL, 0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31, 0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34);

	write_reg(par, MIPI_DCS_ENTER_INVERT_MODE);

	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	mdelay(200);

	return 0;
}

#endif
