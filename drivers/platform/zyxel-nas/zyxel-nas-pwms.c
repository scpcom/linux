/*
 * Zyxel NAS PWMs Driver
 *
 * Copyright (C) 2023 scpcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on the zyxel gpio driver.
 */
#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include "zyxel-nas-ctrl.h"

/* pwm reg offsets */
#define REGMAP_PWM_CLOCK_DIVIDER_CONTROL	0x00
#define REGMAP_PWM_CURRENT_EVENT		0x04
#define REGMAP_PWM4_ENABLE_MAX			0x28
#define REGMAP_PWM4_LOW_DUTY_CYCLE		0x2C
#define REGMAP_PWM5_ENABLE_MAX			0x30
#define REGMAP_PWM5_LOW_DUTY_CYCLE		0x34

#define BUZ_PWM_CLOCK_DIVIDER_CONTROL	REGMAP_PWM_CLOCK_DIVIDER_CONTROL
#define BUZ_PWM_LOW_DUTY_CYCLE		REGMAP_PWM5_LOW_DUTY_CYCLE
#define BUZ_PWM_ENABLE_MAX		REGMAP_PWM5_ENABLE_MAX
#define BUZ_PWM_CDC_MASK		((0x1 << 0) | (0x1 << 31))
#define BUZ_PWM_LDC_MASK		(0xB000)
#define BUZ_PWM_ENM_MASK		(0x10000 | (0x1 << 31))

#define FAN_PWM_CLOCK_DIVIDER_CONTROL	REGMAP_PWM_CLOCK_DIVIDER_CONTROL
#define FAN_PWM_LOW_DUTY_CYCLE		REGMAP_PWM4_LOW_DUTY_CYCLE
#define FAN_PWM_ENABLE_MAX		REGMAP_PWM4_ENABLE_MAX
#define FAN_PWM_CDC_MASK		((0x1 << 0) | (0x1 << 31))
#define FAN_PWM_LDC_MASK		(1500)
#define FAN_PWM_ENM_MASK		(5000 | (0x1 << 31))

/** define the buzzer settings **/
#define BEEP_DURATION   1000

#define BZ_TIMER_PERIOD (HZ/2)
#define RING_FOREVER 1
#define RING_BRIEF 0

#define TIME_BITS       5
#define FREQ_BITS       4
#define STATE_BITS      2

#define TIME_MASK       (0x1F << (FREQ_BITS + STATE_BITS))
#define FREQ_MASK       (0xF << STATE_BITS)
#define STATE_MASK      0x3

#define GET_TIME(addr)  ((addr & TIME_MASK) >> (FREQ_BITS + STATE_BITS))
#define GET_FREQ(addr)  ((addr & FREQ_MASK) >> STATE_BITS)
#define GET_STATE(addr) (addr & STATE_MASK)

typedef enum {
	BUZ_OFF = 0,            /* turn off buzzer */
	BUZ_ON,
	BUZ_KILL,               /* kill buzzer daemon, equaling to BUZ_OFF*/
	BUZ_FOREVER             /* keep buzzing */
} buz_cmd_t;

struct nas_pwms_priv {
	struct regmap *pwmregs;

	struct nas_ctrl_timer_list bz_timer;
	short bz_time;
	short bz_timer_status;
	short bz_type;
};
struct nas_pwms_priv *nas_pwms = NULL;


static inline unsigned int pwm_readl(unsigned int reg)
{
	unsigned int val;
	regmap_read(nas_pwms->pwmregs, reg, &val);
	return val;
}

static inline void pwm_writel(unsigned int val, unsigned int reg)
{
	regmap_write(nas_pwms->pwmregs, reg, val);
}

static void nas_pwms_init_buzzer_pin(void)
{
	unsigned long reg;

	/* Select Pin for PWM (27:26 '01' - PWM[5]) */
	// done by pinctrl

	/* Enable the Clock Divider and set the value to 1 */
	reg = BUZ_PWM_CLOCK_DIVIDER_CONTROL;
	pwm_writel((pwm_readl(reg) | BUZ_PWM_CDC_MASK), reg);

	/* Enable PWM #5 timer and set the max value (0x10000) of PWM #5 */
	reg = BUZ_PWM_ENABLE_MAX;
	pwm_writel((pwm_readl(reg) | BUZ_PWM_ENM_MASK), reg);
}

static void nas_pwms_init_fan_pin(void)
{
	unsigned long reg;

	/* Select Pin for PWM (27:26 '01' - PWM[4]) */
	// done by pinctrl

	/* Enable the Clock Divider and set the value to 1 */
	reg = FAN_PWM_CLOCK_DIVIDER_CONTROL;
	pwm_writel((pwm_readl(reg) | FAN_PWM_CDC_MASK), reg);

	/* Enable PWM #4 timer and set the max value (5000) of PWM #4 */
	reg = FAN_PWM_ENABLE_MAX;
	pwm_writel((pwm_readl(reg) | FAN_PWM_ENM_MASK), reg);
}


static void buzzer_timer_func(unsigned long in_data)
{
	unsigned long reg = BUZ_PWM_LOW_DUTY_CYCLE;

	if (nas_pwms->bz_time != 0)    /* continue the timer */
	{
		int i;
		for(i = 0 ; i < BEEP_DURATION ; i++)
		{
			pwm_writel(pwm_readl(reg) ^ BUZ_PWM_LDC_MASK, reg);
			udelay(500);
		}

		pwm_writel(pwm_readl(reg) & ~BUZ_PWM_LDC_MASK, reg);
		nas_ctrl_mod_timer(&nas_pwms->bz_timer, jiffies + BZ_TIMER_PERIOD);
	}
	--nas_pwms->bz_time;
}

void Beep(void)
{
	int i;
	unsigned long reg = BUZ_PWM_LOW_DUTY_CYCLE;

	if (!nas_pwms)
		return;

	for(i = 0 ; i < BEEP_DURATION ; i++)
	{
		pwm_writel(pwm_readl(reg) ^ BUZ_PWM_LDC_MASK, reg);
		udelay(500);
	}

	pwm_writel(pwm_readl(reg) & ~BUZ_PWM_LDC_MASK, reg);
}

void Beep_Beep(int duty_high, int duty_low)
{
	// Duty cycle unit : ms
	int i;
	unsigned long reg = BUZ_PWM_LOW_DUTY_CYCLE;

	if (!nas_pwms)
		return;

	for(i = 0 ; i < BEEP_DURATION ; i++){
		pwm_writel(pwm_readl(reg) ^ BUZ_PWM_LDC_MASK, reg);
		udelay(duty_high);
	}

	for(i = 0 ; i < BEEP_DURATION ; i++){
		pwm_writel(pwm_readl(reg) & ~BUZ_PWM_LDC_MASK, reg);
		udelay(duty_low);
	}
}

void set_buzzer(unsigned long bz_data)
{
	unsigned short time, status;
	unsigned long reg = BUZ_PWM_LOW_DUTY_CYCLE;

	if (!nas_pwms)
		return;

	time = GET_TIME(bz_data);
	status = GET_STATE(bz_data);

	if (nas_pwms->bz_timer_status == TIMER_OFFLINE)
	{
		nas_ctrl_init_timer(&nas_pwms->bz_timer, buzzer_timer_func, 0);
		nas_pwms->bz_timer_status = TIMER_SLEEPING;
	}

	printk(KERN_ERR"bz time = %x\n", time);
	printk(KERN_ERR"bz status = %x\n", status);
	printk(KERN_ERR"bz_timer_status = %x\n", nas_pwms->bz_timer_status);

	// Turn off bz first
	if (nas_pwms->bz_timer_status == TIMER_RUNNING)
	{
		if (nas_pwms->bz_type == RING_FOREVER && status == BUZ_ON)
		{
			//printk(KERN_ERR"Buzzer Forever Already On \n");
			return;
		}
		nas_pwms->bz_timer_status = TIMER_SLEEPING;

		/* Disable buzzer first */
		pwm_writel(pwm_readl(reg) & ~BUZ_PWM_LDC_MASK, reg);
		nas_ctrl_del_timer_sync(&nas_pwms->bz_timer);
		nas_pwms->bz_type = RING_BRIEF;
		//printk(KERN_ERR"Closed Buzzer, bz_type = %d\n", bz_type);
	}

	if (status == BUZ_ON || status == BUZ_FOREVER)
	{
		// set bz time
		nas_pwms->bz_timer_status = TIMER_RUNNING;
		if (time >= 32 || status == BUZ_FOREVER) time = -1;
		if (time == 0) time = 1;
		nas_pwms->bz_time = time;
		printk(KERN_ERR"start buzzer\n");
		nas_pwms->bz_timer.function = buzzer_timer_func;
		nas_ctrl_mod_timer(&nas_pwms->bz_timer, jiffies + BZ_TIMER_PERIOD);
		if (status == BUZ_FOREVER){
			nas_pwms->bz_type = RING_FOREVER;
			//printk(KERN_ERR"Buzzer Forever, bz_type = %d\n", bz_type);
		}
	}
}


static void nas_pwms_set_init_timer(void)
{
	// init bz timer
	nas_pwms->bz_timer_status = TIMER_OFFLINE;
	nas_pwms->bz_type = RING_BRIEF;
}

static int nas_pwms_probe(struct platform_device *pdev)
{
	int ret;

	nas_pwms = devm_kzalloc(&pdev->dev, sizeof(*nas_pwms),
			GFP_KERNEL);
	if (!nas_pwms)
		return -ENOMEM;

	nas_pwms->pwmregs =
		syscon_regmap_lookup_by_compatible("fsl,ls1024a-pwm");
	if (IS_ERR(nas_pwms->pwmregs)) {
		dev_err(&pdev->dev, "failed to get GPIO registers: %ld\n",
		        PTR_ERR(nas_pwms->pwmregs));
		ret = PTR_ERR(nas_pwms->pwmregs);
		goto err;
	}

	platform_set_drvdata(pdev, nas_pwms);

	nas_pwms_init_buzzer_pin();
	nas_pwms_init_fan_pin();
	nas_pwms_set_init_timer();
	pwm_writel(FAN_PWM_LDC_MASK, FAN_PWM_LOW_DUTY_CYCLE);

	return 0;
err:
	nas_pwms = NULL;
	return ret;
}

static const struct of_device_id of_nas_pwms_match[] = {
	{ .compatible = "zyxel,nas-pwms", },
	{},
};

static struct platform_driver nas_pwms_driver = {
	.probe = nas_pwms_probe,
	.driver = {
		.name = "nas-pwms",
		.of_match_table = of_nas_pwms_match,
	},
};

module_platform_driver(nas_pwms_driver);

MODULE_AUTHOR("scpcom <scpcom@gmx.de>");
MODULE_DESCRIPTION("Zyxel NAS PWMs driver");
MODULE_LICENSE("GPL");
