// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7789V LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
 */

#ifndef __LINUX_INIT_ST7789V_WEACTSTUDIO_H
#define __LINUX_INIT_ST7789V_WEACTSTUDIO_H

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"
#include "fb_st7789v.h"

/**
 * init_st7789v_weactstudio_display() - initialize the display controller
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
static int init_st7789v_weactstudio_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	/* turn off sleep mode */
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	/* set pixel format to RGB-565 */
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, MIPI_DCS_PIXEL_FMT_16BIT);

	write_reg(par, PORCTRL, 0x0c, 0x0c, 0x00, 0x33, 0x33);

	/*
	 * VGH = 13.26V
	 * VGL = -10.43V
	 */
	write_reg(par, GCTRL, 0x35);

	/*
	 * VDV and VRH register values come from command write
	 * (instead of NVM)
	 */
	write_reg(par, VDVVRHEN, 0x01);

	/*
	 * VAP =  4.5V + (VCOM + VCOM offset + 0.5 * VDV)
	 * VAN = -4.5V + (VCOM + VCOM offset + 0.5 * VDV)
	 */
	write_reg(par, VRHS, 0x13);

	/* VDV = 0V */
	write_reg(par, VDVS, 0x20);

	/* VCOM = 1.425V */
	write_reg(par, VCOMS, 0x35);

	/* VCOM offset = 0V */
	//write_reg(par, VCMOFSET, 0x20);

	/* XOR RGB setting in command 36h */
	write_reg(par, LCMCTRL, 0x0c);

	/* FR = 60Hz */
	write_reg(par, FRCTRL2, 0x0f);

	write_reg(par, REGSEL2, 0x0f);
	write_reg(par, REGSEL1, 0x08);

	/*
	 * Color Enhancement On
	 * Medium enhancement
	 */
	write_reg(par, WRCACE, 0x90);

	/*
	 * AVDD = 6.8V
	 * AVCL = -4.8V
	 * VDS = 2.3V
	 */
	write_reg(par, PWCTRL1, 0xA4, 0xA1);

	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, 0x00); // Memory Access Control

	write_reg(par, PVGAMCTRL, 0xd0,0x00,0x06,0x09,0x0b,0x2a,0x3c,0x55,0x4b,0x08,0x16,0x14,0x19,0x20);
	write_reg(par, NVGAMCTRL, 0xd0,0x00,0x06,0x09,0x0b,0x29,0x36,0x54,0x4b,0x0d,0x16,0x14,0x21,0x20);

	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);

	return 0;
}

#endif
