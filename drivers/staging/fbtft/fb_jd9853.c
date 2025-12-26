// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the JD9853 LCD display controller
 *
 * This display uses 9-bit SPI: Data/Command bit + 8 data bits
 * For platforms that doesn't support 9-bit, the driver is capable
 * of emulating this using 8-bit transfer.
 * This is done by transferring eight 9-bit words in 9 bytes.
 *
 * Copyright (C) 2013 Christian Vogelgsang
 * Based on adafruit22fb.c by Noralf Tronnes
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>

#include "fbtft.h"

#define DRVNAME		"fb_jd9853"
#define WIDTH		172
#define HEIGHT		320

static int init_display(struct fbtft_par *par)
{
	par->fbtftops.reset(par);

	if (par->gpio.cs != -1)
		gpio_set_value(par->gpio.cs, 0);  /* Activate chip */

	// set password to access inhouse register
	write_reg(par, 0xDF, 0x98, 0x53);

	write_reg(par, 0xDF, 0x98, 0x53);

	//set command page
	//page 0 command
	write_reg(par, 0xDE, 0x00);

	//Set VCOM voltage for normal scan direction
	//vcom=-0.3-0.025*value
	write_reg(par, 0xB2, 0x23);

	// Set positive / negative voltage of Gamma power
	write_reg(par, 0xB7, 0x00, 0x47, 0x00, 0x6F);

	//Power mode and charge pump related setting
	write_reg(par, 0xBB, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0);

	// Set Source Output driving ability
	write_reg(par, 0xC0, 0x44, 0xA4);

	//Set Panel relate register
	//- - - SS_PANEL GS_PANEL REV_PANEL CFHR -
	//SS_Panel: Set Source scan output direction /1:S240-> S1  0:S1 -> S240
	//GS_Panel: Set Gate scan output direction /0:Top -> Bottom Scan (G1->G320)  1:Bottom -> Top Scan (G320 -> G1)
	//REV_Panel: Set the display of the same data on both normally-white
	//and normally-black panels. //0:Normal Black 1:Normal White
	//CFHR: Set color fliter horizontial alignment order  /1:BGR  0:RGB
	write_reg(par, 0xC1, 0x12);

	//Set Display Waveform Cycles of RGB Mode
	//- RGB_INV_PI[1:0] RGB_INV_I[1:0] IDLE_TYPE RGB_INV_NP[1:0]
	//RGB_INV_NP[1:0]: Set source dot inversion type at normal or partial mode /01:2-dot 10:Column 00:1-dot
	write_reg(par, 0xC3, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77);

	// Timing control setting
	//- - - - VBFP_RATIO[1:0] TE_OPT[1:0]
	//- TE_DELAY[6:0]
	//LN[7:0] : Sets the gate line number to drive LCD panel.Gate line number = LN*2 / 320  Line
	//SLT_NP[7:0]:Sets the scan line time width. (4 x OSC) CLK / step.Note: fosc = 10MHz
	//- VFP_NP[6:0]
	//VFP_xx[7:0]: Vertical front porch number setting. / 0x0E=60Hz 0x4E=50Hz 7E=45Hz
	write_reg(par, 0xC4, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79,
			 0x0B, 0x0A, 0x16, 0x82);

	//Set Red Gamma output voltage.This command is used to set postive /neagative volatge of source output
	write_reg(par, 0xC8, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
			 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
			 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
			 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00);
	//SET CGOUTx_L Signal Mapping, GS_Panel=0
	write_reg(par, 0xD0, 0x04, 0x06, 0x6B, 0x0F, 0x00);
	//RAMCTRL
	//- CR_OPTION SPI_2LAN_EN RP RM MLBIT_INV DM[1:0]
	//CR_OPTION: for data mapping.used with EPF[1:0]
	//SPI_2LAN_EN: Enable SPI 2 data lane when IM[3:0]=0101  //0=disable
	//RP : Enable DPI data path. 0=disable  1=enable
	//RM : select data path for GRAM. 1=data from DPI/DSI  0=data from 2C/3C command
	//MLBIT_INV: RGB data MSB/LSB reversal(only for MCU Interface RGB565,except QSPI)
	//DM[1:0]: select contol timing and display data path. 00=internal vs, hs ,de;Display Data Path=GRAM
	write_reg(par, 0xD7, 0x00, 0x30);

	write_reg(par, 0xE6, 0x14);

	//set command  page 1 command
	write_reg(par, 0xDE, 0x01);

	write_reg(par, 0xB7, 0x03, 0x13, 0xEF, 0x35, 0x35);
	write_reg(par, 0xC1, 0x14, 0x15, 0xC0);
	write_reg(par, 0xC2, 0x06, 0x3A);
	write_reg(par, 0xC4, 0x72, 0x12);
	write_reg(par, 0xBE, 0x00);

	write_reg(par, 0xDE, 0x02);

	write_reg(par, 0xE5, 0x00, 0x02, 0x00);
	write_reg(par, 0xE5, 0x01, 0x02, 0x00);

	write_reg(par, 0xDE, 0x00);

	write_reg(par, MIPI_DCS_SET_TEAR_OFF);
	write_reg(par, MIPI_DCS_SET_TEAR_SCANLINE,  0x00, 0x00);
	write_reg(par, MIPI_DCS_SET_TEAR_ON, 0x00);
	write_reg(par, MIPI_DCS_SET_TEAR_SCANLINE,  0x00, 0x00);

	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, (MIPI_DCS_PIXEL_FMT_16BIT << 4) | MIPI_DCS_PIXEL_FMT_16BIT);
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,  0x00);
	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS, 0x00, 0x22, 0x00, 0xCD);
	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS, 0x00, 0x00, 0x01, 0x3F);
	write_reg(par, MIPI_DCS_SET_TEAR_ON, 0x00);
	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);

	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	write_reg(par, 0xDE, 0x02);

	write_reg(par, 0xE5, 0x00, 0x02, 0x00);

	write_reg(par, 0xDE, 0x00);

	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	mdelay(20);

	return 0;
}

static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	int _dw = WIDTH;
	int _xo = _dw < 240 ? (240 - _dw) / 2 : 0;
	int _xs = xs + _xo;
	int _xe = xe + _xo;

	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS,
		  (_xs >> 8) & 0xFF, _xs & 0xFF, (_xe >> 8) & 0xFF, _xe & 0xFF);

	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS,
		  (ys >> 8) & 0xFF, ys & 0xFF, (ye >> 8) & 0xFF, ye & 0xFF);

	write_reg(par, MIPI_DCS_SET_TEAR_ON, 0x00);

	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

#define MEM_Y   BIT(7) /* MY row address order */
#define MEM_X   BIT(6) /* MX column address order */
#define MEM_V   BIT(5) /* MV row / column exchange */
#define MEM_L   BIT(4) /* ML vertical refresh order */
#define MEM_H   BIT(2) /* MH horizontal refresh order */
#define MEM_BGR (3) /* RGB-BGR Order */
static int set_var(struct fbtft_par *par)
{
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  (par->bgr << MEM_BGR));
		break;
	case 270:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  MEM_X | MEM_V | (par->bgr << MEM_BGR));
		break;
	case 180:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  MEM_Y | MEM_X |(par->bgr << MEM_BGR));
		break;
	case 90:
		write_reg(par, MIPI_DCS_SET_ADDRESS_MODE,
			  MEM_Y | MEM_V | (par->bgr << MEM_BGR));
		break;
	}

	return 0;
}

static int write_vmem16_bus8(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16;
	__le16 *txbuf16 = par->txbuf.buf;
	size_t remain;
	size_t to_copy;
	size_t tx_array_size;
	int i;
	int ret = 0;
	size_t startbyte_size = 0;

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		      __func__, offset, len);

	remain = len / 2;
	vmem16 = (u16 *)(par->info->screen_buffer + offset);

	if (par->gpio.dc != -1)
		gpio_set_value(par->gpio.dc, 1);

	/* non buffered write */
	if (!par->txbuf.buf)
		return par->fbtftops.write(par, vmem16, len);

	/* buffered write */
	tx_array_size = par->txbuf.len / 2;

	if (par->startbyte) {
		txbuf16 = par->txbuf.buf + 1;
		tx_array_size -= 2;
		*(u8 *)(par->txbuf.buf) = par->startbyte | 0x2;
		startbyte_size = 1;
	}

	while (remain) {
		to_copy = min(tx_array_size, remain);
		dev_dbg(par->info->device, "to_copy=%zu, remain=%zu\n",
			to_copy, remain - to_copy);

		for (i = 0; i < to_copy; i++)
			txbuf16[i] = cpu_to_le16(vmem16[i]);

		vmem16 = vmem16 + to_copy;
		ret = par->fbtftops.write(par, par->txbuf.buf,
						startbyte_size + to_copy * 2);
		if (ret < 0)
			return ret;
		remain -= to_copy;
	}

	return ret;
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = WIDTH,
	.height = HEIGHT,
	.gamma_num = 0,
	.gamma_len = 0,
	.fbtftops = {
		.init_display = init_display,
		.set_addr_win = set_addr_win,
		.set_var = set_var,
		.write_vmem = write_vmem16_bus8,
	},
};

FBTFT_REGISTER_DRIVER(DRVNAME, "jadard,jd9853", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:jd9853");
MODULE_ALIAS("platform:jd9853");

MODULE_DESCRIPTION("FB driver for the JD9853 LCD display controller");
MODULE_AUTHOR("Christian Vogelgsang");
MODULE_LICENSE("GPL");
