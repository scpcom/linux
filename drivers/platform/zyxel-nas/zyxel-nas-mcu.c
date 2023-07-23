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
	struct gpio_desc *reset_gpio;
	u32 active_delay_ms;
	u32 wait_delay_ms;
	struct proc_dir_entry *wdt_proc;
};
struct nas_mcu *nas_mcu;

static int nas_mcu_wdt_get(void)
{
	if (IS_ERR(nas_mcu->reset_gpio)) {
		return PTR_ERR(nas_mcu->reset_gpio);
	}

	return gpiod_get_value(nas_mcu->reset_gpio);
}

static int nas_mcu_wdt_set(unsigned int value)
{
	if (IS_ERR(nas_mcu->reset_gpio)) {
		return PTR_ERR(nas_mcu->reset_gpio);
	}

	/* drive it active, also inactive->active edge */
	gpiod_direction_output(nas_mcu->reset_gpio, 1);
	mdelay(nas_mcu->active_delay_ms);

	/* drive it active, also inactive->active edge */
	gpiod_set_value(nas_mcu->reset_gpio, value);

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
	nas_mcu = devm_kzalloc(&pdev->dev, sizeof(*nas_mcu),
			GFP_KERNEL);
	if (!nas_mcu)
		return -ENOMEM;

	nas_mcu->reset_gpio = devm_gpiod_get(&pdev->dev, NULL, GPIOD_ASIS);
	if (IS_ERR(nas_mcu->reset_gpio)) {
		dev_err(&pdev->dev, "Could not get reset GPIO\n");
		return PTR_ERR(nas_mcu->reset_gpio);
	}

	nas_mcu->active_delay_ms = 100;
	nas_mcu->wait_delay_ms = 3000;

	of_property_read_u32(pdev->dev.of_node, "active-delay",
			&nas_mcu->active_delay_ms);
	of_property_read_u32(pdev->dev.of_node, "wait-delay",
			&nas_mcu->wait_delay_ms);

	platform_set_drvdata(pdev, nas_mcu);

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
