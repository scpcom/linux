/*
 * td100.h --  Allwinnertech TD100 Codec ALSA Soc Audio driver
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
#ifndef _TD100_CODEC_H
#define _TD100_CODEC_H

#define SUNXI_CODEC_BASE_ADDR		0x1000

#define SUNXI_ANA_ADC1_CTRL		0x1000
#define SUNXI_ANA_ADC2_CTRL		0x1001
#define SUNXI_ANA_ADC3_CTRL		0x1002
#define SUNXI_ANA_ADC_DITHER_CTRL	0x1008
#define SUNXI_ANA_ADCL_CTRL		0x1009
#define SUNXI_ANA_ADCR_CTRL		0x100A
#define SUNXI_ANA_ADC_TUN1		0x100B
#define SUNXI_ANA_ADC_TUN2		0x100C
#define SUNXI_ANA_BIAS_CTRL1		0x1010
#define SUNXI_ANA_BIAS_CTRL2		0x1011
#define SUNXI_ANA_BIAS_CTRL3		0x1012
#define SUNXI_ANA_MICBIAS_CTRL		0x1013

#define SUNXI_SYSCLK_CTRL		0x1020
#define SUNXI_MOD_CLK_EN		0x1021
#define SUNXI_MOD_RST_CTRL		0x1022

#define SUNXI_I2S_CTRL			0x1030
#define SUNXI_I2S_BCLK_CTRL		0x1031
#define SUNXI_I2S_LRCK_CTRL1		0x1032
#define SUNXI_I2S_LRCK_CTRL2		0x1033
#define SUNXI_I2S_FMT_CTRL1		0x1034
#define SUNXI_I2S_FMT_CTRL2		0x1035
#define SUNXI_I2S_FMT_CTRL3		0x1036

#define SUNXI_I2S_TX_CTRL1		0x1038
#define SUNXI_I2S_TX_CTRL2		0x1039
#define SUNXI_I2S_TX_CTRL3		0x103A
#define SUNXI_I2S_TX_CHMP_CTRL1		0x103C
#define SUNXI_I2S_TX_CHMP_CTRL2		0x103D
#define SUNXI_I2S_TX_CHMP_CTRL3		0x103E
#define SUNXI_I2S_TX_CHMP_CTRL4		0x103F
#define SUNXI_I2S_TX_CHMP_CTRL5		0x1040
#define SUNXI_I2S_TX_CHMP_CTRL6		0x1041
#define SUNXI_I2S_TX_CHMP_CTRL7		0x1042
#define SUNXI_I2S_TX_CHMP_CTRL8		0x1043

#define SUNXI_I2S_RX_CTRL1		0x1050
#define SUNXI_I2S_RX_CTRL2		0x1051
#define SUNXI_I2S_RX_CTRL3		0x1052
#define SUNXI_I2S_RX_CHMP_CTRL1		0x1054
#define SUNXI_I2S_RX_CHMP_CTRL2		0x1055
#define SUNXI_I2S_RX_CHMP_CTRL3		0x1056
#define SUNXI_I2S_RX_CHMP_CTRL4		0x1057
#define SUNXI_I2S_RX_CHMP_CTRL5		0x1058
#define SUNXI_I2S_RX_CHMP_CTRL6		0x1059
#define SUNXI_I2S_RX_CHMP_CTRL7		0x105A
#define SUNXI_I2S_RX_CHMP_CTRL8		0x105B

#define SUNXI_ADC_SPRC			0x1060
#define SUNXI_ADC_DIG_EN		0x1061

#define SUNXI_HPF_EN			0x1066
#define SUNXI_HPF_COE_CTRL1		0x1067
#define SUNXI_HPF_COE_CTRL2		0x1068
#define SUNXI_HPF_COE_CTRL3		0x1069
#define SUNXI_HPF_COE_CTRL4		0x106A
#define SUNXI_HPF_GAIN_COE_CTRL1	0x106B
#define SUNXI_HPF_GAIN_COE_CTRL2	0x106C
#define SUNXI_HPF_GAIN_COE_CTRL3	0x106D
#define SUNXI_HPF_GAIN_COE_CTRL4	0x106E

#define SUNXI_ADC1_DVOL_CTRL		0x1070
#define SUNXI_ADC2_DVOL_CTRL		0x1071
#define SUNXI_ADC3_DVOL_CTRL		0x1072
#define SUNXI_ADCL_DVOL_CTRL		0x1073
#define SUNXI_ADCR_DVOL_CTRL		0x1074

#define SUNXI_ADC_DIG_DEBUG		0x107F
#define SUNXI_ADC_ANA_DEBUG1		0x1080
#define SUNXI_ADC_ANA_DEBUG2		0x1081
#define SUNXI_I2S_PADDRV_CTRL		0x1082
#define SUNXI_DIG_ADC_CTRL		0x1083

#define TD100_REG_MAX			0x1083




/* SUNXI_ANA_ADC1_CTRL: 0x00h */
#define ADC1_EN				7
#define MIC1_PGA_EN			6
#define MIC1_MODE			5
#define MIC1_PGA_GAIN_CTRL		0

/* SUNXI_ANA_ADC2_CTRL: 0x01h */
#define ADC2_EN				7
#define MIC2_PGA_EN			6
#define MIC2_MODE			5
#define MIC2_PGA_GAIN_CTRL		0

/* SUNXI_ANA_ADC3_CTRL: 0x02h */
#define ADC3_EN				7
#define MIC3_PGA_EN			6
#define MIC3_MODE			5
#define MIC3_PGA_GAIN_CTRL		0

/* SUNXI_ANA_ADC_DITHER_CTRL: 0x08h */
#define MIC_ADC_DITHER_EN		7
#define MIC_ADC_DITHER_SEL		5
#define IOPADACS			4
#define LINEIN_ADC_DITHER_EN		2
#define LINEADC_DITHER_CLK		0

/* SUNXI_ANA_ADCL_CTRL: 0x09h */
#define ADCL_EN				7
#define LINEIN_GAIN			4
#define ADCL_MUTE			0

/* SUNXI_ANA_ADCR_CTRL: 0x0Ah */
#define ADCR_EN				7
#define ADCR_MUTE			0

/* SUNXI_ANA_ADC_TUN1: 0x0Bh */
#define MIC_ADC_IOP1			6
#define MIC_ADC_IOP2			4
#define MIC_ADC_IOPMIC			2
#define MIC_ADC_IOPMIX			0

/* SUNXI_ANA_ADC_TUN2: 0x0Ch */
#define LINEIN_ADC_IOP1			6
#define LINEIN_ADC_IOP2			4
#define LINEIN_ADC_IOPAAF		2
#define LINEIN_ADC_IOPMIX		0

/* SUNXI_ANA_BIAS_CTRL1: 0x10h */
#define BG_EN				7
#define ALDO_EN				6
#define BGV				0

/* SUNXI_ANA_BIAS_CTRL2: 0x11h */
#define BDTC				0

/* SUNXI_ANA_BIAS_CTRL3: 0x12h */
#define BG_RESBYS_EN			7
#define ITEST_EN			6
#define OPVRS				4
#define SYS_CURRENT_TUNING		0

/* SUNXI_ANA_MICBIAS_CTRL: 0x13h */
#define CHOPPERCK			4
#define MICBIAS_EN			3
#define CHOPPER_EN			2
#define MICBIAS_VOL			0

/* SUNXI_SYSCLK_CTRL: 0x20h */
#define SYSCLK_SRC			2
#define SYSCLK_EN			0

/* SUNXI_MOD_CLK_EN: 0x21h */
#define MOD_CLK_EN			0

/* SUNXI_MOD_RST_CTRL: 0x22h */
#define GLOBE_RST_EN			4
#define MOD_RST_CTRL			0

/* SUNXI_I2S_CTRL: 0x30h */
#define BCLK_IOEN			7
#define LRCK_IOEN			6
#define MCLK_IOEN			5
#define DOUT_EN				4
#define TX_EN				2
#define RX_EN				1
#define GEN				0

/* SUNXI_I2S_BCLK_CTRL: 0x31h */
#define EDGE_TRANSFER			5
#define BCLK_POLARITY			4
#define BCLKDIV				0

/* SUNXI_I2S_LRCK_CTRL1: 0x32h */
#define LRCK_POLARITY			4
#define LRCK_PERIODH			0

/* SUNXI_I2S_LRCK_CTRL2: 0x33h */
#define LRCK_PERIODL			0

/* SUNXI_I2S_FMT_CTRL1: 0x34h */
#define MODE_SEL			4
#define TX_OFFSET			2
#define TX_SLOT_HIZ			1
#define TX_STATE			0

/* SUNXI_I2S_FMT_CTRL2: 0x35h */
#define SW				4
#define SR				0

/* SUNXI_I2S_FMT_CTRL3: 0x36h */
#define TX_MLS				7
#define SEXT				5
#define DOUT_MUTE			3
#define LRCK_WIDTH			2
#define TX_PDM				0

/* SUNXI_I2S_TX_CTRL1: 0x38h */
#define TX_CHSEL			0

/* SUNXI_I2S_TX_CTRL2: 0x39h */
#define TX_CHEN_LOW			0

/* SUNXI_I2S_TX_CTRL3: 0x3Ah */
#define TX_CHEN_HIGH			0

/* SUNXI_I2S_TX_CHMP_CTRL1: 0x3Ch */
#define TX_CH2_MAP			4
#define TX_CH1_MAP			0

/* SUNXI_I2S_TX_CHMP_CTRL2: 0x3Dh */
#define TX_CH4_MAP			4
#define TX_CH3_MAP			0

/* SUNXI_I2S_TX_CHMP_CTRL3: 0x3Eh */
#define TX_CH6_MAP			4
#define TX_CH5_MAP			0

/* SUNXI_I2S_TX_CHMP_CTRL4: 0x3Fh */
#define TX_CH8_MAP			4
#define TX_CH7_MAP			0

/* SUNXI_I2S_TX_CHMP_CTRL5: 0x40h */
#define TX_CH10_MAP			4
#define TX_CH9_MAP			0

/* SUNXI_I2S_TX_CHMP_CTRL6: 0x41h */
#define TX_CH12_MAP			4
#define TX_CH11_MAP			0

/* SUNXI_I2S_TX_CHMP_CTRL7: 0x42h */
#define TX_CH14_MAP			4
#define TX_CH13_MAP			0

/* SUNXI_I2S_TX_CHMP_CTRL8: 0x43h */
#define TX_CH16_MAP			4
#define TX_CH15_MAP			0

/* SUNXI_I2S_RX_CTRL1: 0x50h */
#define RX_CHSEL			0

/* SUNXI_I2S_RX_CTRL2: 0x51h */
#define RX_CHEN_LOW			0

/* SUNXI_I2S_RX_CTRL3: 0x52h */
#define RX_CHEN_HIGH			0

/* SUNXI_I2S_RX_CHMP_CTRL1: 0x54h */
#define RX_CH2_MAP			4
#define RX_CH1_MAP			0

/* SUNXI_I2S_RX_CHMP_CTRL2: 0x55h */
#define RX_CH4_MAP			4
#define RX_CH3_MAP			0

/* SUNXI_I2S_RX_CHMP_CTRL3: 0x56h */
#define RX_CH6_MAP			4
#define RX_CH5_MAP			0

/* SUNXI_I2S_RX_CHMP_CTRL4: 0x57h */
#define RX_CH8_MAP			4
#define RX_CH7_MAP			0

/* SUNXI_I2S_RX_CHMP_CTRL5: 0x58h */
#define RX_CH10_MAP			4
#define RX_CH9_MAP			0

/* SUNXI_I2S_RX_CHMP_CTRL6: 0x59h */
#define RX_CH12_MAP			4
#define RX_CH11_MAP			0

/* SUNXI_I2S_RX_CHMP_CTRL7: 0x5Ah */
#define RX_CH14_MAP			4
#define RX_CH13_MAP			0

/* SUNXI_I2S_RX_CHMP_CTRL8: 0x5Bh */
#define RX_CH16_MAP			4
#define RX_CH15_MAP			0

/* SUNXI_ADC_SPRC: 0x60h */
#define AD_SWAP3			5
#define AD_SWAP2			4
#define AD_SWAP1			3
#define ADC_FS				0

/* SUNXI_ADC_DIG_EN: 0x61h */
#define REQ_WIDTH			4
#define REQ_EN				3
#define ADC_DG_EN			0

/* SUNXI_HPF_EN: 0x66h */
#define	DIG_ADCR_HPF_EN			4
#define	DIG_ADCL_HPF_EN			3
#define	DIG_ADC3_HPF_EN			2
#define	DIG_ADC2_HPF_EN			1
#define	DIG_ADC1_HPF_EN			0

/* SUNXI_HPF_COE_CTRL1: 0x67h */
#define HPF_COE1			0

/* SUNXI_HPF_COE_CTRL2: 0x68h */
#define HPF_COE2			0

/* SUNXI_HPF_COE_CTRL3: 0x69h */
#define HPF_COE3			0

/* SUNXI_HPF_COE_CTRL4: 0x6Ah */
#define HPF_COE4			0

/* SUNXI_HPF_GAIN_COE_CTRL1: 0x6Bh */
#define HPF_GAIN_COE1			0

/* SUNXI_HPF_GAIN_COE_CTRL2: 0x6Ch */
#define HPF_GAIN_COE2			0

/* SUNXI_HPF_GAIN_COE_CTRL3: 0x6Dh */
#define HPF_GAIN_COE3			0

/* SUNXI_HPF_GAIN_COE_CTRL4: 0x6Eh */
#define HPF_GAIN_COE4			0

/* SUNXI_ADC1_DVOL_CTRL: 0x70h */
#define DIG_ADC1_VOL			0

/* SUNXI_ADC2_DVOL_CTRL: 0x71h */
#define DIG_ADC2_VOL			0

/* SUNXI_ADC3_DVOL_CTRL: 0x72h */
#define DIG_ADC3_VOL			0

/* SUNXI_ADCL_DVOL_CTRL: 0x73h */
#define DIG_ADCL_VOL			0

/* SUNXI_ADCR_DVOL_CTRL: 0x74h */
#define DIG_ADCR_VOL			0

/* SUNXI_ADC_DIG_DEBUG: 0x7Fh */
#define I2S_LPB_DEBUG			3
#define ADC_PTN_SEL			0

/* SUNXI_ADC_ANA_DEBUG1: 0x80h */
#define ADC_CLK_DEBUG			0

/* SUNXI_ADC_ANA_DEBUG2: 0x81h */
#define ADC1_CTRL_DEBUG			4
#define ADC0_CTRL_DEBUG			0

/* SUNXI_I2S_PADDRV_CTRL: 0x82h */
#define PADDRV_MCLK			6
#define PADDRV_BCLK			4
#define PADDRV_LRCK			2
#define PADDRV_DOUT			0

/* SUNXI_DIG_ADC_CTRL: 0x83h */



/*===========================================================================*/
/*** Some Config Value ***/
//SYSCLK_SRC
#define SYSCLK_SRC_MCLK				0
#define SYSCLK_SRC_BCLK				1

//I2S BCLK POLARITY Control
#define BCLK_NORMAL_DRIVE_N_SAMPLE_P		0
#define BCLK_INVERT_DRIVE_P_SAMPLE_N		1

//I2S LRCK POLARITY Control
#define	LRCK_LEFT_LOW_RIGHT_HIGH		0
#define LRCK_LEFT_HIGH_RIGHT_LOW		1

//I2S Format Selection
#define PCM_FORMAT				0
#define LEFT_JUSTIFIED_FORMAT			1
#define RIGHT_JUSTIFIED_FORMAT			2

//I2S Sign Extend in slot
#define ZERO_OR_AUDIIO_GAIN_PADDING_LSB		0
#define SIGN_EXTENSION_MSB			1
#define TRANSFER_ZERO_AFTER			3

//ADC Digital Debug Control
#define ADC_PTN_NORMAL				0
#define ADC_PTN_0x5A5A5A			1
#define ADC_PTN_0x123456			2
#define ADC_PTN_ZERO				3
#define ADC_PTN_I2S_RX_DATA			4


/*===========================================================================*/
/* Some Configurations Default Defined */
#define TD100_DEBUG_EN		0

#if TD100_DEBUG_EN
#define TD100_DEBUG(...)		printk(__VA_ARGS__)
#else
#define TD100_DEBUG(...)
#endif

/* 0:ADC normal,  1:0x5A5A5A,  2:0x123456,  3:0x000000,  4~7:I2S_RX_DATA,  other:reserved */
#define TD100_ADC_PATTERN_SEL	ADC_PTN_NORMAL

/* 8/12/16/20/24/28/32bit Slot Width */
#define TD100_SLOT_WIDTH		32
/*range[1, 1024], default PCM mode, I2S/LJ/RJ mode shall divide by 2 */
#define TD100_LRCK_PERIOD		256

#endif /* __TD100_CODEC_H */
