// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7789V LCD Controller
 *
 * Copyright (C) 2015 Dennis Menschel
 */

#ifndef __LINUX_FB_ST7789V_H
#define __LINUX_FB_ST7789V_H

/**
 * enum st7789v_command - ST7789V display controller commands
 *
 * @WRCACE: write content adaptive brightness control and color enhancement
 * @PORCTRL: porch setting
 * @GCTRL: gate control
 * @VCOMS: VCOM setting
 * @LCMCTRL: LCM control
 * @VDVVRHEN: VDV and VRH command enable
 * @VRHS: VRH set
 * @VDVS: VDV set
 * @VCMOFSET: VCOM offset set
 * @FRCTRL2: frame rate control in normal mode
 * @REGSEL1: register value selection 1
 * @REGSEL2: register value selection 2
 * @PWCTRL1: power control 1
 * @PVGAMCTRL: positive voltage gamma control
 * @NVGAMCTRL: negative voltage gamma control
 * @PWCTRL2: power control 2
 * @EQCTRL: Equalize time control
 *
 * The command names are the same as those found in the datasheet to ease
 * looking up their semantics and usage.
 *
 * Note that the ST7789V display controller offers quite a few more commands
 * which have been omitted from this list as they are not used at the moment.
 * Furthermore, commands that are compliant with the MIPI DCS have been left
 * out as well to avoid duplicate entries.
 */
enum st7789v_command {
	WRCACE = 0x55,
	PORCTRL = 0xB2,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	LCMCTRL = 0xC0,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	FRCTRL2 = 0xC6,
	REGSEL1 = 0xC8,
	REGSEL2 = 0xCA,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
	PWCTRL2 = 0xE8,
	EQCTRL = 0xE9,
};

#endif
