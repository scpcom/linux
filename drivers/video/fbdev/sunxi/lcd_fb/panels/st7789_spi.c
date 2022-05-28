/*
 * drivers/video/fbdev/sunxi/lcd_fb/panels/st7789_spi.c
 *
 * Copyright (c) 2018-2022 Sipeed Co., Ltd.
 *
 */

#include "st7789_spi.h"
#include <linux/spi/spidev.h>
#include <linux/spi/spi.h>

static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);
static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);
#define RESET(s, v) sunxi_lcd_gpio_set_value(s, 0, v)
#define DC(s, v) sunxi_lcd_gpio_set_value(s, 1, v)
static struct spi_device *spi_device;
static struct disp_panel_para info[LCD_FB_MAX];

static int spi_init(void)
{
	int ret = -1;
	struct spi_master *master;

	master = spi_busnum_to_master(0);  //we use spi0
	if (!master) {
		lcd_fb_wrn("fail to get master\n");
		goto OUT;
	}

	spi_device = spi_alloc_device(master);
	if (!spi_device) {
		lcd_fb_wrn("fail to get spi device\n");
		goto OUT;
	}

	spi_device->bits_per_word = 8;
	spi_device->max_speed_hz = 60000000; /*50Mhz*/
	spi_device->mode = SPI_MODE_0;

	ret = spi_setup(spi_device);
	if (ret) {
		lcd_fb_wrn("Faile to setup spi\n");
		goto FREE;
	}

	lcd_fb_inf("Init spi1:bits_per_word:%d max_speed_hz:%d mode:%d\n",
		   spi_device->bits_per_word, spi_device->max_speed_hz,
		   spi_device->mode);

	ret = 0;
	goto OUT;

FREE:
	spi_master_put(master);
	kfree(spi_device);
	spi_device = NULL;
OUT:
	return ret;
}

// command LOW
static int comm_out(unsigned int sel, unsigned char cmd)
{

	struct spi_transfer	t;

	if (!spi_device)
		return -1;

	DC(sel, 0);
	memset(&t, 0, sizeof(struct spi_transfer));


	t.tx_buf 	= &cmd;
	t.len		= 1;
	t.bits_per_word	= 8;
	t.speed_hz = 24000000;

	return spi_sync_transfer(spi_device, &t, 1);
}

//Data High
static int reg_out(unsigned int sel, unsigned char reg)
{
	struct spi_transfer	t;

	if (!spi_device)
		return -1;

	memset(&t, 0, sizeof(struct spi_transfer));
	DC(sel, 1);

	t.tx_buf 	= &reg;
	t.len		= 1;
	t.bits_per_word	= 8;
	t.speed_hz = 24000000;

	return spi_sync_transfer(spi_device, &t, 1);
}

static void address(unsigned int sel, int x, int y, int width, int height)
{
	comm_out(sel, 0x2B); /* Set row address */
	reg_out(sel, (y >> 8) & 0xff);
	reg_out(sel, y & 0xff);
	reg_out(sel, (height >> 8) & 0xff);
	reg_out(sel, height & 0xff);
	comm_out(sel, 0x2A); /* Set coloum address */
	reg_out(sel, (x >> 8) & 0xff);
	reg_out(sel, x & 0xff);
	reg_out(sel, (width >> 8) & 0xff);
	reg_out(sel, width & 0xff);
}

static int black_screen(unsigned int sel)
{
	struct spi_transfer t;
	int i = 0, ret = -1;
	unsigned char *pixel = NULL;
	unsigned char Bpp = 3;

	if (!spi_device)
		return -1;

	ret = bsp_disp_get_panel_info(sel, &info[sel]);
	if (ret) {
		lcd_fb_wrn("get panel info fail!\n");
		goto OUT;
	}
	if (info[sel].lcd_pixel_fmt == LCDFB_FORMAT_RGB_565 ||
	    info[sel].lcd_pixel_fmt == LCDFB_FORMAT_BGR_565)
		Bpp = 2;

	comm_out(sel, 0x2c);
	DC(sel, 1);
	pixel = kmalloc(info[sel].lcd_x * Bpp, GFP_KERNEL | __GFP_ZERO);
	if (!pixel)
		goto OUT;

	memset(&t, 0, sizeof(struct spi_transfer));
	t.tx_buf = pixel;
	t.len = info[sel].lcd_x * Bpp;
	t.bits_per_word = 8;
	t.speed_hz = 60000000;

	for (i = 0; i < info[sel].lcd_y; ++i)
		ret = spi_sync_transfer(spi_device, &t, 1);

	kfree(pixel);
OUT:

	return ret;
}

static void init_panel(unsigned int sel)
{
	unsigned int rotate;


	if (bsp_disp_get_panel_info(sel, &info[sel])) {
		lcd_fb_wrn("get panel info fail!\n");
		return;
	}

	sunxi_lcd_delay_ms(120);  

	comm_out(sel , 0x11);     //Sleep out

	sunxi_lcd_delay_ms(120); 

	comm_out(sel , 0x36);     
	reg_out(sel , 0x00);  

	comm_out(sel , 0x3A);     
	reg_out(sel , 0x05);     //0x05( 65K Color) 

	comm_out(sel , 0x21);     

	comm_out(sel , 0xB2);     
	reg_out(sel , 0x05);   
	reg_out(sel , 0x05);   
	reg_out(sel , 0x00);   
	reg_out(sel , 0x33);   
	reg_out(sel , 0x33);   

	comm_out(sel , 0xB7);     
	reg_out(sel , 0x23);   

	comm_out(sel , 0xBB);     
	reg_out(sel , 0x22);   

	comm_out(sel , 0xC0);     
	reg_out(sel , 0x2C);   

	comm_out(sel , 0xC2);     
	reg_out(sel , 0x01);   

	comm_out(sel , 0xC3);     
	reg_out(sel , 0x13);   

	comm_out(sel , 0xC4);     
	reg_out(sel , 0x20);   

	comm_out(sel , 0xC6);     
	reg_out(sel , 0x0F);   

	comm_out(sel , 0xD0);     
	reg_out(sel , 0xA4);   
	reg_out(sel , 0xA1);   

	comm_out(sel , 0xD6);     
	reg_out(sel , 0xA1);   

	comm_out(sel , 0xE0);
	reg_out(sel , 0x70);
	reg_out(sel , 0x06);
	reg_out(sel , 0x0C);
	reg_out(sel , 0x08);
	reg_out(sel , 0x09);
	reg_out(sel , 0x27);
	reg_out(sel , 0x2E);
	reg_out(sel , 0x34);
	reg_out(sel , 0x46);
	reg_out(sel , 0x37);
	reg_out(sel , 0x13);
	reg_out(sel , 0x13);
	reg_out(sel , 0x25);
	reg_out(sel , 0x2A);

	comm_out(sel , 0xE1);
	reg_out(sel , 0x70);
	reg_out(sel , 0x04);
	reg_out(sel , 0x08);
	reg_out(sel , 0x09);
	reg_out(sel , 0x07);
	reg_out(sel , 0x03);
	reg_out(sel , 0x2C);
	reg_out(sel , 0x42);
	reg_out(sel , 0x42);
	reg_out(sel , 0x38);
	reg_out(sel , 0x14);
	reg_out(sel , 0x14);
	reg_out(sel , 0x27);
	reg_out(sel , 0x2C);

	comm_out(sel , 0x29);     //Display on
	/****************************************************/
	comm_out(sel , 0x2A);     //Column Address Set X
	reg_out(sel , 0x00);   //x high
	reg_out(sel , 0x34);   //52 //x low
	reg_out(sel , 0x00);   //
	reg_out(sel , 0xBA);   //186

	comm_out(sel , 0x2B);     //Row Address Set Y
	reg_out(sel , 0x00);   
	reg_out(sel , 0x28);   //40
	reg_out(sel , 0x01);   
	reg_out(sel , 0x17);   //279

	comm_out(sel , 0x2C);

	/*****************************************************/

	//comm_out(sel , 0x11);     //Sleep out
	//sunxi_lcd_delay_ms(120);                //sunxi_lcd_delay_ms 120ms

	/*****************************************************/

	//comm_out(sel , 0x10);     //Sleep in
	//sunxi_lcd_delay_ms(120);                //sunxi_lcd_delay_ms 120ms


	//address(sel, 0, 0, info[sel].lcd_x - 1, info[sel].lcd_y - 1);
	//sunxi_lcd_delay_ms(200); /* Delay 120ms */
	//comm_out(sel, 0x29); /* Display ON */

	black_screen(sel);
}

static void exit_panel(unsigned int sel)
{
	comm_out(sel, 0x28);
	sunxi_lcd_delay_ms(20);
	comm_out(sel, 0x10);
	sunxi_lcd_delay_ms(20);

	RESET(sel, 0);
	sunxi_lcd_delay_ms(10);

	sunxi_lcd_pin_cfg(sel, 0);

	if (spi_device) {
		if (spi_device->master->cleanup)
			spi_device->master->cleanup(spi_device);
		spi_master_put(spi_device->master);
		kfree(spi_device);
		spi_device = NULL;
	}
}

static int panel_blank(unsigned int sel, unsigned int en)
{
	if (en)
		comm_out(sel, 0x28);
	else
		comm_out(sel, 0x29);
	return 0;
}

static int panel_dma_transfer(unsigned int sel, void *buf, unsigned int len)
{
	struct spi_transfer t;
	int ret = 0;

	if (!spi_device)
		return -1;

	memset(&t, 0, sizeof(struct spi_transfer));

	t.tx_buf = (void *)buf;
	t.len = len;
	t.bits_per_word = 8;
	t.speed_hz = 60000000;

	ret = spi_sync_transfer(spi_device, &t, 1);


	return ret;
}

static int panel_set_var(unsigned int sel, struct fb_info *p_info)
{
	return 0;
}

static void LCD_cfg_panel_info(struct panel_extend_para *info)
{
	;
}

static s32 LCD_open_flow(u32 sel)
{
	lcd_fb_here;
	/* open lcd power, and delay 50ms */
	LCD_OPEN_FUNC(sel, LCD_power_on, 50);
	/* open lcd power, than delay 200ms */
	LCD_OPEN_FUNC(sel, LCD_panel_init, 20);
	/* open lcd backlight, and delay 0ms */
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	lcd_fb_here;
	/* close lcd backlight, and delay 0ms */
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 50);
	/* open lcd power, than delay 200ms */
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 10);
	/* close lcd power, and delay 500ms */
	LCD_CLOSE_FUNC(sel, LCD_power_off, 10);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	/* config lcd_power pin to open lcd power0 */
	int ret = 0;
	lcd_fb_here;

	sunxi_lcd_power_enable(sel, 0);

	ret = spi_init();
	if (ret) {
		lcd_fb_wrn("Init spi fail!\n");
		return;
	}
	sunxi_lcd_pin_cfg(sel, 1);
	RESET(sel, 1);
	sunxi_lcd_delay_ms(100);
	RESET(sel, 0);
	sunxi_lcd_delay_ms(100);
	RESET(sel, 1);
}

static void LCD_power_off(u32 sel)
{
	lcd_fb_here;
	/* config lcd_power pin to close lcd power0 */
	sunxi_lcd_power_disable(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	/* config lcd_bl_en pin to open lcd backlight */
	sunxi_lcd_backlight_enable(sel);
	lcd_fb_here;
}

static void LCD_bl_close(u32 sel)
{
	/* config lcd_bl_en pin to close lcd backlight */
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
	lcd_fb_here;
}


static void LCD_panel_init(u32 sel)
{
	lcd_fb_here;
	init_panel(sel);
	return;
}

static void LCD_panel_exit(u32 sel)
{
	lcd_fb_here;
	exit_panel(sel);
	return;
}

/* sel: 0:lcd0; 1:lcd1 */
static s32 LCD_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	lcd_fb_here;
	return 0;
}

static int lcd_set_layer(unsigned int sel, void *buf, unsigned int len)
{
	return panel_dma_transfer(sel, buf, len);
}

static int lcd_set_var(unsigned int sel, struct fb_info *p_info)
{
	return panel_set_var(sel, p_info);
}

static int lcd_blank(unsigned int sel, unsigned int en)
{
	return panel_blank(sel, en);
}

static int lcd_set_addr_win(unsigned int sel, int x, int y, int width, int height)
{
	address(sel, x, y, width, height);
	return 0;
}

struct __lcd_panel st7789_spi_panel = {
    /* panel driver name, must mach the name of lcd_drv_name in sys_config.fex
       */
	.name = "st7789_spi",
	.func = {
		.cfg_panel_info = LCD_cfg_panel_info,
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
		.lcd_user_defined_func = LCD_user_defined_func,
		//.set_layer = lcd_set_layer,
		.blank = lcd_blank,
		.set_var = lcd_set_var,
		.set_addr_win = lcd_set_addr_win,
		},
};
