/*
 * drivers/video/fbdev/sunxi/disp2/disp/lcd/nv3052c_rgb.c
 *
 * Copyright (c) 2018-2021 Allwinnertech Co., Ltd.
 * Author: zepan <zepan@sipeed.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

&lcd0 {
	lcd_used            = <1>;

	lcd_driver_name     = "nv3052c_rgb";
	lcd_backlight       = <100>;
	lcd_if              = <2>;

	lcd_x               = <480>;
	lcd_y               = <480>;
	lcd_width           = <70>;
	lcd_height          = <72>;
	lcd_dclk_freq       = <30>;

	lcd_pwm_used        = <1>;
	lcd_pwm_ch          = <7>;
	lcd_pwm_freq        = <19000>;
	lcd_pwm_pol         = <0>;
	lcd_pwm_max_limit   = <255>;

	lcd_hbp             = <60>;
	lcd_ht              = <612>;
	lcd_hspw            = <12>;
	lcd_vbp             = <18>;
	lcd_vt              = <520>;
	lcd_vspw            = <4>;

	lcd_dsi_if          = <0>;
	lcd_dsi_lane        = <4>;
	lcd_lvds_if         = <0>;
	lcd_lvds_colordepth = <0>;
	lcd_lvds_mode       = <0>;
	lcd_frm             = <0>;
	lcd_hv_clk_phase    = <0>;
	lcd_hv_sync_polarity= <0>;
	lcd_io_phase        = <0x0000>;
	lcd_gamma_en        = <0>;
	lcd_bright_curve_en = <0>;
	lcd_cmap_en         = <0>;
	lcd_fsync_en        = <0>;
	lcd_fsync_act_time  = <1000>;
	lcd_fsync_dis_time  = <1000>;
	lcd_fsync_pol       = <0>;

	deu_mode            = <0>;
	lcdgamma4iep        = <22>;
	smart_color         = <90>;

	lcd_gpio_0 = <&pio PG 13 GPIO_ACTIVE_HIGH>;RST
	lcd_gpio_1 = <&pio PE 14 GPIO_ACTIVE_HIGH>;CS
	lcd_gpio_2 = <&pio PE 12 GPIO_ACTIVE_HIGH>;SDA
	lcd_gpio_3 = <&pio PE 15 GPIO_ACTIVE_HIGH>;SCK
	pinctrl-0 = <&rgb18_pins_a>;
	pinctrl-1 = <&rgb18_pins_b>;
};
 */
#include "nv3052c_rgb.h"
#include "default_panel.h"

//s32 sunxi_lcd_gpio_set_value(u32 screen_id, u32 io_index, u32 value)

#define nv3052c_spi_scl_1   sunxi_lcd_gpio_set_value(0, 3, 1)
#define nv3052c_spi_scl_0   sunxi_lcd_gpio_set_value(0, 3, 0)
#define nv3052c_spi_sdi_1   sunxi_lcd_gpio_set_value(0, 2, 1)
#define nv3052c_spi_sdi_0   sunxi_lcd_gpio_set_value(0, 2, 0)
#define nv3052c_spi_cs_1    sunxi_lcd_gpio_set_value(0, 1, 1)
#define nv3052c_spi_cs_0    sunxi_lcd_gpio_set_value(0, 1, 0)
#define nv3052c_spi_reset_1 sunxi_lcd_gpio_set_value(0, 0, 1)
#define nv3052c_spi_reset_0 sunxi_lcd_gpio_set_value(0, 0, 0)

static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

static void LCD_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		/* {input value, corrected value} */
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
		{
		 {LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
		 {LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
		 {LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
		 },
		{
		 {LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
		 {LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
		 {LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
		 },
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value =
			    lcd_gamma_tbl[i][1] +
			    ((lcd_gamma_tbl[i + 1][1] -
			      lcd_gamma_tbl[i][1]) * j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
			    (value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] =
	    (lcd_gamma_tbl[items - 1][1] << 16) +
	    (lcd_gamma_tbl[items - 1][1] << 8) + lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 LCD_open_flow(u32 sel)
{
    printk("=====================LCD_open_flow\n");
	/* open lcd power, and delay 50ms */
	LCD_OPEN_FUNC(sel, LCD_power_on, 20);
	/* open lcd power, than delay 200ms */
	LCD_OPEN_FUNC(sel, LCD_panel_init, 20);
	/* open lcd controller, and delay 100ms */
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);
	/* open lcd backlight, and delay 0ms */
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	/* close lcd backlight, and delay 0ms */
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 0);
	/* close lcd controller, and delay 0ms */
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	/* open lcd power, than delay 200ms */
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 200);
	/* close lcd power, and delay 500ms */
	LCD_CLOSE_FUNC(sel, LCD_power_off, 500);

	return 0;
}

static void LCD_power_on(u32 sel)
{
    printk("=====================LCD_power_on\n");
	/* config lcd_power pin to open lcd power0 */
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_pin_cfg(sel, 1);

}

static void LCD_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	/* config lcd_power pin to close lcd power0 */
	sunxi_lcd_power_disable(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
    printk("=====================LCD_bl_open\n");
	sunxi_lcd_pwm_enable(sel);
	sunxi_lcd_backlight_enable(sel);
}

static void LCD_bl_close(u32 sel)
{
	/* config lcd_bl_en pin to close lcd backlight */
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

//three line 9bit mode
static void LCD_WRITE_DATA(u32 value)
{
	u32 i;
	nv3052c_spi_cs_0;
	nv3052c_spi_sdi_1;
	nv3052c_spi_scl_0;
	sunxi_lcd_delay_us(10);
	nv3052c_spi_scl_1;
	for (i = 0; i < 8; i++) {
		sunxi_lcd_delay_us(10);
		if (value & 0x80)
			nv3052c_spi_sdi_1;
		else
			nv3052c_spi_sdi_0;
		value <<= 1;
		sunxi_lcd_delay_us(10);
		nv3052c_spi_scl_0;
		nv3052c_spi_scl_1;
	}
	sunxi_lcd_delay_us(10);
	nv3052c_spi_cs_1;
}

static void LCD_WRITE_COMMAND(u32 value)
{
	u32 i;
	nv3052c_spi_cs_0;
	nv3052c_spi_sdi_0;
	nv3052c_spi_scl_0;
	sunxi_lcd_delay_us(10);
	nv3052c_spi_scl_1;
	for (i = 0; i < 8; i++) {
		sunxi_lcd_delay_us(10);
		if (value & 0x80)
			nv3052c_spi_sdi_1;
		else
			nv3052c_spi_sdi_0;
		nv3052c_spi_scl_0;
		sunxi_lcd_delay_us(10);
		nv3052c_spi_scl_1;
		value <<= 1;
	}
	sunxi_lcd_delay_us(10);
	nv3052c_spi_cs_1;
}


static void LCD_panel_init(u32 sel)
{
    printk("=====================LCD_panel_init\n");
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x30);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x52);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x01);
	LCD_WRITE_COMMAND(0xE3);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0xF6);
	LCD_WRITE_DATA(0xC0);
	LCD_WRITE_COMMAND(0xF0);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x0A);
	LCD_WRITE_DATA(0x11);
	LCD_WRITE_COMMAND(0x23);
	LCD_WRITE_DATA(0xA2);
	LCD_WRITE_COMMAND(0x25);
	LCD_WRITE_DATA(0x14);
	LCD_WRITE_COMMAND(0x26);
	LCD_WRITE_DATA(0x2E);
	LCD_WRITE_COMMAND(0x27);
	LCD_WRITE_DATA(0x2E);
	LCD_WRITE_COMMAND(0x29);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0x2A);
	LCD_WRITE_DATA(0xCF);
	LCD_WRITE_COMMAND(0x32);
	LCD_WRITE_DATA(0x34);
	LCD_WRITE_COMMAND(0x38);
	LCD_WRITE_DATA(0x9C);
	LCD_WRITE_COMMAND(0x39);
	LCD_WRITE_DATA(0xA7);
	LCD_WRITE_COMMAND(0x3A);
	LCD_WRITE_DATA(0x73); 
	LCD_WRITE_COMMAND(0x3B);
	LCD_WRITE_DATA(0x94);
	LCD_WRITE_COMMAND(0x40);
	LCD_WRITE_DATA(0x07);
	LCD_WRITE_COMMAND(0x42);
	LCD_WRITE_DATA(0x6D);
	LCD_WRITE_COMMAND(0x43);
	LCD_WRITE_DATA(0x83);
	LCD_WRITE_COMMAND(0x81);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x91);
	LCD_WRITE_DATA(0x57);
	LCD_WRITE_COMMAND(0x92);
	LCD_WRITE_DATA(0x57);
	LCD_WRITE_COMMAND(0xA0);
	LCD_WRITE_DATA(0x52);
	LCD_WRITE_COMMAND(0xA1);
	LCD_WRITE_DATA(0x50);
	LCD_WRITE_COMMAND(0xA4);
	LCD_WRITE_DATA(0x9C);
	LCD_WRITE_COMMAND(0xA7);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0xA8);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0xA9);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0xAA);
	LCD_WRITE_DATA(0xA8);
	LCD_WRITE_COMMAND(0xAB);
	LCD_WRITE_DATA(0x28);
	LCD_WRITE_COMMAND(0xAE);
	LCD_WRITE_DATA(0xD2);
	LCD_WRITE_COMMAND(0xAF);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0xB0);
	LCD_WRITE_DATA(0xD2);
	LCD_WRITE_COMMAND(0xB2);
	LCD_WRITE_DATA(0x26);
	LCD_WRITE_COMMAND(0xB3);
	LCD_WRITE_DATA(0x26);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x30);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x52);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0xB1);
	LCD_WRITE_DATA(0x0E);
	LCD_WRITE_COMMAND(0xD1);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_COMMAND(0xB4);
	LCD_WRITE_DATA(0x28);
	LCD_WRITE_COMMAND(0xD4);
	LCD_WRITE_DATA(0x32);
	LCD_WRITE_COMMAND(0xB2);
	LCD_WRITE_DATA(0x08);
	LCD_WRITE_COMMAND(0xD2);
	LCD_WRITE_DATA(0x03);
	LCD_WRITE_COMMAND(0xB3);
	LCD_WRITE_DATA(0x29);
	LCD_WRITE_COMMAND(0xD3);
	LCD_WRITE_DATA(0x33);
	LCD_WRITE_COMMAND(0xB6);
	LCD_WRITE_DATA(0x12);
	LCD_WRITE_COMMAND(0xD6);
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_COMMAND(0xB7);
	LCD_WRITE_DATA(0x32);
	LCD_WRITE_COMMAND(0xD7);
	LCD_WRITE_DATA(0x39);
	LCD_WRITE_COMMAND(0xC1);
	LCD_WRITE_DATA(0x08);
	LCD_WRITE_COMMAND(0xE1);
	LCD_WRITE_DATA(0x04);
	LCD_WRITE_COMMAND(0xB8);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_COMMAND(0xD8);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_COMMAND(0xB9);
	LCD_WRITE_DATA(0x03);
	LCD_WRITE_COMMAND(0xD9);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0xBD);
	LCD_WRITE_DATA(0x13);
	LCD_WRITE_COMMAND(0xDD);
	LCD_WRITE_DATA(0x14);
	LCD_WRITE_COMMAND(0xBC);
	LCD_WRITE_DATA(0x10);
	LCD_WRITE_COMMAND(0xDC);
	LCD_WRITE_DATA(0x11);
	LCD_WRITE_COMMAND(0xBB);
	LCD_WRITE_DATA(0x0D);
	LCD_WRITE_COMMAND(0xDB);
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_COMMAND(0xBA);
	LCD_WRITE_DATA(0x0E);
	LCD_WRITE_COMMAND(0xDA);
	LCD_WRITE_DATA(0x10);
	LCD_WRITE_COMMAND(0xBE);
	LCD_WRITE_DATA(0x18);
	LCD_WRITE_COMMAND(0xDE);
	LCD_WRITE_DATA(0x1A);
	LCD_WRITE_COMMAND(0xBF);
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_COMMAND(0xDF);
	LCD_WRITE_DATA(0x11);
	LCD_WRITE_COMMAND(0xC0);
	LCD_WRITE_DATA(0x16);
	LCD_WRITE_COMMAND(0xE0);
	LCD_WRITE_DATA(0x18);
	LCD_WRITE_COMMAND(0xB5);
	LCD_WRITE_DATA(0x37);
	LCD_WRITE_COMMAND(0xD5);
	LCD_WRITE_DATA(0x32);
	LCD_WRITE_COMMAND(0xB0);
	LCD_WRITE_DATA(0x02);
	LCD_WRITE_COMMAND(0xD0);
	LCD_WRITE_DATA(0x05);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x30);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x52);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x03);
	LCD_WRITE_COMMAND(0x00);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x01);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x02);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x03);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x08);
	LCD_WRITE_DATA(0x0A);
	LCD_WRITE_COMMAND(0x09);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_COMMAND(0x0A);
	LCD_WRITE_DATA(0x0C);
	LCD_WRITE_COMMAND(0x0B);
	LCD_WRITE_DATA(0x0D);
	LCD_WRITE_COMMAND(0x20);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x21);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x22);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x23);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x28);
	LCD_WRITE_DATA(0x22);
	LCD_WRITE_COMMAND(0x2A);
	LCD_WRITE_DATA(0xe9);
	LCD_WRITE_COMMAND(0x2B);
	LCD_WRITE_DATA(0xe9);
	LCD_WRITE_COMMAND(0x30);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x31);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x32);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x33);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x34);
	LCD_WRITE_DATA(0xA1);
	LCD_WRITE_COMMAND(0x35);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x36);
	LCD_WRITE_DATA(0x26);  
	LCD_WRITE_COMMAND(0x37);
	LCD_WRITE_DATA(0x03);
	LCD_WRITE_COMMAND(0x40);
	LCD_WRITE_DATA(0x0D);  
	LCD_WRITE_COMMAND(0x41);
	LCD_WRITE_DATA(0x0E);  
	LCD_WRITE_COMMAND(0x42);
	LCD_WRITE_DATA(0x0F);  
	LCD_WRITE_COMMAND(0x43);
	LCD_WRITE_DATA(0x10);  
	LCD_WRITE_COMMAND(0x44);
	LCD_WRITE_DATA(0x22);
	LCD_WRITE_COMMAND(0x45);
	LCD_WRITE_DATA(0xe4);  
	LCD_WRITE_COMMAND(0x46);
	LCD_WRITE_DATA(0xe5);  
	LCD_WRITE_COMMAND(0x47);
	LCD_WRITE_DATA(0x22);
	LCD_WRITE_COMMAND(0x48);
	LCD_WRITE_DATA(0xe6);  
	LCD_WRITE_COMMAND(0x49);
	LCD_WRITE_DATA(0xe7);  
	LCD_WRITE_COMMAND(0x50);
	LCD_WRITE_DATA(0x11);  
	LCD_WRITE_COMMAND(0x51);
	LCD_WRITE_DATA(0x12);  
	LCD_WRITE_COMMAND(0x52);
	LCD_WRITE_DATA(0x13);  
	LCD_WRITE_COMMAND(0x53);
	LCD_WRITE_DATA(0x14);  
	LCD_WRITE_COMMAND(0x54);
	LCD_WRITE_DATA(0x22);
	LCD_WRITE_COMMAND(0x55);
	LCD_WRITE_DATA(0xe8);  
	LCD_WRITE_COMMAND(0x56);
	LCD_WRITE_DATA(0xe9);  
	LCD_WRITE_COMMAND(0x57);
	LCD_WRITE_DATA(0x22);
	LCD_WRITE_COMMAND(0x58);
	LCD_WRITE_DATA(0xea);  
	LCD_WRITE_COMMAND(0x59);
	LCD_WRITE_DATA(0xeb);  
	LCD_WRITE_COMMAND(0x60);
	LCD_WRITE_DATA(0x05);  
	LCD_WRITE_COMMAND(0x61);
	LCD_WRITE_DATA(0x05);  
	LCD_WRITE_COMMAND(0x65);
	LCD_WRITE_DATA(0x0A);  
	LCD_WRITE_COMMAND(0x66);
	LCD_WRITE_DATA(0x0A);
	  
	LCD_WRITE_COMMAND(0x80);
	LCD_WRITE_DATA(0x0f);    
	LCD_WRITE_COMMAND(0x81);
	LCD_WRITE_DATA(0x0d);    
	LCD_WRITE_COMMAND(0x82);
	LCD_WRITE_DATA(0x0b);    
	LCD_WRITE_COMMAND(0x83);
	LCD_WRITE_DATA(0x09);    
	LCD_WRITE_COMMAND(0x8A);
	LCD_WRITE_DATA(0x01);    
	LCD_WRITE_COMMAND(0x8B);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0x8D);
	LCD_WRITE_DATA(0x00);    
	LCD_WRITE_COMMAND(0x8E);
	LCD_WRITE_DATA(0x03);    
	LCD_WRITE_COMMAND(0x90);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0x91);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0x92);
	LCD_WRITE_DATA(0x11);    
	LCD_WRITE_COMMAND(0x94);
	LCD_WRITE_DATA(0x00);    
	LCD_WRITE_COMMAND(0x95);
	LCD_WRITE_DATA(0x00);
		
	LCD_WRITE_COMMAND(0x96);
	LCD_WRITE_DATA(0x10);    
	LCD_WRITE_COMMAND(0x97);
	LCD_WRITE_DATA(0x0e);    
	LCD_WRITE_COMMAND(0x98);
	LCD_WRITE_DATA(0x0c);    
	LCD_WRITE_COMMAND(0x99);
	LCD_WRITE_DATA(0x0a);    
	LCD_WRITE_COMMAND(0xA0);
	LCD_WRITE_DATA(0x02);    
	LCD_WRITE_COMMAND(0xA1);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0xA3);
	LCD_WRITE_DATA(0x00);    
	LCD_WRITE_COMMAND(0xA4);
	LCD_WRITE_DATA(0x04);    
	LCD_WRITE_COMMAND(0xA6);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0xA7);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0xA8);
	LCD_WRITE_DATA(0x12);    
	LCD_WRITE_COMMAND(0xAA);
	LCD_WRITE_DATA(0x00);    
	LCD_WRITE_COMMAND(0xAB);
	LCD_WRITE_DATA(0x00); 

	LCD_WRITE_COMMAND(0xB0);
	LCD_WRITE_DATA(0x0A); 
	LCD_WRITE_COMMAND(0xB1);
	LCD_WRITE_DATA(0x0C); 
	LCD_WRITE_COMMAND(0xB2);
	LCD_WRITE_DATA(0x0E); 
	LCD_WRITE_COMMAND(0xB3);
	LCD_WRITE_DATA(0x10); 
	LCD_WRITE_COMMAND(0xBa);
	LCD_WRITE_DATA(0x04); 
	LCD_WRITE_COMMAND(0xBb);
	LCD_WRITE_DATA(0x00); 
	LCD_WRITE_COMMAND(0xBd);
	LCD_WRITE_DATA(0x1F); 
	LCD_WRITE_COMMAND(0xBe);
	LCD_WRITE_DATA(0x02); 
	LCD_WRITE_COMMAND(0xc0);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0xc1);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0xc2);
	LCD_WRITE_DATA(0x11);    
	LCD_WRITE_COMMAND(0xc4);
	LCD_WRITE_DATA(0x00);    
	LCD_WRITE_COMMAND(0xc5);
	LCD_WRITE_DATA(0x00);
	 
	LCD_WRITE_COMMAND(0xC6);
	LCD_WRITE_DATA(0x09); 
	LCD_WRITE_COMMAND(0xC7);
	LCD_WRITE_DATA(0x0B); 
	LCD_WRITE_COMMAND(0xC8);
	LCD_WRITE_DATA(0x0D); 
	LCD_WRITE_COMMAND(0xC9);
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_COMMAND(0xD0);
	LCD_WRITE_DATA(0x03); 
	LCD_WRITE_COMMAND(0xD1);
	LCD_WRITE_DATA(0x00); 
	LCD_WRITE_COMMAND(0xD3);
	LCD_WRITE_DATA(0x1F);
	LCD_WRITE_COMMAND(0xD4);
	LCD_WRITE_DATA(0x01);
	LCD_WRITE_COMMAND(0xd6);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0xd7);
	LCD_WRITE_DATA(0x1f);    
	LCD_WRITE_COMMAND(0xd8);
	LCD_WRITE_DATA(0x12);    
	LCD_WRITE_COMMAND(0xdA);
	LCD_WRITE_DATA(0x00);    
	LCD_WRITE_COMMAND(0xdB);
	LCD_WRITE_DATA(0x00);   
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x30);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x52);
	LCD_WRITE_COMMAND(0xFF);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_COMMAND(0x3A);
	LCD_WRITE_DATA(0x66);
	LCD_WRITE_COMMAND(0x36);
	LCD_WRITE_DATA(0x0a);
	LCD_WRITE_COMMAND(0x11);
	LCD_WRITE_DATA(0x00);
	sunxi_lcd_delay_ms( 200 );                        
	LCD_WRITE_COMMAND(0x29);
	LCD_WRITE_DATA(0x00);
	sunxi_lcd_delay_ms(100);   
	return;
}

static void LCD_panel_exit(u32 sel)
{
	return;
}

/* sel: 0:lcd0; 1:lcd1 */
static s32 LCD_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel nv3052c_rgb_panel = {
	/* panel driver name, must mach the lcd_drv_name in sys_config.fex */
	.name = "nv3052c_rgb",
	.func = {
		 .cfg_panel_info = LCD_cfg_panel_info,
		 .cfg_open_flow = LCD_open_flow,
		 .cfg_close_flow = LCD_close_flow,
		 .lcd_user_defined_func = LCD_user_defined_func,
		 }
	,
};
