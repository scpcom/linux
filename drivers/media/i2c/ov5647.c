// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for OmniVision OV5647 cameras.
 *
 * Based on Samsung S5K6AAFX SXGA 1/6" 1.3M CMOS Image Sensor driver
 * Copyright (C) 2011 Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * Based on Omnivision OV7670 Camera Driver
 * Copyright (C) 2006-7 Jonathan Corbet <corbet@lwn.net>
 *
 * Copyright (C) 2016, Synopsys, Inc.
 */

#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>

#include <linux/gpio/consumer.h>

#include <linux/rk-camera-module.h>

#define SENSOR_NAME "ov5647"

#define REG_NULL 0xffff

#define MIPI_CTRL00_BUS_IDLE	BIT(2)

// Chip control
#define OV5647_SW_STANDBY	0x100
#define OV5647_SW_RESET	0x0103

// System Control
#define OV5647_REG_CHIPID_H	0x300A
#define OV5647_REG_CHIPID_L	0x300B
#define OV5647_REG_PAD_OUT2	0x300D

// AEC/AGC1
// Exposure
#define OV5647_REG_LINE_H	0x3500
#define OV5647_REG_LINE_M	0x3501
#define OV5647_REG_LINE_L	0x3502
// AGC
#define OV5647_REG_GAIN_H	0x350A
#define OV5647_REG_GAIN_L	0x350B

// Frame Control
#define OV5647_REG_FRAME_OFF_NUMBER	0x4202

// MIPI Top
#define OV5647_REG_MIPI_CTRL00	0x4800
#define OV5647_REG_MIPI_CTRL14	0x4814

#define OV5647_EXPOSURE_MIN	0x000000
#define OV5647_EXPOSURE_MAX	0x0fffff
#define OV5647_EXPOSURE_STEP	0x01
#define OV5647_EXPOSURE_DEFAULT	0x001000

#define OV5647_ANALOG_GAIN_MIN	0x0000
#define OV5647_ANALOG_GAIN_MAX	0x03ff
#define OV5647_ANALOG_GAIN_STEP	0x01
#define OV5647_ANALOG_GAIN_DEFAULT 0x100

#define OV5647_LINK_FREQ_150MHZ		150000000
static const s64 link_freq_menu_items[] = {
	OV5647_LINK_FREQ_150MHZ
};

struct regval_list {
	u16 addr;
	u8 data;
};

struct ov5647_state {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct mutex lock;
	struct v4l2_mbus_framefmt format;
	unsigned int width;
	unsigned int height;
	int power_count;
	struct clk *xvclk;
	struct gpio_desc *pwdn_gpio;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *anal_gain;

	const struct ov5647_mode *cur_mode;
	u32 module_index;
	const char *module_facing;
	const char *module_name;
	const char *len_name;
};

struct ov5647_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	struct regval_list *reg_list;
};

static inline struct ov5647_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov5647_state, sd);
}

static const struct regval_list sensor_oe_disable_regs[] = {
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{REG_NULL, 0x00}
};

static const struct regval_list sensor_oe_enable_regs[] = {
	{0x3000, 0x0f},
	{0x3001, 0xff},
	{0x3002, 0xe4},
	{REG_NULL, 0x00}
};

static struct regval_list ov5647_common_regs[] = {
	/* upstream */
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x370c, 0x03},
	{0x5000, 0x06},
	{0x5003, 0x08},
	{0x5a00, 0x08},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x301d, 0xf0},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3c01, 0x80},
	{0x3b07, 0x0c},
	{0x3630, 0x2e},
	{0x3632, 0xe2},
	{0x3633, 0x23},
	{0x3634, 0x44},
	{0x3620, 0x64},
	{0x3621, 0xe0},
	{0x3600, 0x37},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x3731, 0x02},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3f05, 0x02},
	{0x3f06, 0x10},
	{0x3f01, 0x0a},
	{0x3a0f, 0x58},
	{0x3a10, 0x50},
	{0x3a1b, 0x58},
	{0x3a1e, 0x50},
	{0x3a11, 0x60},
	{0x3a1f, 0x28},
	{0x4001, 0x02},
	{0x4000, 0x09},
	{0x3503, 0x03},		/* manual,0xAE */
	{0x3500, 0x00},
	{0x3501, 0x6f},
	{0x3502, 0x00},
	{0x350a, 0x00},
	{0x350b, 0x6f},
	{0x5001, 0x01},		/* manual,0xAWB */
	{0x5180, 0x08},
	{0x5186, 0x04},
	{0x5187, 0x00},
	{0x5188, 0x04},
	{0x5189, 0x00},
	{0x518a, 0x04},
	{0x518b, 0x00},
	{0x5000, 0x00},		/* lenc WBC on */
	{0x3011, 0x62},
	/* mipi */
	{0x3016, 0x08},
	{0x3017, 0xe0},
	{0x3018, 0x44},
	{0x3034, 0x08},
	{0x3106, 0xf5},
	{REG_NULL, 0x00}
};

static struct regval_list ov5647_1296x972[] = {
	{0x0100, 0x00},
	{0x3035, 0x21},		/* PLL */
	{0x3036, 0x60},		/* PLL */
	{0x303c, 0x11},		/* PLL */
	{0x3821, 0x07},		/* ISP mirror on, Sensor mirror on, H bin on */
	{0x3820, 0x41},		/* ISP flip off, Sensor flip off, V bin on */
	{0x3612, 0x59},		/* analog control */
	{0x3618, 0x00},		/* analog control */
	{0x380c, 0x07},		/* HTS = 1896 */
	{0x380d, 0x68},		/* HTS */
	{0x380e, 0x05},		/* VTS = 1420 */
	{0x380f, 0x8c},		/* VTS */
	{0x3814, 0x31},		/* X INC */
	{0x3815, 0x31},		/* X INC */
	{0x3708, 0x64},		/* analog control */
	{0x3709, 0x52},		/* analog control */
	{0x3808, 0x05},		/* DVPHO = 1296 */
	{0x3809, 0x10},		/* DVPHO */
	{0x380a, 0x03},		/* DVPVO = 984 */
	{0x380b, 0xcc},		/* DVPVO */
	{0x3800, 0x00},		/* X Start */
	{0x3801, 0x08},		/* X Start */
	{0x3802, 0x00},		/* Y Start */
	{0x3803, 0x02},		/* Y Start */
	{0x3804, 0x0a},		/* X End */
	{0x3805, 0x37},		/* X End */
	{0x3806, 0x07},		/* Y End */
	{0x3807, 0xa1},		/* Y End */
	/* banding filter */
	{0x3a08, 0x01},		/* B50 */
	{0x3a09, 0x27},		/* B50 */
	{0x3a0a, 0x00},		/* B60 */
	{0x3a0b, 0xf6},		/* B60 */
	{0x3a0d, 0x04},		/* B60 max */
	{0x3a0e, 0x03},		/* B50 max */
	{0x4004, 0x02},		/* black line number */
	{0x4837, 0x24},		/* MIPI pclk period */
	{0x0100, 0x01},
	{REG_NULL, 0x00}
};

static struct regval_list ov5647_2592x1944[] = {
	{0x0100, 0x00},
	{0x3035, 0x21},
	{0x3036, 0x60},
	{0x303c, 0x11},
	{0x3612, 0x5b},
	{0x3618, 0x04},
	{0x380c, 0x0a},
	{0x380d, 0x8c},
	{0x380e, 0x07},
	{0x380f, 0xb6},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3708, 0x64},
	{0x3709, 0x12},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x3800, 0x00},
	{0x3801, 0x0c},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x0a},
	{0x3805, 0x33},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3a08, 0x01},
	{0x3a09, 0x28},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0d, 0x08},
	{0x3a0e, 0x06},
	{0x4004, 0x04},
	{0x4837, 0x19},
	{0x0100, 0x01},
	{REG_NULL, 0x00}
};

static const struct ov5647_mode supported_modes[] = {
	{
		.width = 2592,
		.height = 1944,
		.max_fps = {
			.numerator = 10000,
			.denominator = 150000,
		},
		.hts_def = 0x0a8c,
		.vts_def = 0x07b6,
		.reg_list = ov5647_2592x1944,
	},
	{
		.width = 1296,
		.height = 972,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.hts_def = 0x0768,
		.vts_def = 0x058c,
		.reg_list = ov5647_1296x972,
	},
};


static const s64 link_freq_menu_items[] = {
	175000000,
	163333400,
	163333400,
	110000000,
};

static const struct ov5647_mode ov5647_modes[] = {
	/* 2592x1944 full resolution full FOV 10-bit mode. */
	{
		.format = {
			.code		= MEDIA_BUS_FMT_SBGGR10_1X10,
			.colorspace	= V4L2_COLORSPACE_RAW,
			.field		= V4L2_FIELD_NONE,
			.width		= 2592,
			.height		= 1944
		},
		.max_fps = {
			.numerator = 10000,
			.denominator = 150000,
		},
		.crop = {
			.left		= OV5647_PIXEL_ARRAY_LEFT,
			.top		= OV5647_PIXEL_ARRAY_TOP,
			.width		= 2592,
			.height		= 1944
		},
		.pixel_rate	= 87500000,
		.hts		= 2844,
		.vts		= 0x7b0,
		.link_freq_index = 0,
		.reg_list	= ov5647_2592x1944_10bpp,
		.num_regs	= ARRAY_SIZE(ov5647_2592x1944_10bpp)
	},
	/* 1080p30 10-bit mode. Full resolution centre-cropped down to 1080p. */
	{
		.format = {
			.code		= MEDIA_BUS_FMT_SBGGR10_1X10,
			.colorspace	= V4L2_COLORSPACE_RAW,
			.field		= V4L2_FIELD_NONE,
			.width		= 1920,
			.height		= 1080
		},
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.crop = {
			.left		= 348 + OV5647_PIXEL_ARRAY_LEFT,
			.top		= 434 + OV5647_PIXEL_ARRAY_TOP,
			.width		= 1928,
			.height		= 1080,
		},
		.pixel_rate	= 81666700,
		.hts		= 2416,
		.vts		= 0x450,
		.link_freq_index = 1,
		.reg_list	= ov5647_1080p30_10bpp,
		.num_regs	= ARRAY_SIZE(ov5647_1080p30_10bpp)
	},
	/* 2x2 binned full FOV 10-bit mode. */
	{
		.format = {
			.code		= MEDIA_BUS_FMT_SBGGR10_1X10,
			.colorspace	= V4L2_COLORSPACE_RAW,
			.field		= V4L2_FIELD_NONE,
			.width		= 1296,
			.height		= 972
		},
		.max_fps = {
			.numerator = 10000,
			.denominator = 450000,
		},
		.crop = {
			.left		= OV5647_PIXEL_ARRAY_LEFT,
			.top		= OV5647_PIXEL_ARRAY_TOP,
			.width		= 2592,
			.height		= 1944,
		},
		.pixel_rate	= 81666700,
		.hts		= 1896,
		.vts		= 0x59b,
		.link_freq_index = 2,
		.reg_list	= ov5647_2x2binned_10bpp,
		.num_regs	= ARRAY_SIZE(ov5647_2x2binned_10bpp)
	},
	/* 10-bit VGA full FOV 60fps. 2x2 binned and subsampled down to VGA. */
	{
		.format = {
			.code		= MEDIA_BUS_FMT_SBGGR10_1X10,
			.colorspace	= V4L2_COLORSPACE_RAW,
			.field		= V4L2_FIELD_NONE,
			.width		= 640,
			.height		= 480
		},
		.max_fps = {
			.numerator = 10000,
			.denominator = 900000,
		},
		.crop = {
			.left		= 16 + OV5647_PIXEL_ARRAY_LEFT,
			.top		= OV5647_PIXEL_ARRAY_TOP,
			.width		= 2560,
			.height		= 1920,
		},
		.pixel_rate	= 55000000,
		.hts		= 1852,
		.vts		= 0x1f8,
		.link_freq_index = 3,
		.reg_list	= ov5647_640x480_10bpp,
		.num_regs	= ARRAY_SIZE(ov5647_640x480_10bpp)
	},
};

/* Default sensor mode is 2x2 binned 640x480 SBGGR10_1X10. */
#define OV5647_DEFAULT_MODE	(&ov5647_modes[0])
#define OV5647_DEFAULT_FORMAT	(ov5647_modes[0].format)

static int ov5647_write16(struct v4l2_subdev *sd, u16 reg, u16 val)
{
	unsigned char data[4] = { reg >> 8, reg & 0xff, val >> 8, val & 0xff};
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_master_send(client, data, 3);
	/*
	 * Writing the wrong number of bytes also needs to be flagged as an
	 * error. Success needs to produce a 0 return code.
	 */
	if (ret == 3) {
		ret = 0;
	} else {
		dev_dbg(&client->dev, "%s: i2c write error, reg: %x\n",
			__func__, reg);

	return 0;
}

static int ov5647_read(struct v4l2_subdev *sd, u16 reg, u8 *val)
{
	unsigned char data_w[2] = { reg >> 8, reg & 0xff };
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = i2c_master_send(client, data_w, 2);
	/*
	 * A negative return code, or sending the wrong number of bytes, both
	 * count as an error.
	 */
	if (ret != 2) {
		dev_dbg(&client->dev, "%s: i2c write error, reg: %x\n",
			__func__, reg);
		if (ret >= 0)
			ret = -EINVAL;
		return ret;
	}

	ret = i2c_master_recv(client, val, 1);
	/*
	 * The only return value indicating success is 1. Anything else, even
	 * a non-negative value, indicates something went wrong.
	 */
	if (ret == 1) {
		ret = 0;
	} else {
		dev_dbg(&client->dev, "%s: i2c read error, reg: %x\n",
			__func__, reg);

	return 0;
}

static int ov5647_write_array(struct v4l2_subdev *sd, struct regval_list *regs)
{
	int i, ret;

	for (i = 0; regs[i].addr != REG_NULL; i++) {
		ret = ov5647_write(sd, regs[i].addr, regs[i].data);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ov5647_set_virtual_channel(struct v4l2_subdev *sd, int channel)
{
	u8 channel_id;
	int ret;

	ret = ov5647_read(sd, OV5647_REG_MIPI_CTRL14, &channel_id);
	if (ret < 0)
		return ret;

	channel_id &= ~(3 << 6);

	return ov5647_write(sd, OV5647_REG_MIPI_CTRL14,
			    channel_id | (channel << 6));
}

static int ov5647_set_mode(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647 *sensor = to_sensor(sd);
	u8 resetval, rdval;
	int ret;

	ret = ov5647_read(sd, OV5647_SW_STANDBY, &rdval);
	if (ret < 0)
		return ret;

	ret = ov5647_write_array(sd, sensor->mode->reg_list,
				 sensor->mode->num_regs);
	if (ret < 0) {
		dev_err(&client->dev, "write sensor default regs error\n");
		return ret;
	}

	return ov5647_write(sd, OV5647_REG_PAD_OUT2, 0x00);
}

static int ov5647_stream_off(struct v4l2_subdev *sd)
{
	int ret;

	ret = ov5647_write(sd, OV5647_REG_MIPI_CTRL00, 0x25);
	if (ret < 0)
		return ret;

	ret = ov5647_read(sd, OV5647_SW_STANDBY, &resetval);
	if (ret < 0)
		return ret;

	return ov5647_write(sd, OV5647_REG_PAD_OUT2, 0x01);
}

static int ov5647_stream_on(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647 *sensor = to_sensor(sd);
	u8 val = MIPI_CTRL00_BUS_IDLE;
	int ret;

	ret = ov5647_set_mode(sd);
	if (ret) {
		dev_err(&client->dev, "Failed to program sensor mode: %d\n", ret);
		return ret;
	}

	/* Apply customized values from user when stream starts. */
	ret =  __v4l2_ctrl_handler_setup(sd->ctrl_handler);
	if (ret)
		return ret;

static int ov5647_set_exposure(struct v4l2_subdev *sd, s32 val)
{
	int ret;

	ret = ov5647_write(sd, OV5647_REG_LINE_L, val & 0x00FF);
	if (ret < 0)
		return ret;

	ret = ov5647_write(sd, OV5647_REG_LINE_M, (val & 0xFF00) >> 8);
	if (ret < 0)
		return ret;

	return ov5647_write(sd, OV5647_REG_LINE_H, val >> 16);
}

static int ov5647_set_analog_gain(struct v4l2_subdev *sd, s32 val)
{
	int ret;

	ret = ov5647_write(sd, OV5647_REG_GAIN_L, val & 0xff);
	if (ret < 0)
		return ret;

	return ov5647_write(sd, OV5647_REG_GAIN_H, val >> 8);
}

static int __sensor_init(struct v4l2_subdev *sd)
{
	int ret;
	u8 resetval, rdval;
	struct ov5647_state *ov5647 = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	ret = ov5647_write(sd, OV5647_REG_MIPI_CTRL00, val);
	if (ret < 0)
		return ret;

	ret = ov5647_write_array(sd, ov5647_common_regs);
	if (ret < 0) {
		dev_err(&client->dev, "write sensor common regs error\n");
		return ret;
	}

	ret = ov5647_write_array(sd, ov5647->cur_mode->reg_list);
	if (ret < 0) {
		dev_err(&client->dev, "write sensor mode regs error\n");
		return ret;

	return ov5647_write(sd, OV5640_REG_PAD_OUT, 0x00);
}

static int ov5647_stream_off(struct v4l2_subdev *sd)
{
	int ret;

	ret = ov5647_write(sd, OV5647_REG_MIPI_CTRL00,
			   MIPI_CTRL00_CLOCK_LANE_GATE | MIPI_CTRL00_BUS_IDLE |
			   MIPI_CTRL00_CLOCK_LANE_DISABLE);
	if (ret < 0)
		return ret;
	ret = ov5647_set_exposure(sd, ov5647->exposure->val * 16);
	if (ret < 0)
		return ret;
	ret = ov5647_set_analog_gain(sd, ov5647->anal_gain->val);
	if (ret < 0)
		return ret;

	ret = ov5647_write(sd, OV5647_REG_FRAME_OFF_NUMBER, 0x0f);
	if (ret < 0)
		return ret;

	if (!(resetval & 0x01)) {
		dev_err(&client->dev, "Device was in SW standby");
		ret = ov5647_write(sd, OV5647_SW_STANDBY, 0x01);
		if (ret < 0)
			return ret;
	}

	return ov5647_stream_off(sd);
}

static int ov5647_power_on(struct device *dev)
{
	int ret = 0;
	struct ov5647_state *ov5647 = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_info(dev, "OV5647 power on\n");

	if (on && !ov5647->power_count) {
		dev_err(&client->dev, "OV5647 power on\n");

		ret = clk_prepare_enable(ov5647->xvclk);
		if (ret < 0) {
			dev_err(&client->dev, "clk prepare enable failed\n");
			goto out;
		}
		if (!IS_ERR(ov5647->pwdn_gpio))
			gpiod_set_value_cansleep(ov5647->pwdn_gpio, 1);

		ret = ov5647_write_array(sd, sensor_oe_enable_regs);
		if (ret < 0) {
			clk_disable_unprepare(ov5647->xvclk);
			dev_err(&client->dev,
				"write sensor_oe_enable_regs error\n");
			goto out;
		}

		ret = __sensor_init(sd);
		if (ret < 0) {
			clk_disable_unprepare(ov5647->xvclk);
			dev_err(&client->dev,
				"Camera not available, check Power\n");
			goto out;
		}
	} else if (!on && ov5647->power_count == 1) {
		dev_err(&client->dev, "OV5647 power off\n");

		ret = ov5647_write_array(sd, sensor_oe_disable_regs);

error_clk_disable:
	clk_disable_unprepare(sensor->xclk);
error_pwdn:
	gpiod_set_value_cansleep(sensor->pwdn, 1);

	return ret;
}

static int ov5647_power_off(struct device *dev)
{
	struct ov5647 *sensor = dev_get_drvdata(dev);
	u8 rdval;
	int ret;

		clk_disable_unprepare(ov5647->xvclk);

		if (!IS_ERR(ov5647->pwdn_gpio))
			gpiod_set_value_cansleep(ov5647->pwdn_gpio, 0);
	}

	ret = ov5647_write_array(&sensor->sd, sensor_oe_disable_regs,
				 ARRAY_SIZE(sensor_oe_disable_regs));
	if (ret < 0)
		dev_dbg(dev, "disable oe failed\n");

	/* Enter software standby */
	ret = ov5647_read(&sensor->sd, OV5647_SW_STANDBY, &rdval);
	if (ret < 0)
		dev_dbg(dev, "software standby failed\n");

	rdval &= ~0x01;
	ret = ov5647_write(&sensor->sd, OV5647_SW_STANDBY, rdval);
	if (ret < 0)
		dev_dbg(dev, "software standby failed\n");

	clk_disable_unprepare(sensor->xclk);
	gpiod_set_value_cansleep(sensor->pwdn, 1);

	return 0;
}

static void ov5647_sensor_get_module_inf(struct ov5647_state *ov5647,
				struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strlcpy(inf->base.sensor, SENSOR_NAME, sizeof(inf->base.sensor));
	strlcpy(inf->base.module, ov5647->module_name,
			sizeof(inf->base.module));
	strlcpy(inf->base.lens, ov5647->len_name, sizeof(inf->base.lens));
}

static long ov5647_sensor_ioctl(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	struct ov5647_state *ov5647 = to_state(sd);
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
		case RKMODULE_GET_MODULE_INFO:
			ov5647_sensor_get_module_inf(ov5647, (struct rkmodule_inf *)arg);
			break;
		case RKMODULE_SET_QUICK_STREAM:
			stream = *((u32 *)arg);
			if (stream)
				set_sw_standby(sd, false);
			else
				set_sw_standby(sd, true);
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ov5647_sensor_compat_ioctl32(struct v4l2_subdev *sd,
				unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	long ret;
	u32 stream = 0;

	switch (cmd) {
		case RKMODULE_GET_MODULE_INFO:
			inf = kzalloc(sizeof(*inf), GFP_KERNEL);
			if (!inf) {
				ret = -ENOMEM;
				return ret;
			}

			ret = ov5647_sensor_ioctl(sd, cmd, inf);
			if (!ret)
				ret = copy_to_user(up, inf, sizeof(*inf));
			kfree(inf);
			break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(cfg, up, sizeof(*cfg));
		if (!ret)
			ret = ov5647_sensor_ioctl(sd, cmd, cfg);
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = ov5647_sensor_ioctl(sd, cmd, &stream);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5647_sensor_get_register(struct v4l2_subdev *sd,
				      struct v4l2_dbg_register *reg)
{
	int ret;
	u8 val;

	ret = ov5647_read(sd, reg->reg & 0xff, &val);
	if (ret < 0)
		return ret;

	reg->val = val;
	reg->size = 1;

	return 0;
}

static int ov5647_sensor_set_register(struct v4l2_subdev *sd,
				      const struct v4l2_dbg_register *reg)
{
	return ov5647_write(sd, reg->reg & 0xff, reg->val & 0xff);
}
#endif

/**
 * @short Subdev core operations registration
 */
static const struct v4l2_subdev_core_ops ov5647_subdev_core_ops = {
	.s_power = ov5647_sensor_power,
	.ioctl = ov5647_sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = ov5647_sensor_compat_ioctl32,
#endif
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov5647_sensor_get_register,
	.s_register = ov5647_sensor_set_register,
#endif
	.ioctl	= ov5647_ioctl,
};

static const struct v4l2_rect *
__ov5647_get_pad_crop(struct ov5647 *ov5647,
		      struct v4l2_subdev_pad_config *sd_state,
		      unsigned int pad, enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_crop(&ov5647->sd, sd_state, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &ov5647->mode->crop;
	}

	return NULL;
}

static int ov5647_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5647 *sensor = to_sensor(sd);
	int ret;

	mutex_lock(&sensor->lock);
	if (sensor->streaming == enable) {
		mutex_unlock(&sensor->lock);
		return 0;
	}

	if (enable) {
		dev_info(&client->dev, "stream start\n");
		ret = pm_runtime_resume(&client->dev);
		if (ret < 0)
		{
			dev_err(&client->dev, "pm_runtime_resume failed: %d\n", ret);
			goto error_unlock;
		}
		ret = pm_runtime_get(&client->dev);
		if (ret < 0)
		{
			dev_err(&client->dev, "pm_runtime_get failed: %d\n", ret);
			goto error_unlock;
		}
		ret = ov5647_stream_on(sd);
		if (ret < 0) {
			dev_err(&client->dev, "stream start failed: %d\n", ret);
			goto error_pm;
		}
	} else {
		dev_info(&client->dev, "stream stop\n");
		ret = ov5647_stream_off(sd);
		if (ret < 0) {
			dev_err(&client->dev, "stream stop failed: %d\n", ret);
			goto error_pm;
		}
		pm_runtime_put(&client->dev);
	}

	sensor->streaming = enable;
	mutex_unlock(&sensor->lock);

	return 0;

error_pm:
	pm_runtime_put(&client->dev);
error_unlock:    
	mutex_unlock(&sensor->lock);

	return ret;
}

static int ov5647_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct ov5647 *sensor = to_sensor(sd);
	const struct ov5647_mode *mode = sensor->mode;

	mutex_lock(&sensor->lock);
	fi->interval = mode->max_fps;
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ov5647_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	u32 val = 0;

	val = V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0 |
	      V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK;
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}


static int ov5647_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	*status = 0;
	return 0;
}

static int ov5647_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *fi)
{
	struct ov5647_state *ov5647 = to_state(sd);
	const struct ov5647_mode *mode = ov5647->cur_mode;

	mutex_lock(&ov5647->lock);
	fi->interval = mode->max_fps;
	mutex_unlock(&ov5647->lock);

	return 0;
}

#define OV5647_LANES	2
static int ov5647_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	u32 val = 0;

	val = 1 << (OV5647_LANES - 1) |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}

static const struct v4l2_subdev_video_ops ov5647_subdev_video_ops = {
	.s_stream = ov5647_s_stream,
	.g_frame_interval = ov5647_g_frame_interval,
};

/* This function returns the mbus code for the current settings of the
   HFLIP and VFLIP controls. */

static u32 ov5647_get_mbus_code(struct v4l2_subdev *sd)
{
	struct ov5647 *sensor = to_sensor(sd);
	/* The control values are only 0 or 1. */
	int index =  sensor->hflip->val | (sensor->vflip->val << 1);

	static const u32 codes[4] = {
		MEDIA_BUS_FMT_SGBRG10_1X10,
		MEDIA_BUS_FMT_SBGGR10_1X10,
		MEDIA_BUS_FMT_SRGGB10_1X10,
		MEDIA_BUS_FMT_SGRBG10_1X10
	};

	return codes[index];
}

static int ov5647_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = ov5647_get_mbus_code(sd);

	return 0;
}

static int ov5647_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(ov5647_modes))
		return -EINVAL;

	fie->code = MEDIA_BUS_FMT_SBGGR10_1X10;

	fie->width = ov5647_modes[fie->index].format.width;
	fie->height = ov5647_modes[fie->index].format.height;
	fie->interval = ov5647_modes[fie->index].max_fps;
	return 0;
}

static int ov5647_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	const struct v4l2_mbus_framefmt *fmt;

	if (fse->code != ov5647_get_mbus_code(sd) ||
	    fse->index >= ARRAY_SIZE(ov5647_modes))
		return -EINVAL;

	fmt = &ov5647_modes[fse->index].format;
	fse->min_width = fmt->width;
	fse->max_width = fmt->width;
	fse->min_height = fmt->height;
	fse->max_height = fmt->height;

	return 0;
}

static int ov5647_get_pad_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *sd_state,
			      struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt = &format->format;
	const struct v4l2_mbus_framefmt *sensor_format;
	struct ov5647 *sensor = to_sensor(sd);

	mutex_lock(&sensor->lock);
	switch (format->which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		sensor_format = v4l2_subdev_get_try_format(sd, sd_state,
							   format->pad);
		break;
	default:
		sensor_format = &sensor->mode->format;
		break;
	}

	*fmt = *sensor_format;
	/* The code we pass back must reflect the current h/vflips. */
	fmt->code = ov5647_get_mbus_code(sd);
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ov5647_set_pad_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *sd_state,
			      struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt = &format->format;
	struct ov5647 *sensor = to_sensor(sd);
	const struct ov5647_mode *mode;

	mode = v4l2_find_nearest_size(ov5647_modes, ARRAY_SIZE(ov5647_modes),
				      format.width, format.height,
				      fmt->width, fmt->height);

	/* Update the sensor mode and apply at it at streamon time. */
	mutex_lock(&sensor->lock);
	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, sd_state, format->pad) = mode->format;
	} else {
		int exposure_max, exposure_def;
		int hblank, vblank;

		sensor->mode = mode;
		__v4l2_ctrl_modify_range(sensor->pixel_rate, mode->pixel_rate,
					 mode->pixel_rate, 1, mode->pixel_rate);

		hblank = mode->hts - mode->format.width;
		__v4l2_ctrl_modify_range(sensor->hblank, hblank, hblank, 1,
					 hblank);

		vblank = mode->vts - mode->format.height;
		__v4l2_ctrl_modify_range(sensor->vblank, OV5647_VBLANK_MIN,
					 OV5647_VTS_MAX - mode->format.height,
					 1, vblank);
		__v4l2_ctrl_s_ctrl(sensor->vblank, vblank);

		exposure_max = mode->vts - 4;
		exposure_def = min(exposure_max, OV5647_EXPOSURE_DEFAULT);
		__v4l2_ctrl_modify_range(sensor->exposure,
					 sensor->exposure->minimum,
					 exposure_max, sensor->exposure->step,
					 exposure_def);
	}
	*fmt = mode->format;
	/* The code we pass back must reflect the current h/vflips. */
	fmt->code = ov5647_get_mbus_code(sd);

	__v4l2_ctrl_s_ctrl(sensor->link_freq, mode->link_freq_index);
	v4l2_info(sd, "%s res wxh:%dx%d, link freq:%llu", __func__,
			fmt->width, fmt->height, link_freq_menu_items[mode->link_freq_index]);
	mutex_unlock(&sensor->lock);

	return 0;
}

static int ov5647_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SBGGR8_1X8)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = supported_modes[fse->index].height;

	return 0;
}

static int ov5647_enum_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fie->code != MEDIA_BUS_FMT_SBGGR8_1X8)
		return -EINVAL;

	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	return 0;
}

static int ov5647_get_reso_dist(const struct ov5647_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	    abs(mode->height - framefmt->height);
}

static const struct ov5647_mode *ov5647_find_best_fit(struct v4l2_subdev_format
						      *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = ov5647_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int ov5647_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct ov5647_state *ov5647 = to_state(sd);
	const struct ov5647_mode *mode;
	s64 h_blank, v_blank, pixel_rate;

	mutex_lock(&ov5647->lock);

	mode = ov5647_find_best_fit(fmt);
	fmt->format.code = MEDIA_BUS_FMT_SBGGR8_1X8;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
	} else {
		ov5647->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(ov5647->hblank, h_blank,
					 h_blank, 1, h_blank);
		v_blank = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(ov5647->vblank, v_blank,
					 v_blank, 1, v_blank);
		pixel_rate = mode->vts_def * mode->hts_def *
			(mode->max_fps.denominator / mode->max_fps.numerator);
		__v4l2_ctrl_modify_range(ov5647->pixel_rate, pixel_rate,
					 pixel_rate, 1, pixel_rate);
	}

	mutex_unlock(&ov5647->lock);

	return 0;
}

static int ov5647_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct ov5647_state *ov5647 = to_state(sd);
	const struct ov5647_mode *mode = ov5647->cur_mode;

	mutex_lock(&ov5647->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = MEDIA_BUS_FMT_SBGGR8_1X8;
		fmt->format.field = V4L2_FIELD_NONE;
	}

	mutex_unlock(&ov5647->lock);

	return 0;
}

static const struct v4l2_subdev_pad_ops ov5647_subdev_pad_ops = {
	.enum_mbus_code = ov5647_enum_mbus_code,
	.enum_frame_size = ov5647_enum_frame_size,
	.enum_frame_interval = ov5647_enum_frame_interval,
	.get_fmt = ov5647_get_fmt,
	.set_fmt = ov5647_set_fmt,
	.get_mbus_config = ov5647_g_mbus_config,
};

static const struct v4l2_subdev_ops ov5647_subdev_ops = {
	.core = &ov5647_subdev_core_ops,
	.video = &ov5647_subdev_video_ops,
	.pad = &ov5647_subdev_pad_ops,
};

static int ov5647_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5647_state *ov5647 =
	    container_of(ctrl->handler, struct ov5647_state, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ov5647_set_exposure(&ov5647->sd, ctrl->val * 16);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ov5647_set_analog_gain(&ov5647->sd, ctrl->val);
		break;
	default:
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ov5647_ctrl_ops = {
	.s_ctrl = ov5647_set_ctrl,
};

static int ov5647_initialize_controls(struct v4l2_subdev *sd)
{
	struct v4l2_ctrl_handler *handler;
	struct ov5647_state *ov5647 = to_state(sd);
	const struct ov5647_mode *mode = ov5647->cur_mode;
	s64 pixel_rate, h_blank, v_blank;
	int ret;

	handler = &ov5647->ctrl_handler;
	ret = v4l2_ctrl_handler_init(handler, 1);
	if (ret)
		return ret;

	/* freq */
	ov5647->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
						   V4L2_CID_LINK_FREQ,
						   0, 0, link_freq_menu_items);
	if (ov5647->link_freq)
		ov5647->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	pixel_rate = mode->vts_def * mode->hts_def *
			(mode->max_fps.denominator / mode->max_fps.numerator);
	ov5647->pixel_rate =
	    v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE, 0, pixel_rate,
			      1, pixel_rate);

	/* blank */
	h_blank = mode->hts_def - mode->width;
	ov5647->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
					   h_blank, h_blank, 1, h_blank);
	v_blank = mode->vts_def - mode->height;
	ov5647->vblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_VBLANK,
					   v_blank, v_blank, 1, v_blank);

	/* exposure */
	ov5647->exposure = v4l2_ctrl_new_std(handler, &ov5647_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     OV5647_EXPOSURE_MIN,
					     OV5647_EXPOSURE_MAX,
					     OV5647_EXPOSURE_STEP,
					     OV5647_EXPOSURE_DEFAULT);
	ov5647->anal_gain =
	    v4l2_ctrl_new_std(handler, &ov5647_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			      OV5647_ANALOG_GAIN_MIN, OV5647_ANALOG_GAIN_MAX,
			      OV5647_ANALOG_GAIN_STEP,
			      OV5647_ANALOG_GAIN_DEFAULT);

	if (handler->error) {
		v4l2_ctrl_handler_free(handler);
		return handler->error;
	}

	sd->ctrl_handler = handler;

	return 0;
}

static int ov5647_detect(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 read;
	int ret;

	ret = ov5647_write(sd, OV5647_SW_RESET, 0x01);
	if (ret < 0)
		return ret;

	ret = ov5647_read(sd, OV5647_REG_CHIPID_H, &read);
	if (ret < 0)
		return ret;

	if (read != 0x56) {
		dev_err(&client->dev, "ID High expected 0x56 got %x", read);
		return -ENODEV;
	}

	ret = ov5647_read(sd, OV5647_REG_CHIPID_L, &read);
	if (ret < 0)
		return ret;

	if (read != 0x47) {
		dev_err(&client->dev, "ID Low expected 0x47 got %x", read);
		return -ENODEV;
	}

	return ov5647_write(sd, OV5647_SW_RESET, 0x00);
}

static int ov5647_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ov5647_state *ov5647 = to_state(sd);
	struct v4l2_mbus_framefmt *try_fmt =
	    v4l2_subdev_get_try_format(sd, fh->pad, 0);

	mutex_lock(&ov5647->lock);
	/* Initialize try_fmt */
	try_fmt->width = ov5647->cur_mode->width;
	try_fmt->height = ov5647->cur_mode->height;
	try_fmt->code = MEDIA_BUS_FMT_SBGGR8_1X8;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&ov5647->lock);
	/* No crop or compose */
	return 0;
}

static const struct v4l2_subdev_internal_ops ov5647_subdev_internal_ops = {
	.open = ov5647_open,
};

static int ov5647_parse_dt(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_fwnode_endpoint bus_cfg;
	struct device_node *ep;
	struct fwnode_handle *fwnode;

static int ov5647_parse_dt(struct ov5647 *sensor, struct device_node *np)
{
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	struct device_node *ep;
	int ret;

	ep = of_graph_get_next_endpoint(client->dev.of_node, NULL);
	if (!ep)
		return -EINVAL;

	fwnode = of_fwnode_handle(ep);
		//lane num is fixed to 2. so just read, not use it.
	ret = fwnode_property_read_u32_array(fwnode, "data-lanes", NULL, 0);
	if (ret <= 0) {
		dev_info (&client->dev, "[%d] lane - %d\n",__LINE__, ret);
	}
	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(ep), &bus_cfg);
	if (ret)
		goto out;

	sensor->clock_ncont = bus_cfg.bus.mipi_csi2.flags &
			      V4L2_MBUS_CSI2_NONCONTINUOUS_CLOCK;

out:
	of_node_put(ep);

	return ret;
}

static int ov5647_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct device *dev = &client->dev;
	struct ov5647_state *sensor;
	int ret;
	struct v4l2_subdev *sd;
	struct device_node *np = dev->of_node;
	u32 xvclk_freq;
	char facing[2] = "b";

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	ret = of_property_read_u32(np, RKMODULE_CAMERA_MODULE_INDEX,
			&sensor->module_index);
	if (ret) {
		dev_warn(dev, "could not get module index!\n");
		sensor->module_index = 0;
	}
	ret |= of_property_read_string(np, RKMODULE_CAMERA_MODULE_FACING,
			&sensor->module_facing);
	ret |= of_property_read_string(np, RKMODULE_CAMERA_MODULE_NAME,
			&sensor->module_name);
	ret |= of_property_read_string(np, RKMODULE_CAMERA_LENS_NAME,
			&sensor->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* get system clock (xvclk) */
	sensor->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sensor->xvclk)) {
		dev_err(dev, "could not get xvclk");
		return PTR_ERR(sensor->xvclk);
	}

	xvclk_freq = clk_get_rate(sensor->xvclk);
	if (xvclk_freq != 24000000) {
		dev_err(dev, "Unsupported clock frequency: %u\n", xvclk_freq);
		return -EINVAL;
	}

	sensor->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	mutex_init(&sensor->lock);

	sensor->cur_mode = &supported_modes[0];
	sd = &sensor->sd;
	v4l2_i2c_subdev_init(sd, client, &ov5647_subdev_ops);

	if (IS_ENABLED(CONFIG_OF) && sd) {
		ret = ov5647_parse_dt(sd);
		if (ret) {
			dev_err(dev, "DT parsing error: %d\n", ret);
			return ret;
		}
	}

	ret = ov5647_initialize_controls(sd);
	if (ret)
		return ret;

	sensor->sd.internal_ops = &ov5647_subdev_internal_ops;
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;

	ret = media_entity_pads_init(&sd->entity, 1, &sensor->pad);
	if (ret < 0)
		goto ctrl_handler_free;

	ret = ov5647_power_on(dev);
	if (ret)
		goto entity_cleanup;

	ret = ov5647_detect(sd);
	if (ret < 0)
		goto power_off;

	memset(facing, 0, sizeof(facing));
	if (strcmp(sensor->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	memset(facing, 0, sizeof(facing));
	if (strcmp(sensor->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
			sensor->module_index, facing,
			SENSOR_NAME, dev_name(sd->dev));

	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret < 0)
		goto power_off;

	/* Enable runtime PM and turn off the device */
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	dev_info(dev, "OmniVision OV5647 camera driver probed\n");

	return 0;

power_off:
	ov5647_power_off(dev);
entity_cleanup:
	media_entity_cleanup(&sd->entity);
ctrl_handler_free:
	v4l2_ctrl_handler_free(&sensor->ctrls);
mutex_destroy:
	mutex_destroy(&sensor->lock);

	return ret;
}

static int ov5647_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5647_state *ov5647 = to_state(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls);
	v4l2_device_unregister_subdev(sd);
	pm_runtime_disable(&client->dev);
	mutex_destroy(&sensor->lock);

	return 0;
}

static const struct dev_pm_ops ov5647_pm_ops = {
	SET_RUNTIME_PM_OPS(ov5647_power_off, ov5647_power_on, NULL)
};

static const struct i2c_device_id ov5647_id[] = {
	{ "ov5647", 0 },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(i2c, ov5647_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ov5647_of_match[] = {
	{ .compatible = "ovti,ov5647" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, ov5647_of_match);
#endif

static struct i2c_driver ov5647_driver = {
	.driver = {
		.of_match_table = of_match_ptr(ov5647_of_match),
		.name	= OV5647_NAME,
		.pm	= &ov5647_pm_ops,
	},
	.probe		= ov5647_probe,
	.remove		= ov5647_remove,
	.id_table	= ov5647_id,
};

module_i2c_driver(ov5647_driver);

MODULE_AUTHOR("Ramiro Oliveira <roliveir@synopsys.com>");
MODULE_AUTHOR("Modify by abel <guilin1985@gmail.com>");
MODULE_AUTHOR("Stephen Chen <stephen@radxa.com>");
MODULE_DESCRIPTION("A low-level driver for OmniVision ov5647 sensors");
MODULE_LICENSE("GPL v2");
