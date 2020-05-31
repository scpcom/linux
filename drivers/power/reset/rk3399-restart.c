/*
 * Rockchip SoCs Reboot Driver
 *
 * Copyright (C) 2015 Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on the gpio-restart driver.
 */
#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <asm/system_misc.h>

struct rk3399_restart {
	struct gpio_desc *reset_gpio;
	u32 active_delay_ms;
	u32 inactive_delay_ms;
	u32 wait_delay_ms;
};
struct rk3399_restart *rk3399_restart;

static void rk3399_reboot(enum reboot_mode reboot_mode, const char *cmd)
{
	if (IS_ERR(rk3399_restart->reset_gpio)) {
		return;
	}
	/* drive it active, also inactive->active edge */
	gpiod_direction_output(rk3399_restart->reset_gpio, 1);
	mdelay(rk3399_restart->active_delay_ms);

	/* drive inactive, also active->inactive edge */
	gpiod_set_value(rk3399_restart->reset_gpio, 0);
	mdelay(rk3399_restart->inactive_delay_ms);

	/* drive it active, also inactive->active edge */
	gpiod_set_value(rk3399_restart->reset_gpio, 1);

	/* give it some time */
	mdelay(rk3399_restart->wait_delay_ms);

	WARN_ON(1);

	return;
}

static int rk3399_restart_probe(struct platform_device *pdev)
{
	bool open_source = false;

	rk3399_restart = devm_kzalloc(&pdev->dev, sizeof(*rk3399_restart),
			GFP_KERNEL);
	if (!rk3399_restart)
		return -ENOMEM;

	open_source = of_property_read_bool(pdev->dev.of_node, "open-source");

	rk3399_restart->reset_gpio = devm_gpiod_get(&pdev->dev, NULL,
			open_source ? GPIOD_IN : GPIOD_OUT_LOW);
	if (IS_ERR(rk3399_restart->reset_gpio)) {
		dev_err(&pdev->dev, "Could not get reset GPIO\n");
		return PTR_ERR(rk3399_restart->reset_gpio);
	}

	rk3399_restart->active_delay_ms = 100;
	rk3399_restart->inactive_delay_ms = 100;
	rk3399_restart->wait_delay_ms = 3000;

	of_property_read_u32(pdev->dev.of_node, "active-delay",
			&rk3399_restart->active_delay_ms);
	of_property_read_u32(pdev->dev.of_node, "inactive-delay",
			&rk3399_restart->inactive_delay_ms);
	of_property_read_u32(pdev->dev.of_node, "wait-delay",
			&rk3399_restart->wait_delay_ms);

	platform_set_drvdata(pdev, rk3399_restart);

	arm_pm_restart = rk3399_reboot;

	return 0;
}

static const struct of_device_id of_rk3399_restart_match[] = {
	{ .compatible = "rockchip,rk3399-restart", },
	{},
};

static struct platform_driver rk3399_restart_driver = {
	.probe = rk3399_restart_probe,
	.driver = {
		.name = "rk3399-restart-gpio",
		.of_match_table = of_rk3399_restart_match,
	},
};

static int __init rk3399_restart_init(void)
{
	return platform_driver_probe(&rk3399_restart_driver,
			rk3399_restart_probe);
}
subsys_initcall(rk3399_restart_init);
