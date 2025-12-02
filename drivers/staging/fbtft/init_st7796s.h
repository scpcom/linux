// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7796S LCD Controller
 *
 * Based on fb_st7789v.c and Arduino ST7796S init sequence
 *
 * Modified by Bhupendra Nagra bhupiister@gmail.com
 */

#ifndef __LINUX_INIT_ST7796S_H
#define __LINUX_INIT_ST7796S_H

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <video/mipi_display.h>

#include "fbtft.h"
#include "fb_st7796s.h"

/**
 * init_st7796s_display() - initialize the display controller
 *
 * @par: FBTFT parameter object
 *
 * ST7796S Initialisation (Adafruit default sequence)
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static inline int init_st7796s_display(struct fbtft_par *par)
{
    par->fbtftops.reset(par);
    mdelay(150);

    /* Manufacturer command unlock (same as Arduino) */
    write_reg(par, CSCON,  0xC3);
    write_reg(par, CSCON,  0x96);

    /* VCOM Control */
    write_reg(par, VCMPCTL,  0x1C);

    /* MADCTL (Memory Access Control) */
    write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,  0x00);   /* MX=0, MY=0, MV=0, BGR=1 */

    /* Pixel Format = 16-bit (RGB565) */
    write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, (MIPI_DCS_PIXEL_FMT_16BIT << 4) | MIPI_DCS_PIXEL_FMT_16BIT);

    /* Interface Mode */
    write_reg(par, IFMODE,  0x80);

    /* Inversion Control */
    write_reg(par, DIC,  0x01);

    /* Display Function Control */
    write_reg(par, DFC,  0x80, 0x02, 0x3B);

    /* Entry Mode */
    write_reg(par, EM,  0xC6);

    /* Manufacturer command lock (after init) */
    write_reg(par, CSCON,  0x69);
    write_reg(par, CSCON,  0x3C);

    /* Exit Sleep */
    write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
    mdelay(150);

    /* Display ON */
    write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
    mdelay(150);

    return 0;
}

#endif
