// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7796S LCD Controller
 *
 * Based on fb_st7789v.c and Arduino ST7796S init sequence
 *
 * Modified by Bhupendra Nagra bhupiister@gmail.com
 */

#ifndef __LINUX_FB_ST7796S_H
#define __LINUX_FB_ST7796S_H

/**
 * enum st7796s_command - ST7796S display controller commands
 *
 * @WRCABC: Write Adaptive Brightness Control
 * @IFMODE: Interface Mode Control
 * @FRMCTR1: Frame Rate Control (In Normal Mode/Full Colors)
 * @FRMCTR2: Frame Rate Control 2 (In Idle Mode/8 Colors)
 * @FRMCTR3: Frame Rate Control 3 (In Partial Mode/Full Colors)
 * @DIC: Display Inversion Control
 * @BPC: Blanking Porch Control
 * @DFC: Display Function Control
 * @EM: Entry Mode Set
 * @PWR1: Power Control 1
 * @PWR2: Power Control 2
 * @PWR3: Power Control 3
 * @VCMPCTL: VCOM Control
 * @VCMOFFSET: VCOM Offset Register
 * @PGC: Positive Gamma Control
 * @NGC: Negative Gamma Control
 * @DOCA: Display Output Ctrl Adjust
 * @CSCON: Command Set Control
 *
 * The command names are the same as those found in the datasheet to ease
 * looking up their semantics and usage.
 *
 * Note that the ST7796S display controller offers quite a few more commands
 * which have been omitted from this list as they are not used at the moment.
 * Furthermore, commands that are compliant with the MIPI DCS have been left
 * out as well to avoid duplicate entries.
 */
enum st7796s_command {
	WRCABC = 0x55,
	IFMODE = 0xB0,
	FRMCTR1 = 0xB1,
	FRMCTR2 = 0xB2,
	FRMCTR3 = 0xB3,
	DIC = 0xB4,
	BPC = 0xB5,
	DFC = 0xB6,
	EM = 0xB7,
	PWR1 = 0xC0,
	PWR2 = 0xC1,
	PWR3 = 0xC2,
	VCMPCTL = 0xC5,
	VCMOFFSET = 0xC6,
	PGC = 0xE0,
	NGC = 0xE1,
	DOCA = 0xE8,
	CSCON = 0xF0,
};

#endif
