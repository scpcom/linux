/*
 * td100.c --  Allwinnertech TD100 Codec ALSA Soc Audio driver
 * (C) Copyright 2014-2018
 * allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * Author: Lu
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <linux/of.h>
#include <sound/tlv.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include "../../../drivers/media/platform/sunxi-vin/modules/sensor/td100/td100_drv.h"
#include "td100.h"

struct td100_priv {
	struct snd_soc_codec *codec;
};

struct real_val_to_reg_val {
	unsigned int real_val;
	unsigned int reg_val;
};

static const struct real_val_to_reg_val td100_sample_rate[] = {
	{8000,   5},
	{11025,  4},
	{12000,  4},
	{16000,  3},
	{22050,  2},
	{24000,  2},
	{32000,  1},
	{44100,  0},
	{48000,  0},
};

static const struct real_val_to_reg_val td100_bclk_div[] = {
	{0, 0},
	{1, 1},
	{2, 2},
	{4, 3},
	{6, 4},
	{8, 5},
	{12, 6},
	{16, 7},
	{24, 8},
	{32, 9},
	{48, 10},
	{64, 11},
	{96, 12},
	{128, 13},
	{176, 14},
	{192, 15},
};

static int td100_read(u16 addr, u8 *value);
static int td100_update_bits(u16 addr, u8 mask, u8 value);

/************************* General(volume) controls ***************************/
static const DECLARE_TLV_DB_SCALE(adc_pga_gain_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(digital_vol_tlv, -11925, 75, 0);

/* td100 volume controls */
static const struct snd_kcontrol_new td100_controls[] = {
	/* Channels PGA Gain */
	SOC_SINGLE_TLV("MIC1 Gain", SUNXI_ANA_ADC1_CTRL, MIC1_PGA_GAIN_CTRL,
			0x7, 0, adc_pga_gain_tlv),
	SOC_SINGLE_TLV("MIC2 Gain", SUNXI_ANA_ADC2_CTRL, MIC2_PGA_GAIN_CTRL,
			0x7, 0, adc_pga_gain_tlv),
	SOC_SINGLE_TLV("MIC3 Gain", SUNXI_ANA_ADC3_CTRL, MIC3_PGA_GAIN_CTRL,
			0x7, 0, adc_pga_gain_tlv),
	SOC_SINGLE_TLV("LINEIN Gain", SUNXI_ANA_ADCL_CTRL, LINEIN_GAIN,
			0x7, 0, adc_pga_gain_tlv),

	/* Channels Digital Volume */
	SOC_SINGLE_TLV("ADC1 Digital Volume", SUNXI_ADC1_DVOL_CTRL, 0, 0xff,
			   0, digital_vol_tlv),
	SOC_SINGLE_TLV("ADC2 Digital Volume", SUNXI_ADC2_DVOL_CTRL, 0, 0xff,
			   0, digital_vol_tlv),
	SOC_SINGLE_TLV("ADC3 Digital Volume", SUNXI_ADC3_DVOL_CTRL, 0, 0xff,
			   0, digital_vol_tlv),
	SOC_SINGLE_TLV("ADCL Digital Volume", SUNXI_ADCL_DVOL_CTRL, 0, 0xff,
			   0, digital_vol_tlv),
	SOC_SINGLE_TLV("ADCR Digital Volume", SUNXI_ADCR_DVOL_CTRL, 0, 0xff,
			   0, digital_vol_tlv),
};

/**************************** DAPM controls **********************************/
/* I2S TX Ch1 Mapping Mux */
static const char *i2s_tx_ch1_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch1_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL1, TX_CH1_MAP, 5, i2s_tx_ch1_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch1_map_mux =
SOC_DAPM_ENUM("I2S TX CH1 MUX", i2s_tx_ch1_map_mux_enum);


/* I2S TX Ch2 Mapping Mux */
static const char *i2s_tx_ch2_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"

};
static const struct soc_enum i2s_tx_ch2_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL1, TX_CH2_MAP, 5, i2s_tx_ch2_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch2_map_mux =
SOC_DAPM_ENUM("I2S TX CH2 MUX", i2s_tx_ch2_map_mux_enum);


/* I2S TX Ch3 Mapping Mux */
static const char *i2s_tx_ch3_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch3_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL2, TX_CH3_MAP, 5, i2s_tx_ch3_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch3_map_mux =
SOC_DAPM_ENUM("I2S TX CH3 MUX", i2s_tx_ch3_map_mux_enum);


/* I2S TX Ch4 Mapping Mux */
static const char *i2s_tx_ch4_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch4_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL2, TX_CH4_MAP, 5, i2s_tx_ch4_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch4_map_mux =
SOC_DAPM_ENUM("I2S TX CH4 MUX", i2s_tx_ch4_map_mux_enum);


/* I2S TX Ch5 Mapping Mux */
static const char *i2s_tx_ch5_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch5_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL3, TX_CH5_MAP, 5, i2s_tx_ch5_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch5_map_mux =
SOC_DAPM_ENUM("I2S TX CH5 MUX", i2s_tx_ch5_map_mux_enum);


/* I2S TX Ch6 Mapping Mux */
static const char *i2s_tx_ch6_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch6_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL3, TX_CH6_MAP, 5, i2s_tx_ch6_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch6_map_mux =
SOC_DAPM_ENUM("I2S TX CH6 MUX", i2s_tx_ch6_map_mux_enum);


/* I2S TX Ch7 Mapping Mux */
static const char *i2s_tx_ch7_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch7_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL4, TX_CH7_MAP, 5, i2s_tx_ch7_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch7_map_mux =
SOC_DAPM_ENUM("I2S TX CH7 MUX", i2s_tx_ch7_map_mux_enum);


/* I2S TX Ch8 Mapping Mux */
static const char *i2s_tx_ch8_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch8_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL4, TX_CH8_MAP, 5, i2s_tx_ch8_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch8_map_mux =
SOC_DAPM_ENUM("I2S TX CH8 MUX", i2s_tx_ch8_map_mux_enum);


/* I2S TX Ch9 Mapping Mux */
static const char *i2s_tx_ch9_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch9_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL5, TX_CH9_MAP, 5, i2s_tx_ch9_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch9_map_mux =
SOC_DAPM_ENUM("I2S TX CH9 MUX", i2s_tx_ch9_map_mux_enum);


/* I2S TX Ch10 Mapping Mux */
static const char *i2s_tx_ch10_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch10_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL5, TX_CH10_MAP, 5, i2s_tx_ch10_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch10_map_mux =
SOC_DAPM_ENUM("I2S TX CH10 MUX", i2s_tx_ch10_map_mux_enum);


/* I2S TX Ch11 Mapping Mux */
static const char *i2s_tx_ch11_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch11_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL6, TX_CH11_MAP, 5, i2s_tx_ch11_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch11_map_mux =
SOC_DAPM_ENUM("I2S TX CH11 MUX", i2s_tx_ch11_map_mux_enum);


/* I2S TX Ch12 Mapping Mux */
static const char *i2s_tx_ch12_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch12_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL6, TX_CH12_MAP, 5, i2s_tx_ch12_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch12_map_mux =
SOC_DAPM_ENUM("I2S TX CH12 MUX", i2s_tx_ch12_map_mux_enum);


/* I2S TX Ch13 Mapping Mux */
static const char *i2s_tx_ch13_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch13_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL7, TX_CH13_MAP, 5, i2s_tx_ch13_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch13_map_mux =
SOC_DAPM_ENUM("I2S TX CH13 MUX", i2s_tx_ch13_map_mux_enum);


/* I2S TX Ch14 Mapping Mux */
static const char *i2s_tx_ch14_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch14_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL7, TX_CH14_MAP, 5, i2s_tx_ch14_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch14_map_mux =
SOC_DAPM_ENUM("I2S TX CH14 MUX", i2s_tx_ch14_map_mux_enum);


/* I2S TX Ch15 Mapping Mux */
static const char *i2s_tx_ch15_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch15_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL8, TX_CH15_MAP, 5, i2s_tx_ch15_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch15_map_mux =
SOC_DAPM_ENUM("I2S TX CH15 MUX", i2s_tx_ch15_map_mux_enum);


/* I2S TX Ch16 Mapping Mux */
static const char *i2s_tx_ch16_map_mux_text[] = {
	"ADC1 Sample Switch", "ADC2 Sample Switch", "ADC3 Sample Switch",
	"ADCL Sample Switch", "ADCR Sample Switch"
};
static const struct soc_enum i2s_tx_ch16_map_mux_enum =
SOC_ENUM_SINGLE(SUNXI_I2S_TX_CHMP_CTRL8, TX_CH16_MAP, 5, i2s_tx_ch16_map_mux_text);
static const struct snd_kcontrol_new i2s_tx_ch16_map_mux =
SOC_DAPM_ENUM("I2S TX CH16 MUX", i2s_tx_ch16_map_mux_enum);


/******************************* DAPM widgets *********************************/
/* td100 dapm widgets */
static const struct snd_soc_dapm_widget td100_dapm_widgets[] = {
	//Input Widgets
	SND_SOC_DAPM_INPUT("MIC1P"),
	SND_SOC_DAPM_INPUT("MIC1N"),

	SND_SOC_DAPM_INPUT("MIC2P"),
	SND_SOC_DAPM_INPUT("MIC2N"),

	SND_SOC_DAPM_INPUT("MIC3P"),
	SND_SOC_DAPM_INPUT("MIC3N"),

	SND_SOC_DAPM_INPUT("LINEINL"),
	SND_SOC_DAPM_INPUT("LINEINR"),

	//MIC PGA
	SND_SOC_DAPM_PGA("MIC1 PGA", SUNXI_ANA_ADC1_CTRL, MIC1_PGA_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC2 PGA", SUNXI_ANA_ADC2_CTRL, MIC2_PGA_EN, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MIC3 PGA", SUNXI_ANA_ADC3_CTRL, MIC3_PGA_EN, 0, NULL, 0),

	SND_SOC_DAPM_ADC("ADC1", NULL, SUNXI_ANA_ADC1_CTRL, ADC1_EN, 0),
	SND_SOC_DAPM_ADC("ADC2", NULL, SUNXI_ANA_ADC2_CTRL, ADC2_EN, 0),
	SND_SOC_DAPM_ADC("ADC3", NULL, SUNXI_ANA_ADC3_CTRL, ADC3_EN, 0),
	SND_SOC_DAPM_ADC("ADCL", NULL, SUNXI_ANA_ADCL_CTRL, ADCL_EN, 0),
	SND_SOC_DAPM_ADC("ADCR", NULL, SUNXI_ANA_ADCR_CTRL, ADCR_EN, 0),

	//I2S TX CH1 MUX
	SND_SOC_DAPM_MUX("I2S TX CH1 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch1_map_mux),

	//I2S TX CH2 MUX
	SND_SOC_DAPM_MUX("I2S TX CH2 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch2_map_mux),

	//I2S TX CH3 MUX
	SND_SOC_DAPM_MUX("I2S TX CH3 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch3_map_mux),

	//I2S TX CH4 MUX
	SND_SOC_DAPM_MUX("I2S TX CH4 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch4_map_mux),

	//I2S TX CH5 MUX
	SND_SOC_DAPM_MUX("I2S TX CH5 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch5_map_mux),

	//I2S TX CH6 MUX
	SND_SOC_DAPM_MUX("I2S TX CH6 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch6_map_mux),

	//I2S TX CH7 MUX
	SND_SOC_DAPM_MUX("I2S TX CH7 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch7_map_mux),

	//I2S TX CH8 MUX
	SND_SOC_DAPM_MUX("I2S TX CH8 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch8_map_mux),

	//I2S TX CH9 MUX
	SND_SOC_DAPM_MUX("I2S TX CH9 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch9_map_mux),

	//I2S TX CH10 MUX
	SND_SOC_DAPM_MUX("I2S TX CH10 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch10_map_mux),

	//I2S TX CH11 MUX
	SND_SOC_DAPM_MUX("I2S TX CH11 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch11_map_mux),

	//I2S TX CH12 MUX
	SND_SOC_DAPM_MUX("I2S TX CH12 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch12_map_mux),

	//I2S TX CH13 MUX
	SND_SOC_DAPM_MUX("I2S TX CH13 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch13_map_mux),

	//I2S TX CH14 MUX
	SND_SOC_DAPM_MUX("I2S TX CH14 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch14_map_mux),

	//I2S TX CH15 MUX
	SND_SOC_DAPM_MUX("I2S TX CH15 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch15_map_mux),

	//I2S TX CH16 MUX
	SND_SOC_DAPM_MUX("I2S TX CH16 MUX", SND_SOC_NOPM, 0, 0,
			 &i2s_tx_ch16_map_mux),

	//AIF OUT -> (stream widget, stname must be same with codec dai_driver stream_name,
	//which will be used to build dai widget)
	SND_SOC_DAPM_AIF_OUT("AIF ADC OUT", "Capture", 0, SND_SOC_NOPM, 0, 0),
};

/******************************* DAPM routes ***********************************/
/* td100 dapm routes */
static const struct snd_soc_dapm_route td100_dapm_routes[] = {
	//MIC1 IN
	{"MIC1 PGA", NULL, "MIC1P"},
	{"MIC1 PGA", NULL, "MIC1N"},

	//MIC2 IN
	{"MIC2 PGA", NULL, "MIC2P"},
	{"MIC2 PGA", NULL, "MIC2N"},

	//MIC3 IN
	{"MIC3 PGA", NULL, "MIC3P"},
	{"MIC3 PGA", NULL, "MIC3N"},

	{"ADC1", NULL, "MIC1 PGA"},
	{"ADC2", NULL, "MIC2 PGA"},
	{"ADC3", NULL, "MIC3 PGA"},
	{"ADCL", NULL, "LINEINL"},
	{"ADCR", NULL, "LINEINR"},

	//I2S TX CH1 MUX
	{"I2S TX CH1 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH1 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH1 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH1 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH1 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH2 MUX
	{"I2S TX CH2 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH2 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH2 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH2 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH2 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH3 MUX
	{"I2S TX CH3 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH3 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH3 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH3 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH3 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH4 MUX
	{"I2S TX CH4 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH4 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH4 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH4 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH4 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH5 MUX
	{"I2S TX CH5 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH5 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH5 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH5 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH5 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH6 MUX
	{"I2S TX CH6 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH6 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH6 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH6 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH6 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH7 MUX
	{"I2S TX CH7 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH7 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH7 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH7 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH7 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH8 MUX
	{"I2S TX CH8 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH8 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH8 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH8 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH8 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH9 MUX
	{"I2S TX CH9 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH9 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH9 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH9 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH9 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH10 MUX
	{"I2S TX CH10 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH10 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH10 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH10 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH10 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH11 MUX
	{"I2S TX CH11 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH11 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH11 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH11 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH11 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH12 MUX
	{"I2S TX CH12 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH12 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH12 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH12 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH12 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH13 MUX
	{"I2S TX CH13 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH13 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH13 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH13 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH13 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH14 MUX
	{"I2S TX CH14 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH14 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH14 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH14 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH14 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH15 MUX
	{"I2S TX CH15 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH15 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH15 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH15 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH15 MUX", "ADCR Sample Switch", "ADCR"},

	//I2S TX CH16 MUX
	{"I2S TX CH16 MUX", "ADC1 Sample Switch", "ADC1"},
	{"I2S TX CH16 MUX", "ADC2 Sample Switch", "ADC2"},
	{"I2S TX CH16 MUX", "ADC3 Sample Switch", "ADC3"},
	{"I2S TX CH16 MUX", "ADCL Sample Switch", "ADCL"},
	{"I2S TX CH16 MUX", "ADCR Sample Switch", "ADCR"},

	//AIF ADC OUT
	{"AIF ADC OUT", NULL, "I2S TX CH1 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH2 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH3 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH4 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH5 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH6 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH7 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH8 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH9 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH10 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH11 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH12 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH13 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH14 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH15 MUX"},
	{"AIF ADC OUT", NULL, "I2S TX CH16 MUX"},
};

static int td100_read(u16 addr, u8 *value)
{
	addr = ((addr << 8) | (addr >> 8));

	return td100_cci_read_a16_d8(addr, (unsigned char *)value);
}

static int td100_write(u16 addr, u8 value)
{
	addr = ((addr << 8) | (addr >> 8));

	return td100_cci_write_a16_d8(addr, value);
}

static int td100_update_bits(u16 reg, u8 mask, u8 value)
{
	u8 val_old, val_new;

	td100_read(reg, &val_old);
	val_new = (val_old & ~mask) | (value & mask);
	if (val_new != val_old) {
		td100_write(reg, val_new);
	}

	return 0;
}

static void td100_hw_init(void)
{
	/*** Analog voltage enable ***/
	td100_write(SUNXI_ANA_MICBIAS_CTRL, 0x2D);	/*0x13=0x2D: MICBIAS1 Enable */

	/*** SYSCLK Config ***/
	/*SYSCLK Enable */
	td100_update_bits(SUNXI_SYSCLK_CTRL, 0x1 << SYSCLK_EN, 0x1 << SYSCLK_EN);
	/*0x21=0x07: Module clock enable<I2S, ADC digital,  ADC analog> */
	td100_write(SUNXI_MOD_CLK_EN, 0x07);
	/*0x22=0x03: Module reset de-asserted<I2S, ADC digital> */
	td100_write(SUNXI_MOD_RST_CTRL, 0x3);

	/*** I2S Common Config ***/
	/*DOUT enable */
	td100_update_bits(SUNXI_I2S_CTRL, 0x1 << DOUT_EN, 0x1 << DOUT_EN);

	/*SDO drive data and SDI sample data at the different BCLK edge */
	td100_update_bits(SUNXI_I2S_BCLK_CTRL,
			0x1 << EDGE_TRANSFER, 0x0 << EDGE_TRANSFER);

	/*
	 * config LRCK period:
	 * 16bit * 8ch = 128,
	 * 32bit * 8ch = 256,
	 * 32bit *16ch = 512
	 */
	/*config LRCK period */
	td100_update_bits(SUNXI_I2S_LRCK_CTRL1, 0x3 << LRCK_PERIODH,
			  ((TD100_LRCK_PERIOD - 1) >> 8) << LRCK_PERIODH);
	td100_write(SUNXI_I2S_LRCK_CTRL2, (u8) (TD100_LRCK_PERIOD - 1) & 0xFF);

	td100_update_bits(SUNXI_I2S_FMT_CTRL1,
		0x1 << TX_SLOT_HIZ | 0x1 << TX_STATE,
		0x0 << TX_SLOT_HIZ | 0x0 << TX_STATE);

	/*8/12/16/20/24/28/32bit Slot Width */
	td100_update_bits(SUNXI_I2S_FMT_CTRL2, 0x7 << SW,
			(TD100_SLOT_WIDTH / 4 - 1) << SW);

	/*0x36=0x60: TX MSB first, SDOUT normal, PCM frame type, Linear PCM Data Mode */
	td100_update_bits(SUNXI_I2S_FMT_CTRL3,
			0x1 << TX_MLS | 0x1 << DOUT_MUTE | 0x1 << LRCK_WIDTH
			| 0x3 << TX_PDM,
			0x0 << TX_MLS | 0x0 << DOUT_MUTE | 0x0 << LRCK_WIDTH
			| 0x0 << TX_PDM);

	/*0x66=0x1f: Digital ADCs channel HPF enable*/
	td100_write(SUNXI_HPF_EN, 0x1f);

	/*** Debug Mode About ***/
	/*
	 * 0X7F=0x00: ADC pattern select: 0:ADC normal, 1:0x5A5A5A,
	 * 2:0x123456, 3:0x00, 4~7:I2S RX data
	 */
	td100_update_bits(SUNXI_ADC_DIG_DEBUG, 0x7 << ADC_PTN_SEL,
			(TD100_ADC_PATTERN_SEL & 0x7) << ADC_PTN_SEL);

	/*** ADC DIG part Config***/
	/*0x61=0x01: Digital part globe enable, ADCs digital part enable */
	td100_write(SUNXI_ADC_DIG_EN, 0x1);

	td100_update_bits(SUNXI_ANA_ADC_DITHER_CTRL, 0x1 << MIC_ADC_DITHER_EN,
			  0x1 << MIC_ADC_DITHER_EN);
	td100_update_bits(SUNXI_ANA_ADC_DITHER_CTRL, 0x1 << LINEIN_ADC_DITHER_EN,
			  0x1 << LINEIN_ADC_DITHER_EN);

	td100_write(SUNXI_ANA_ADC_TUN1, 0x55);
	td100_write(SUNXI_ANA_ADC_TUN2, 0x55);

	td100_write(SUNXI_ANA_BIAS_CTRL1, 0xc0);
	td100_write(SUNXI_ANA_BIAS_CTRL2, 0x10);
	td100_write(SUNXI_ANA_BIAS_CTRL2, 0x12);

	/*** MICs Input Mode Select default Config ***/
	td100_update_bits(SUNXI_ANA_ADC1_CTRL, 0x1 << MIC1_MODE,
			  0x0 << MIC1_MODE);
	td100_update_bits(SUNXI_ANA_ADC2_CTRL, 0x1 << MIC2_MODE,
			  0x0 << MIC2_MODE);
	td100_update_bits(SUNXI_ANA_ADC3_CTRL, 0x1 << MIC3_MODE,
			  0x0 << MIC3_MODE);

	td100_update_bits(SUNXI_ANA_ADC1_CTRL, 0x1 << ADC1_EN,
			  0x1 << ADC1_EN);
	td100_update_bits(SUNXI_ANA_ADC2_CTRL, 0x1 << ADC2_EN,
			  0x1 << ADC2_EN);
	td100_update_bits(SUNXI_ANA_ADC3_CTRL, 0x1 << ADC3_EN,
			  0x1 << ADC3_EN);
	td100_update_bits(SUNXI_ANA_ADCL_CTRL, 0x1 << ADCL_EN,
			  0x1 << ADCL_EN);
	td100_update_bits(SUNXI_ANA_ADCR_CTRL, 0x1 << ADCR_EN,
			  0x1 << ADCR_EN);

	td100_update_bits(SUNXI_ANA_ADC1_CTRL, 0x1 << MIC1_PGA_EN,
			  0x1 << MIC1_PGA_EN);
	td100_update_bits(SUNXI_ANA_ADC2_CTRL, 0x1 << MIC2_PGA_EN,
			  0x1 << MIC2_PGA_EN);
	td100_update_bits(SUNXI_ANA_ADC3_CTRL, 0x1 << MIC3_PGA_EN,
			  0x1 << MIC3_PGA_EN);

	td100_update_bits(SUNXI_ANA_ADCL_CTRL, 0x7 << ADCL_MUTE,
			  0x7 << ADCL_MUTE);
	td100_update_bits(SUNXI_ANA_ADCR_CTRL, 0x7 << ADCR_MUTE,
			  0x7 << ADCR_MUTE);
}

static int td100_set_sysclk(struct snd_soc_dai *dai, int clk_id,
			    unsigned int freq, int dir)
{
	TD100_DEBUG("\n--->%s\n", __FUNCTION__);

	switch (clk_id) {
	case SYSCLK_SRC_MCLK:
		TD100_DEBUG("td100 SYSCLK source select MCLK\n\n");
		//System Clock Source Select MCLK
		td100_update_bits(SUNXI_SYSCLK_CTRL, 0x3 << SYSCLK_SRC, SYSCLK_SRC_MCLK << SYSCLK_SRC);
		break;
	case SYSCLK_SRC_BCLK:
		TD100_DEBUG("td100 SYSCLK source select BCLK\n\n");
		//System Clock Source Select BCLK
		td100_update_bits(SUNXI_SYSCLK_CTRL, 0x3 << SYSCLK_SRC, SYSCLK_SRC_BCLK << SYSCLK_SRC);
		break;
	default:
		pr_err("td100 SYSCLK source config error:%d\n\n", clk_id);
		return -EINVAL;
	}

	/* SYSCLK Enable */
	td100_update_bits(SUNXI_SYSCLK_CTRL, 0x1 << SYSCLK_EN,
				      0x1 << SYSCLK_EN);
	return 0;
}

static int td100_set_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	u32 i, bclk_div, bclk_div_reg_val;
	TD100_DEBUG("\n--->%s\n", __FUNCTION__);

	//use div_id to judge Master/Slave mode,  0: Slave mode, 1: Master mode
#if 0
	if (!div_id) {
		TD100_DEBUG
		    ("td100 work as Slave mode, don't need to config BCLK_DIV\n\n");
		return 0;
	}
#endif
	//default PCM mode
	bclk_div = div / (4 * TD100_LRCK_PERIOD);

	for (i = 0; i < ARRAY_SIZE(td100_bclk_div); i++) {
		if (td100_bclk_div[i].real_val == bclk_div) {
			bclk_div_reg_val = td100_bclk_div[i].reg_val;
			TD100_DEBUG("td100 set BCLK_DIV_[%u]\n\n", bclk_div);
			break;
		}
	}

	if (i == ARRAY_SIZE(td100_bclk_div)) {
		pr_err("td100 don't support BCLK_DIV_[%u]\n\n", bclk_div);
		return -EINVAL;
	}

	//td100 set BCLK DIV
	td100_update_bits(SUNXI_I2S_BCLK_CTRL, 0xf << BCLKDIV,
				      bclk_div_reg_val << BCLKDIV);
	return 0;
}

static int td100_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 tx_offset, i2s_mode, sign_ext, lrck_polarity, brck_polarity;

	TD100_DEBUG("\n--->%s\n", __FUNCTION__);

	//td100 config Master/Slave mode
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:		//td100 Master
		TD100_DEBUG("td100 set to work as Master\n");
		//BCLK & LRCK output
		td100_update_bits(SUNXI_I2S_CTRL, 0x3 << LRCK_IOEN, 0x3 << LRCK_IOEN);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:		//td100 Slave
		TD100_DEBUG("td100 set to work as Slave\n");
		//BCLK & LRCK input
		td100_update_bits(SUNXI_I2S_CTRL, 0x3 << LRCK_IOEN, 0x0 << LRCK_IOEN);
		break;
	default:
		pr_err("td100 Master/Slave mode config error:%u\n\n",
		       (fmt & SND_SOC_DAIFMT_MASTER_MASK) >> 12);
		return -EINVAL;
	}

	//td100 config I2S/LJ/RJ/PCM format
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		TD100_DEBUG("td100 config I2S format\n");
		i2s_mode = LEFT_JUSTIFIED_FORMAT;
		tx_offset = 1;
		sign_ext = TRANSFER_ZERO_AFTER;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		TD100_DEBUG("td100 config RIGHT-JUSTIFIED format\n");
		i2s_mode = RIGHT_JUSTIFIED_FORMAT;
		tx_offset = 0;
		sign_ext = SIGN_EXTENSION_MSB;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		TD100_DEBUG("td100 config LEFT-JUSTIFIED format\n");
		i2s_mode = LEFT_JUSTIFIED_FORMAT;
		tx_offset = 0;
		sign_ext = TRANSFER_ZERO_AFTER;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		TD100_DEBUG("td100 config PCM-A format\n");
		i2s_mode = PCM_FORMAT;
		tx_offset = 1;
		sign_ext = TRANSFER_ZERO_AFTER;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		TD100_DEBUG("td100 config PCM-B format\n");
		i2s_mode = PCM_FORMAT;
		tx_offset = 0;
		sign_ext = TRANSFER_ZERO_AFTER;
		break;
	default:
		pr_err("td100 I2S format config error:%u\n\n",
		       fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	td100_update_bits(SUNXI_I2S_FMT_CTRL1,
				0x3 << MODE_SEL | 0x1 << TX_OFFSET,
				i2s_mode << MODE_SEL | tx_offset << TX_OFFSET);
	sign_ext = 0;
	td100_update_bits(SUNXI_I2S_FMT_CTRL3, 0x3 << SEXT,
				      sign_ext << SEXT);

	//td100 config BCLK&LRCK polarity
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		TD100_DEBUG
		    ("td100 config BCLK&LRCK polarity: BCLK_normal,LRCK_normal\n");
		brck_polarity = BCLK_NORMAL_DRIVE_N_SAMPLE_P;
		lrck_polarity = LRCK_LEFT_LOW_RIGHT_HIGH;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		TD100_DEBUG
		    ("td100 config BCLK&LRCK polarity: BCLK_normal,LRCK_invert\n");
		brck_polarity = BCLK_NORMAL_DRIVE_N_SAMPLE_P;
		lrck_polarity = LRCK_LEFT_HIGH_RIGHT_LOW;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		TD100_DEBUG
		    ("td100 config BCLK&LRCK polarity: BCLK_invert,LRCK_normal\n");
		brck_polarity = BCLK_INVERT_DRIVE_P_SAMPLE_N;
		lrck_polarity = LRCK_LEFT_LOW_RIGHT_HIGH;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		TD100_DEBUG
		    ("td100 config BCLK&LRCK polarity: BCLK_invert,LRCK_invert\n");
		brck_polarity = BCLK_INVERT_DRIVE_P_SAMPLE_N;
		lrck_polarity = LRCK_LEFT_HIGH_RIGHT_LOW;
		break;
	default:
		pr_err("td100 config BCLK/LRCLK polarity error:%u\n\n",
		       (fmt & SND_SOC_DAIFMT_INV_MASK) >> 8);
		return -EINVAL;
	}

	td100_update_bits(SUNXI_I2S_BCLK_CTRL, 0x1 << BCLK_POLARITY,
				      brck_polarity << BCLK_POLARITY);

	td100_update_bits(SUNXI_I2S_LRCK_CTRL1, 0x1 << LRCK_POLARITY,
				      lrck_polarity << LRCK_POLARITY);

	return 0;
}

static int td100_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	u16 i, channels, sample_resolution;

	TD100_DEBUG("\n--->%s\n", __FUNCTION__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		TD100_DEBUG("TD100 not need playback.\n");
		return 0;
	}

	td100_hw_init();

	//td100 set sample rate
	for (i = 0; i < ARRAY_SIZE(td100_sample_rate); i++) {
		if (td100_sample_rate[i].real_val == params_rate(params)) {
			td100_update_bits(SUNXI_ADC_SPRC,
				0x7 << ADC_FS,
				td100_sample_rate[i].reg_val << ADC_FS);
			break;
		}
	}

	//td100 set channels
	channels = params_channels(params);
	td100_write(SUNXI_I2S_TX_CTRL1, channels - 1);
	td100_write(SUNXI_I2S_TX_CTRL2, (0xFF >> (0x8 - channels)));
	td100_write(SUNXI_I2S_TX_CTRL3, 0x00);

	//td100 set sample resorution
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		sample_resolution = 8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_resolution = 16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		sample_resolution = 20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		sample_resolution = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sample_resolution = 32;
		break;
	default:
		dev_err(dai->dev,
			"td100 don't supported the sample resolution: %u\n",
			params_format(params));
		return -EINVAL;
	}

	td100_update_bits(SUNXI_I2S_FMT_CTRL2, 0x7 << SR,
				      (sample_resolution / 4 -
				       1) << SR);

	//td100 TX enable, Globle enable
	td100_update_bits(SUNXI_I2S_CTRL, 0x1 << TX_EN | 0x1 << GEN,
				      0x1 << TX_EN | 0x1 << GEN);

	//SYSCLK Select
	//MCLK input Clock 24MHz
	td100_update_bits(SUNXI_I2S_CTRL, 0x1 << MCLK_IOEN,
			  0x0 << MCLK_IOEN);
	td100_update_bits(SUNXI_I2S_PADDRV_CTRL, 0x03 << PADDRV_MCLK,
			  0x03 << PADDRV_MCLK);

	return 0;
}

static int td100_hw_free(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	TD100_DEBUG("\n--->%s\n", __FUNCTION__);

	//td100 I2S Globle disable
	td100_update_bits(SUNXI_I2S_CTRL, 0x1 << GEN, 0x0 << GEN);

	return 0;
}

/*** define  td100  dai_ops  struct ***/
static const struct snd_soc_dai_ops td100_dai_ops = {
	/*DAI clocking configuration */
	.set_sysclk = td100_set_sysclk,
	.set_clkdiv = td100_set_clkdiv,

	/*ALSA PCM audio operations */
	.hw_params = td100_hw_params,
	.hw_free = td100_hw_free,

	/*DAI format configuration */
	.set_fmt = td100_set_fmt,
};

/*** define  td100  dai_driver struct ***/
static struct snd_soc_dai_driver td100_codec_dai = {
	.name = "td100-pcm",
	.capture = {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 5,
		.rates		= SNDRV_PCM_RATE_8000_48000
				| SNDRV_PCM_RATE_KNOT,
		.formats	= SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S20_3LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &td100_dai_ops,
};

static int td100_probe(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm = &component->dapm;
	int ret = 0;

	/*** TD100 System Config About ***/
	td100_write(0x0002, 0x01);
	td100_write(0x0008, 0x01);
	td100_write(0x0014, 0x03);

	/* Add codec component */
	ret = snd_soc_add_component_controls(component, td100_controls, ARRAY_SIZE(td100_controls));
	if (ret)
		dev_err(component->dev, "register codec kcontrols failed %d\n", ret);
	snd_soc_dapm_new_controls(dapm, td100_dapm_widgets,
					ARRAY_SIZE(td100_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, td100_dapm_routes,
					ARRAY_SIZE(td100_dapm_routes));

	/* Init the MIC Gain to the max value */
	td100_update_bits(SUNXI_ANA_ADC1_CTRL, 0x7 << MIC1_PGA_GAIN_CTRL,
			  0x7 << MIC1_PGA_GAIN_CTRL);
	td100_update_bits(SUNXI_ANA_ADC2_CTRL, 0x7 << MIC2_PGA_GAIN_CTRL,
			  0x7 << MIC2_PGA_GAIN_CTRL);
	td100_update_bits(SUNXI_ANA_ADC3_CTRL, 0x7 << MIC3_PGA_GAIN_CTRL,
			  0x7 << MIC3_PGA_GAIN_CTRL);
	td100_update_bits(SUNXI_ANA_ADCL_CTRL, 0x7 << LINEIN_GAIN,
			  0x7 << LINEIN_GAIN);

	return 0;
}

static void td100_remove(struct snd_soc_component *component)
{
}

static int td100_suspend(struct snd_soc_component *component)
{
	return 0;
}

static int td100_resume(struct snd_soc_component *component)
{
	/*** TD100 System Config About ***/
	td100_write(0x0002, 0x01);
	td100_write(0x0008, 0x01);
	td100_write(0x0014, 0x03);

	return 0;
}

static unsigned int td100_codec_read(struct snd_soc_component *component, unsigned int reg)
{
	unsigned int reg_val;

	reg = ((reg << 8) | (reg >> 8));

	td100_cci_read_a16_d8(reg, (unsigned char *)&reg_val);

	return reg_val;
}

static int td100_codec_write(struct snd_soc_component *component,
			     unsigned int reg, unsigned int value)
{
	reg = ((reg << 8) | (reg >> 8));

	td100_cci_write_a16_d8(reg, value);

	return 0;
}

/*** define  td100  codec_driver struct ***/
static const struct snd_soc_component_driver td100_soc_codec_driver = {
	.probe		= td100_probe,
	.remove		= td100_remove,
	.suspend	= td100_suspend,
	.resume		= td100_resume,
/* User R/W Access */
	.read		= td100_codec_read,
	.write		= td100_codec_write,
};

static ssize_t td100_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int val = 0, flag = 0;
	u8 i = 0, reg, num, value_w, value_r;

	val = simple_strtol(buf, NULL, 16);
	flag = (val >> 16) & 0xFF;

	if (flag) {
		reg = (val >> 8) & 0xFF;
		value_w = val & 0xFF;
		printk("\nWrite: start REG:0x%02x,val:0x%02x,count:0x%02x\n",
		       reg, value_w, flag);
		while (flag--) {
			td100_write(((u16)reg + SUNXI_CODEC_BASE_ADDR), value_w);
//			td100_write(reg, value_w);
			printk("Write 0x%02x to REG:0x%02x\n", value_w, reg);
			reg++;
		}
	} else {
		reg = (val >> 8) & 0xFF;
		num = val & 0xff;
		printk("\nRead: start REG:0x%02x,count:0x%02x\n", reg, num);

		do {
			value_r = 0;
			td100_read(((u16)reg + SUNXI_CODEC_BASE_ADDR), &value_r);
//			td100_read(reg, &value_r);
			printk("REG[0x%02x]: 0x%02x;  ", reg, value_r);
			reg++;
			i++;
			if ((i == num) || (i % 4 == 0))
				printk("\n");
		} while (i < num);
	}

	return count;
}

static ssize_t td100_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	printk("\n/*** td100 driver version: V1.0 ***/\n");
	printk("echo flag|reg|val > td100\n");
	printk("eg->read start addres=0x00,count=0xff: echo 00ff >td100\n");
	printk("eg write value:0xfe to address:0x06 :echo 106fe > td100\n");

	return 0;
}

static DEVICE_ATTR(td100, 0644, td100_show, td100_store);

static struct attribute *td100_debug_attrs[] = {
	&dev_attr_td100.attr,
	NULL,
};

static struct attribute_group td100_debug_attr_group = {
	.name  = "td100_debug",
	.attrs = td100_debug_attrs,
};

static int td100_codec_dev_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = snd_soc_register_component(&pdev->dev, &td100_soc_codec_driver, &td100_codec_dai, 1);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed register td100: %d\n", ret);

	ret  = sysfs_create_group(&pdev->dev.kobj, &td100_debug_attr_group);
	if (ret)
		dev_warn(&pdev->dev, "failed to create attr group\n");

	return 0;
}

static int td100_codec_dev_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &td100_debug_attr_group);
	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static const struct of_device_id td100_of_match[] = {
	{ .compatible = "Allwinnertech,td100_audio", },
	{},
};
MODULE_DEVICE_TABLE(of, td100_of_match);

static struct platform_driver td100_codec_driver = {
	.driver	= {
		.name		= "td100-codec",
		.owner		= THIS_MODULE,
		.of_match_table	= td100_of_match,
	},
	.probe	= td100_codec_dev_probe,
	.remove	= td100_codec_dev_remove,
};

static int __init td100_codec_driver_init(void)
{
	if (platform_driver_register(&td100_codec_driver)) {
		pr_err("register td100 codec driver fail.\n");
		return -1;
	}

	return 0;
}

static void __exit td100_codec_driver_exit(void)
{
	platform_driver_unregister(&td100_codec_driver);
}
late_initcall(td100_codec_driver_init);
module_exit(td100_codec_driver_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SUNXI ASoC td100 Codec Driver");
MODULE_AUTHOR("Lu");
MODULE_ALIAS("platform:td100-codec");
