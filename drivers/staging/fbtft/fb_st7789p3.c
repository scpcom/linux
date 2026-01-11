// SPDX-License-Identifier: GPL-2.0+
/*
 * FB driver for the ST7789P3 LCD Controller
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/module.h>

#include <video/mipi_display.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/printk.h>

#include "fbtft.h"

#ifndef dev_err_probe
#define dev_err_probe(dev, err, fmt, ...)              \
    ({                                                 \
        int __ret = (err);                            \
        if (__ret == -EPROBE_DEFER)                    \
            dev_dbg(dev, fmt, ##__VA_ARGS__);         \
        else                                          \
            dev_err(dev, fmt, ##__VA_ARGS__);         \
        __ret;                                        \
    })
#endif


#define DRVNAME "fb_st7789p3"

#define DEFAULT_GAMMA                                 \
	"F0 05 0A 06 06 03 2B 32 43 36 11 10 2B 32\n" \
	"F0 08 0C 0B 09 24 2B 22 43 38 15 16 2F 37"

#define HSD20_IPS_GAMMA                               \
	"D0 05 0A 09 08 05 2E 44 45 0F 17 16 2B 33\n" \
	"D0 05 0A 09 08 05 2E 43 45 0F 16 16 2B 33"

#define HSD20_IPS 0

/**
 * enum st7789p3_command - ST7789P3 display controller commands
 *
 * @PORCTRL: porch setting
 * @GCTRL: gate control
 * @VCOMS: VCOM setting
 * @VDVVRHEN: VDV and VRH command enable
 * @VRHS: VRH set
 * @VDVS: VDV set
 * @VCMOFSET: VCOM offset set
 * @PWCTRL1: power control 1
 * @PVGAMCTRL: positive voltage gamma control
 * @NVGAMCTRL: negative voltage gamma control
 *
 * The command names are the same as those found in the datasheet to ease
 * looking up their semantics and usage.
 *
 * Note that the ST7789P3 display controller offers quite a few more commands
 * which have been omitted from this list as they are not used at the moment.
 * Furthermore, commands that are compliant with the MIPI DCS have been left
 * out as well to avoid duplicate entries.
 */
enum st7789p3_command {
	PORCTRL = 0xB2,
	GCTRL = 0xB7,
	VCOMS = 0xBB,
	VDVVRHEN = 0xC2,
	VRHS = 0xC3,
	VDVS = 0xC4,
	VCMOFSET = 0xC5,
	PWCTRL1 = 0xD0,
	PVGAMCTRL = 0xE0,
	NVGAMCTRL = 0xE1,
};

#define MADCTL_BGR BIT(3) /* bitmask for RGB/BGR order */
#define MADCTL_MV BIT(5) /* bitmask for page/column order */
#define MADCTL_MX BIT(6) /* bitmask for column address order */
#define MADCTL_MY BIT(7) /* bitmask for page address order */

/* 60Hz for 16.6ms, configured as 2*16.6ms */
#define PANEL_TE_TIMEOUT_MS 33

static struct completion panel_te; /* completion for panel TE line */
static int irq_te; /* Linux IRQ for LCD TE line */

static irqreturn_t panel_te_handler(int irq, void *data)
{
	complete(&panel_te);
	return IRQ_HANDLED;
}

/*
 * init_tearing_effect_line() - init tearing effect line.
 * @par: FBTFT parameter object.
 *
 * Return: 0 on success, or a negative error code otherwise.
 */
static int init_tearing_effect_line(struct fbtft_par *par)
{
	struct device *dev = par->info->device;
	struct gpio_desc *te;
	int rc, irq;

	te = gpiod_get_optional(dev, "te", GPIOD_IN);
	if (IS_ERR(te))
		return dev_err_probe(dev, PTR_ERR(te),
				     "Failed to request te GPIO\n");

	/* if te is NULL, indicating no configuration, directly return success */
	if (!te) {
		irq_te = 0;
		return 0;
	}

	irq = gpiod_to_irq(te);

	/* GPIO is locked as an IRQ, we may drop the reference */
	gpiod_put(te);

	if (irq < 0)
		return irq;

	irq_te = irq;
	init_completion(&panel_te);

	/* The effective state is high and lasts no more than 1000 microseconds */
	rc = devm_request_irq(dev, irq_te, panel_te_handler,
			      IRQF_TRIGGER_RISING, "TE_GPIO", par);
	if (rc)
		return dev_err_probe(dev, rc, "TE IRQ request failed.\n");

	disable_irq_nosync(irq_te);

	return 0;
}

/**
 * init_display() - initialize the display controller
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
static int init_display(struct fbtft_par *par)
{
	int rc;

	par->fbtftops.reset(par);

	rc = init_tearing_effect_line(par);
	if (rc)
		return rc;

#if 1
	write_reg(par, 0x11);
	mdelay(120);

	write_reg(par, 0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33);
	write_reg(par, 0xB0, 0x00, 0xE0);
	write_reg(par, 0x36, 0x00);
	write_reg(par, 0x3A, 0x05);

	/*
     * Write(Command, 0xB7);
     * Write(Parameter, 0x45);
     */
	write_reg(par, 0xB7, 0x45);

	/*
        Write(Command , 0xBB);     
        Write(Parameter , 0x1D);   
    */
	write_reg(par, 0xBB, 0x1D);

	/*
        Write(Command , 0xC0);     
        Write(Parameter , 0x2C);   
    */
	write_reg(par, 0xC0, 0x2C);

	/*
        Write(Command , 0xC2);     
        Write(Parameter , 0x01);   
    */
	write_reg(par, 0xC2, 0x01);

	/*
        Write(Command , 0xC3);     
        Write(Parameter , 0x19);   
    */
	write_reg(par, 0xC3, 0x19);

	/*
        Write(Command , 0xC4);     
        Write(Parameter , 0x20);   
    */
	write_reg(par, 0xC4, 0x20);

	/*
        Write(Command , 0xC6);     
        Write(Parameter , 0x0F);   
    */
	write_reg(par, 0xC6, 0x0F);

	/*
        Write(Command , 0xD0);     
        Write(Parameter , 0xA4);   
        Write(Parameter , 0xA1);   
    */
	write_reg(par, 0xD0, 0xA4, 0xA1);

	/*
        Write(Command , 0xD6);     
        Write(Parameter , 0xA1);   
    */
	write_reg(par, 0xD6, 0xA1);

	/*
        Write(Command , 0xE0);     
        Write(Parameter , 0xD0);
        Write(Parameter , 0x10);
        Write(Parameter , 0x21);
        Write(Parameter , 0x14);
        Write(Parameter , 0x15);
        Write(Parameter , 0x2D);
        Write(Parameter , 0x41);
        Write(Parameter , 0x44);
        Write(Parameter , 0x4F);
        Write(Parameter , 0x28);
        Write(Parameter , 0x0E);
        Write(Parameter , 0x0C);
        Write(Parameter , 0x1D);
        Write(Parameter , 0x1F);
    */
	write_reg(par, 0xE0, 0xD0, 0x10, 0x21, 0x14, 0x15, 0x2D, 0x41, 0x44,
		  0x4F, 0x28, 0x0E, 0x0C, 0x1D, 0x1F);

	/*
        Write(Command , 0xE1);     
        Write(Parameter , 0xD0);
        Write(Parameter , 0x0F);
        Write(Parameter , 0x1B);
        Write(Parameter , 0x0D);
        Write(Parameter , 0x0D);
        Write(Parameter , 0x26);
        Write(Parameter , 0x42);
        Write(Parameter , 0x54);
        Write(Parameter , 0x50);
        Write(Parameter , 0x3E);
        Write(Parameter , 0x1A);
        Write(Parameter , 0x18);
        Write(Parameter , 0x22);
        Write(Parameter , 0x25);
    */
	write_reg(par, 0xE1, 0xD0, 0x0F, 0x1B, 0x0D, 0x0D, 0x26, 0x42, 0x54,
		  0x50, 0x3E, 0x1A, 0x18, 0x22, 0x25);

	/*
        Write(Command , 0x29);  
    */
	write_reg(par, 0x29);

	// /*
	//        Write(Command , 0x2A);
	//        Write(Parameter , 0x00);
	//        Write(Parameter , 0x52);
	//        Write(Parameter , 0x00);
	//        Write(Parameter , 0x9D);
	//     */
	// write_reg(par, 0x2A, 0x00, 0x52, 0x00, 0x9D);
	//
	// /*
	//        Write(Command , 0x2B);
	//        Write(Parameter , 0x00);
	//        Write(Parameter , 0x12);
	//        Write(Parameter , 0x01);
	//        Write(Parameter , 0x2D);
	//     */
	// write_reg(par, 0x2B, 0x00, 0x12, 0x01, 0x2D);
	//
	// /*
	//        Write(Command , 0x2C);
	//     */
	// write_reg(par, 0x2C);

	write_reg(par, 0x11);
	mdelay(120);
	write_reg(par, 0x29);
#else
	/* turn off sleep mode */
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	/* set pixel format to RGB-565 */
	write_reg(par, MIPI_DCS_SET_PIXEL_FORMAT, MIPI_DCS_PIXEL_FMT_16BIT);
	if (HSD20_IPS)
		write_reg(par, PORCTRL, 0x05, 0x05, 0x00, 0x33, 0x33);

	else
		write_reg(par, PORCTRL, 0x0B, 0x0B, 0x00, 0x33, 0x33);

	/*
	 * VGH = 13.26V
	 * VGL = -10.43V
	 */
	write_reg(par, GCTRL, 0x75);

	write_reg(par, 0xC0, 0x2C);

	/*
	 * VDV and VRH register values come from command write
	 * (instead of NVM)
	 */
	write_reg(par, VDVVRHEN, 0x01, 0xFF);

	/*
	 * VAP =  4.1V + (VCOM + VCOM offset + 0.5 * VDV)
	 * VAN = -4.1V + (VCOM + VCOM offset + 0.5 * VDV)
	 */
	if (HSD20_IPS)
		write_reg(par, VRHS, 0x13);
	else
		write_reg(par, VRHS, 0x1F);

	/* VCOM = 0.9V */
	if (HSD20_IPS)
		write_reg(par, VCOMS, 0x22);
	else
		write_reg(par, VCOMS, 0x28);

	write_reg(par, 0xC6, 0x13);

	/*
	 * AVDD = 6.8V
	 * AVCL = -4.8V
	 * VDS = 2.3V
	 */
	write_reg(par, PWCTRL1, 0xA7);
	write_reg(par, PWCTRL1, 0xA4, 0xA1);

	write_reg(par, 0xD6, 0xA1);

	/* TE line output is off by default when powering on */
	if (irq_te)
		write_reg(par, MIPI_DCS_SET_TEAR_ON, 0x00);

	write_reg(par, MIPI_DCS_ENTER_INVERT_MODE);
	write_reg(par, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);
	write_reg(par, MIPI_DCS_SET_DISPLAY_ON);
	mdelay(200);
	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
	mdelay(200);

#endif
	return 0;
}

/*
 * write_vmem() - write data to display.
 * @par: FBTFT parameter object.
 * @offset: offset from screen_buffer.
 * @len: the length of data to be writte.
 *
 * Return: 0 on success, or a negative error code otherwise.
 */
static int write_vmem(struct fbtft_par *par, size_t offset, size_t len)
{
	struct device *dev = par->info->device;
	int ret;

	dev_err(dev, "write_vmem len: %d\n", len);
	if (irq_te) {
		enable_irq(irq_te);
		reinit_completion(&panel_te);
		ret = wait_for_completion_timeout(
			&panel_te, msecs_to_jiffies(PANEL_TE_TIMEOUT_MS));
		if (ret == 0)
			dev_err(dev, "wait panel TE timeout\n");

		disable_irq(irq_te);
	}

	switch (par->pdata->display.buswidth) {
	case 8:
		ret = fbtft_write_vmem16_bus8(par, offset, len);
		break;
	case 9:
		ret = fbtft_write_vmem16_bus9(par, offset, len);
		break;
	case 16:
		ret = fbtft_write_vmem16_bus16(par, offset, len);
		break;
	default:
		dev_err(dev, "Unsupported buswidth %d\n",
			par->pdata->display.buswidth);
		ret = 0;
		break;
	}
	// switch (par->pdata->display.buswidth) {
	// case 8:
	// case 16:
	// 	ret = fbtft_write_vmem16_bus16(par, offset, len);
	// 	break;
	// default:
	// 	dev_err(dev, "Unsupported buswidth %d\n",
	// 		par->pdata->display.buswidth);
	// 	ret = 0;
	// 	break;
	// }

	return ret;
}

/**
 * set_var() - apply LCD properties like rotation and BGR mode
 *
 * @par: FBTFT parameter object
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
		break;
	case 90:
		madctl_par |= (MADCTL_MV | MADCTL_MY);
		break;
	case 180:
		madctl_par |= (MADCTL_MX | MADCTL_MY);
		break;
	case 270:
		madctl_par |= (MADCTL_MV | MADCTL_MX);
		break;
	default:
		return -EINVAL;
	}
	write_reg(par, MIPI_DCS_SET_ADDRESS_MODE, madctl_par);
	return 0;
}

/**
 * set_gamma() - set gamma curves
 *
 * @par: FBTFT parameter object
 * @curves: gamma curves
 *
 * Before the gamma curves are applied, they are preprocessed with a bitmask
 * to ensure syntactically correct input for the display controller.
 * This implies that the curves input parameter might be changed by this
 * function and that illegal gamma values are auto-corrected and not
 * reported as errors.
 *
 * Return: 0 on success, < 0 if error occurred.
 */
static int set_gamma(struct fbtft_par *par, u32 *curves)
{
	int i;
	int j;
	int c; /* curve index offset */

	/*
	 * Bitmasks for gamma curve command parameters.
	 * The masks are the same for both positive and negative voltage
	 * gamma curves.
	 */
	static const u8 gamma_par_mask[] = {
		0xFF, /* V63[3:0], V0[3:0]*/
		0x3F, /* V1[5:0] */
		0x3F, /* V2[5:0] */
		0x1F, /* V4[4:0] */
		0x1F, /* V6[4:0] */
		0x3F, /* J0[1:0], V13[3:0] */
		0x7F, /* V20[6:0] */
		0x77, /* V36[2:0], V27[2:0] */
		0x7F, /* V43[6:0] */
		0x3F, /* J1[1:0], V50[3:0] */
		0x1F, /* V57[4:0] */
		0x1F, /* V59[4:0] */
		0x3F, /* V61[5:0] */
		0x3F, /* V62[5:0] */
	};

	for (i = 0; i < par->gamma.num_curves; i++) {
		c = i * par->gamma.num_values;
		for (j = 0; j < par->gamma.num_values; j++)
			curves[c + j] &= gamma_par_mask[j];
		write_reg(par, PVGAMCTRL + i, curves[c + 0], curves[c + 1],
			  curves[c + 2], curves[c + 3], curves[c + 4],
			  curves[c + 5], curves[c + 6], curves[c + 7],
			  curves[c + 8], curves[c + 9], curves[c + 10],
			  curves[c + 11], curves[c + 12], curves[c + 13]);
	}
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

#if 0
static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	if (par->info->var.rotate == 0 || par->info->var.rotate == 180) {
		xs += 35;
		xe += 35;
	} else {
		ys += 35;
		ye += 35;
	}

	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS, (xs >> 8) & 0xFF, xs & 0xFF,
		  (xe >> 8) & 0xFF, xe & 0xFF);

	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS, (ys >> 8) & 0xFF, ys & 0xFF,
		  (ye >> 8) & 0xFF, ye & 0xFF);

	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 170,
	.height = 320,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.write_vmem = write_vmem,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
		.set_addr_win = set_addr_win,
	},
};
#else
static void set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	// if (par->info->var.rotate == 0 || par->info->var.rotate == 180) {
	// 	xs += 82;
	// 	xe += 157;
	// } else {
	// 	ys += 18;
	// 	ye += 301;
	// }
	// xs = 82;
	// xe = 157;
	// ys = 18;
	// ye = 301;
	xs = 18;
	xe = 301;
	ys = 82;
	ye = 157;

	write_reg(par, MIPI_DCS_SET_COLUMN_ADDRESS, (xs >> 8) & 0xFF, xs & 0xFF,
		  (xe >> 8) & 0xFF, xe & 0xFF);

	write_reg(par, MIPI_DCS_SET_PAGE_ADDRESS, (ys >> 8) & 0xFF, ys & 0xFF,
		  (ye >> 8) & 0xFF, ye & 0xFF);

	write_reg(par, MIPI_DCS_WRITE_MEMORY_START);
}

static struct fbtft_display display = {
	.regwidth = 8,
	.width = 76,
	.height = 284,
	.gamma_num = 2,
	.gamma_len = 14,
	.gamma = DEFAULT_GAMMA,
	.fbtftops = {
		.init_display = init_display,
		.write_vmem = write_vmem,
		.set_var = set_var,
		.set_gamma = set_gamma,
		.blank = blank,
		.set_addr_win = set_addr_win,
	},
};
#endif

FBTFT_REGISTER_DRIVER(DRVNAME, "sitronix,st7789p3", &display);

MODULE_ALIAS("spi:" DRVNAME);
MODULE_ALIAS("platform:" DRVNAME);
MODULE_ALIAS("spi:st7789p3");
MODULE_ALIAS("platform:st7789p3");

MODULE_DESCRIPTION("FB driver for the ST7789P3 LCD Controller");
MODULE_AUTHOR("916BGAI,iawak9lkm");
MODULE_LICENSE("GPL");
