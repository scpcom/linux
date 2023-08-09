/*
 * Zyxel NAS MCU Driver
 *
 * Copyright (C) 2023 scpcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on the gpio-restart driver and zyxel gpio driver.
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

struct nas_mcu {
	struct gpio_descs *gpios;
	s32 resb_index;
	s32 wdt_index;
	u32 active_delay_ms;
	u32 wait_delay_ms;
	struct proc_dir_entry *wdt_proc;
};
struct nas_mcu *nas_mcu;

struct gpio_desc *nas_mcu_wdt_get_gpiod(s32 index)
{
        if (IS_ERR(nas_mcu->gpios))
                return ERR_CAST(nas_mcu->gpios);

	if (index < 0)
		return ERR_PTR(-EINVAL);

	if (index < nas_mcu->gpios->ndescs)
		return nas_mcu->gpios->desc[index];
	else
		return ERR_PTR(-EINVAL);
}

static int nas_mcu_wdt_get(void)
{
	struct gpio_desc *wdt_gpio = nas_mcu_wdt_get_gpiod(nas_mcu->wdt_index);

	if (IS_ERR(wdt_gpio)) {
		return PTR_ERR(wdt_gpio);
	}

	return gpiod_get_value(wdt_gpio);
}

static int nas_mcu_wdt_set(unsigned int value)
{
	struct gpio_desc *wdt_gpio = nas_mcu_wdt_get_gpiod(nas_mcu->wdt_index);

	if (IS_ERR(wdt_gpio)) {
		return PTR_ERR(wdt_gpio);
	}

	/* drive it active, also inactive->active edge */
	mdelay(nas_mcu->active_delay_ms);

	/* drive it active, also inactive->active edge */
	gpiod_set_value(wdt_gpio, value);

	/* give it some time */
	mdelay(nas_mcu->wait_delay_ms);

	return 0;
}

ssize_t nas_mcu_wdt_read_fun(struct file *file, char __user *buff,
		size_t count, loff_t *pos)
{
	int len;
	char tmpbuf[64];

	int value = nas_mcu_wdt_get();

	if (value == 0)
		len = sprintf(tmpbuf, "0\n");
	else
		len = sprintf(tmpbuf, "1\n");

	if (*pos != 0)
		len = 0;
	if (!buff)
		return len;
	if (copy_to_user(buff, tmpbuf, len))
		len = 0;
	else
		*pos += len;

	return len;
}

static ssize_t nas_mcu_wdt_write_fun(struct file *file, const char __user *buff,
		size_t count, loff_t *pos)
{
	char tmpbuf[64];

	if (buff && !copy_from_user(tmpbuf, buff, count))
	{
		tmpbuf[count-1] = '\0';
		if ( tmpbuf[0] == '1' )
		{
			// reset mcu watchdog timer
			nas_mcu_wdt_set(1);
			printk(KERN_NOTICE " \033[033mMCU watchdog is reset!\033[0m\n");
		}
		else
		{
			// keep mcu watching timer going
			nas_mcu_wdt_set(0);
			printk(KERN_NOTICE "\033[033mMCU watchdog is not reset(system is booting up)!\033[0m\n");
		}

	}

	return count;
}

static const struct proc_ops nas_mcu_wdt_fops = {
	.proc_read = nas_mcu_wdt_read_fun,
	.proc_write = nas_mcu_wdt_write_fun,
};

static int nas_mcu_probe(struct platform_device *pdev)
{
	int i;

	nas_mcu = devm_kzalloc(&pdev->dev, sizeof(*nas_mcu),
			GFP_KERNEL);
	if (!nas_mcu)
		return -ENOMEM;

	nas_mcu->gpios = devm_gpiod_get_array(&pdev->dev, NULL, GPIOD_ASIS);
	if (IS_ERR(nas_mcu->gpios)) {
		dev_err(&pdev->dev, "error getting mcu GPIOs\n");
		return PTR_ERR(nas_mcu->gpios);
	}

	if (nas_mcu->gpios->ndescs < 1)
		return -EINVAL;

	for (i = 0; i < nas_mcu->gpios->ndescs; i++) {
		struct gpio_desc *gpiod = nas_mcu->gpios->desc[i];
		int ret, state;

		state = gpiod_get_value_cansleep(gpiod);
		if (state < 0)
			return state;

		ret = gpiod_direction_output(gpiod, state);
		if (ret < 0)
			return ret;
	}

	nas_mcu->wdt_index = nas_mcu->gpios->ndescs-1;
	nas_mcu->active_delay_ms = 100;
	nas_mcu->wait_delay_ms = 3000;

	of_property_read_u32(pdev->dev.of_node, "active-delay",
			&nas_mcu->active_delay_ms);
	of_property_read_u32(pdev->dev.of_node, "wait-delay",
			&nas_mcu->wait_delay_ms);

	platform_set_drvdata(pdev, nas_mcu);

	if (nas_mcu->gpios->ndescs > 4) {
		struct gpio_desc *resb_gpio;
		// set CPU_P_RES(GPIO 37) to MCU normal mode(1)
		nas_mcu->resb_index = 2;
		resb_gpio = nas_mcu_wdt_get_gpiod(nas_mcu->resb_index);
		gpiod_set_value(resb_gpio, 1);
	} else {
		nas_mcu->resb_index = -1;
	}

	nas_mcu->wdt_proc = proc_create_data("mcu_wdt", 0644, NULL, &nas_mcu_wdt_fops, NULL);

	return 0;
}

static const struct of_device_id of_nas_mcu_match[] = {
	{ .compatible = "zyxel,nas-mcu", },
	{},
};

static struct platform_driver nas_mcu_driver = {
	.probe = nas_mcu_probe,
	.driver = {
		.name = "nas-mcu",
		.of_match_table = of_nas_mcu_match,
	},
};

module_platform_driver(nas_mcu_driver);

MODULE_AUTHOR("scpcom <scpcom@gmx.de>");
MODULE_DESCRIPTION("Zyxel NAS MCU driver");
MODULE_LICENSE("GPL");
