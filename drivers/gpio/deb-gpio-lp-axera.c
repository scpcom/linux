// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Axera Inc.
 */
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/interrupt.h>

#define BIT_COMMON_GPIO_LP_EIC_EN_SET			BIT(5)
#define COMMON_SYS_EIC_EN_SET_ADDR_OFFSET		0x214
#define BIT_COMMON_SYS_GPIO_LP_INT_CLR_SET(x)		BIT(x)
#define COMMON_SYS_DEB_GPIO_LP_INT_CLR0_SET_ADDR_OFFSET	0x2F4
#define COMMON_SYS_DEB_GPIO_LP_INT_CLR1_SET_ADDR_OFFSET	0x328
#define COMMON_SYS_DEB_GPIO_LP_INT_CLR2_SET_ADDR_OFFSET	0x35c
#define BIT_COMMON_SYS_GPIO_LP_INT_CLR_CLR(x)		BIT(x)
#define COMMON_SYS_DEB_GPIO_LP_INT_CLR0_CLR_ADDR_OFFSET	0x2F8
#define COMMON_SYS_DEB_GPIO_LP_INT_CLR1_CLR_ADDR_OFFSET	0x32c
#define COMMON_SYS_DEB_GPIO_LP_INT_CLR2_CLR_ADDR_OFFSET	0x360
#define BIT_COMMON_SYS_GPIO_LP_RISE_EN_SET(x)		BIT(x)
#define COMMON_SYS_DEB_GPIO_LP_RISE_EN0_SET_ADDR_OFFSET	0x300
#define COMMON_SYS_DEB_GPIO_LP_RISE_EN1_SET_ADDR_OFFSET	0x334
#define COMMON_SYS_DEB_GPIO_LP_RISE_EN2_SET_ADDR_OFFSET	0x368
#define BIT_COMMON_SYS_GPIO_LP_FALL_EN_SET(x)		BIT(x)
#define COMMON_SYS_DEB_GPIO_LP_FALL_EN0_SET_ADDR_OFFSET	0x30C
#define COMMON_SYS_DEB_GPIO_LP_FALL_EN1_SET_ADDR_OFFSET	0x340
#define COMMON_SYS_DEB_GPIO_LP_FALL_EN2_SET_ADDR_OFFSET	0x374
#define BIT_COMMON_SYS_GPIO_LP_INT_EN_SET(x)		BIT(x)
#define COMMON_SYS_DEB_GPIO_LP_INT_EN0_SET_ADDR_OFFSET	0x2E8
#define COMMON_SYS_DEB_GPIO_LP_INT_EN1_SET_ADDR_OFFSET	0x31c
#define COMMON_SYS_DEB_GPIO_LP_INT_EN2_SET_ADDR_OFFSET	0x350

#define IRQ_TYPE_LEVEL_HIGH	4
#define IRQ_TYPE_LEVEL_LOW	8

struct deb_gpio_lp_res {
	struct resource *pin_res;
	struct resource *sys_res;
	int irq;
	int type;
	void __iomem *pin_cfg;
	void __iomem *sys_cfg;
	int lp_num;
	int lp_reg_offset;
	int lp_func_num;
	int lp_func_offset;
	int lp_pull_up;
	int lp_pull_down;
	int lp_pull_en;
};
#if 0
static void config_deb_gpio_pad_function(int type, struct deb_gpio_lp_res *deb_lp_res)
{
	unsigned int val = 0;
	void __iomem *pin_cfg;
	pin_cfg = deb_lp_res->pin_cfg;
	val = readl(pin_cfg + deb_lp_res->lp_reg_offset);
	val |= (deb_lp_res->lp_func_num << deb_lp_res->lp_func_offset);

	/* except pinmux8/9/10 */
	if (deb_lp_res->pin_res->start < 0x104F0000) {
		if (type) {
			val |= (1 << deb_lp_res->lp_pull_up);
		} else {
			val &= ~(1 << deb_lp_res->lp_pull_up);
		}
		val |= (1 << deb_lp_res->lp_pull_en);
	} else {
		if (type) {
			val |= (1 << deb_lp_res->lp_pull_up);
		} else {
			val |= (1 << deb_lp_res->lp_pull_down);
		}
	}
	writel(val, pin_cfg + deb_lp_res->lp_reg_offset);

	if (pin_cfg) {
		iounmap(pin_cfg);
	}

}
#endif
static irqreturn_t deb_gpio_lp_handler(int irq, void *dev_id)
{
	struct deb_gpio_lp_res *deb_lp_res;
	int lp_num;
	void __iomem *sys_cfg;

	deb_lp_res = (struct deb_gpio_lp_res *)dev_id;
	lp_num = deb_lp_res->lp_num;
	sys_cfg = deb_lp_res->sys_cfg;

	if (0 <= lp_num && lp_num <=31) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_SET(lp_num), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR0_SET_ADDR_OFFSET);
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_CLR(lp_num), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR0_CLR_ADDR_OFFSET);
	} else if (32 <= lp_num && lp_num <= 63) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_SET(lp_num - 32), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR1_SET_ADDR_OFFSET);
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_CLR(lp_num - 32), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR1_CLR_ADDR_OFFSET);
	} else if (64 <= lp_num && lp_num <= 89) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_SET(lp_num - 64), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR2_SET_ADDR_OFFSET);
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_CLR(lp_num - 64), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR2_CLR_ADDR_OFFSET);
	}

	return IRQ_HANDLED;
}

static int wakeup_config(struct platform_device *pdev)
{
	struct deb_gpio_lp_res *deb_lp_res;
	void __iomem *pin_cfg;
	void __iomem *sys_cfg;
	int lp_num;

	deb_lp_res = platform_get_drvdata(pdev);

	if (!deb_lp_res) {
		return -ENODATA;
	}

	pin_cfg = ioremap(deb_lp_res->pin_res->start,
			resource_size(deb_lp_res->pin_res));
	deb_lp_res->pin_cfg = pin_cfg;

	if (!pin_cfg) {
		return -ENODATA;
	}

	sys_cfg = ioremap(deb_lp_res->sys_res->start,
			resource_size(deb_lp_res->sys_res));
	deb_lp_res->sys_cfg = sys_cfg;

	if (!sys_cfg) {
		return -ENODATA;
	}

	lp_num = deb_lp_res->lp_num;

	/* eic enable gpio_lp for wakeup */
	// writel(BIT_COMMON_GPIO_LP_EIC_EN_SET, sys_cfg +
	// 	COMMON_SYS_EIC_EN_SET_ADDR_OFFSET);

	/* whatever we clear interrupt status */
	if (0 <= lp_num && lp_num <=31) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_SET(lp_num), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR0_SET_ADDR_OFFSET);
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_CLR(lp_num), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR0_CLR_ADDR_OFFSET);
	} else if (32 <= lp_num && lp_num <= 63) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_SET(lp_num - 32), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR1_SET_ADDR_OFFSET);
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_CLR(lp_num - 32), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR1_CLR_ADDR_OFFSET);
	} else if (64 <= lp_num && lp_num <= 89) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_SET(lp_num - 64), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR2_SET_ADDR_OFFSET);
		writel(BIT_COMMON_SYS_GPIO_LP_INT_CLR_CLR(lp_num - 64), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_CLR2_CLR_ADDR_OFFSET);
	}

	/* set risen or falling */
	if (deb_lp_res->type == IRQ_TYPE_LEVEL_HIGH) {
//		config_deb_gpio_pad_function(0, deb_lp_res);
		if (0 <= lp_num && lp_num <= 31) {
			writel(BIT_COMMON_SYS_GPIO_LP_RISE_EN_SET(lp_num), sys_cfg +
				COMMON_SYS_DEB_GPIO_LP_RISE_EN0_SET_ADDR_OFFSET);
		} else if (32 <= lp_num && lp_num <= 63) {
			writel(BIT_COMMON_SYS_GPIO_LP_RISE_EN_SET(lp_num - 32), sys_cfg +
				COMMON_SYS_DEB_GPIO_LP_RISE_EN1_SET_ADDR_OFFSET);
		} else if (64 <= lp_num && lp_num <= 89) {
			writel(BIT_COMMON_SYS_GPIO_LP_RISE_EN_SET(lp_num - 64), sys_cfg +
				COMMON_SYS_DEB_GPIO_LP_RISE_EN2_SET_ADDR_OFFSET);
		}
	} else if (deb_lp_res->type == IRQ_TYPE_LEVEL_LOW) {
//		config_deb_gpio_pad_function(1, deb_lp_res);
		if (0 <= lp_num && lp_num <= 31) {
			writel(BIT_COMMON_SYS_GPIO_LP_FALL_EN_SET(lp_num), sys_cfg +
				COMMON_SYS_DEB_GPIO_LP_FALL_EN0_SET_ADDR_OFFSET);
		} else if (32 <= lp_num && lp_num <= 63) {
			writel(BIT_COMMON_SYS_GPIO_LP_FALL_EN_SET(lp_num - 32), sys_cfg +
				COMMON_SYS_DEB_GPIO_LP_FALL_EN1_SET_ADDR_OFFSET);
		} else if (64 <= lp_num && lp_num <= 89) {
			writel(BIT_COMMON_SYS_GPIO_LP_FALL_EN_SET(lp_num - 64), sys_cfg +
				COMMON_SYS_DEB_GPIO_LP_FALL_EN2_SET_ADDR_OFFSET);
		}
	}

	if (request_irq(deb_lp_res->irq, deb_gpio_lp_handler, 0, "deb_gpio_lp", deb_lp_res)) {
		return -ENODATA;
	}
	if (0 <= lp_num && lp_num <= 31) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_EN_SET(lp_num), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_EN0_SET_ADDR_OFFSET);
	} else if (32 <= lp_num && lp_num <= 63) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_EN_SET(lp_num - 32), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_EN1_SET_ADDR_OFFSET);
	} else if (64 <= lp_num && lp_num <= 89) {
		writel(BIT_COMMON_SYS_GPIO_LP_INT_EN_SET(lp_num - 64), sys_cfg +
			COMMON_SYS_DEB_GPIO_LP_INT_EN2_SET_ADDR_OFFSET);
	}

	return 0;
}



static int axera_deb_gpio_lp_probe(struct platform_device *pdev)
{
	struct deb_gpio_lp_res *deb_lp_res;
	struct device_node *np = pdev->dev.of_node;

	deb_lp_res = devm_kzalloc(&pdev->dev, sizeof(*deb_lp_res), GFP_KERNEL);
	if (deb_lp_res == NULL)
		return -ENOMEM;

	deb_lp_res->pin_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!deb_lp_res->pin_res)
		return -ENODATA;

	deb_lp_res->sys_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!deb_lp_res->sys_res)
		return -ENODATA;

	deb_lp_res->irq = platform_get_irq_byname(pdev, "wakeup_int");
	if (deb_lp_res->irq < 0) {
		return -ENODATA;
	}
#if 0
	if (of_property_read_u32(np, "lp-reg-offset",
		&deb_lp_res->lp_reg_offset) < 0) {
		return -ENODEV;
	}

	if (of_property_read_u32(np, "lp-func-num",
		&deb_lp_res->lp_func_num) < 0) {
		return -ENODEV;
	}

	if (of_property_read_u32(np, "lp-func-offset",
		&deb_lp_res->lp_func_offset) < 0) {
		return -ENODEV;
	}

	if (of_property_read_u32(np, "lp-pull-up",
		&deb_lp_res->lp_pull_up) < 0) {
		return -ENODEV;
	}

	if (of_property_read_u32(np, "lp-pull-down",
		&deb_lp_res->lp_pull_down) < 0) {
		return -ENODEV;
	}

	if (of_property_read_u32(np, "lp-pull-en",
		&deb_lp_res->lp_pull_en) < 0) {
		return -ENODEV;
	}
#endif
	if (of_property_read_u32(np, "lp-num",
		&deb_lp_res->lp_num) < 0) {
		return -ENODEV;
	}

	if (of_property_read_u32_index(np, "interrupts", 2,
		&deb_lp_res->type) < 0) {
		return -ENODEV;
	}

	platform_set_drvdata(pdev, deb_lp_res);

	if (wakeup_config(pdev) < 0) {
		return -ENODATA;
	}

	return 0;
}

static int axera_deb_gpio_lp_remove(struct platform_device *pdev)
{
	struct deb_gpio_lp_res *deb_lp_res;
	deb_lp_res = platform_get_drvdata(pdev);

	if (!deb_lp_res) {
		return -ENODATA;
	}

	free_irq(deb_lp_res->irq, NULL);

	if (deb_lp_res->sys_cfg)
		iounmap(deb_lp_res->sys_cfg);
	return 0;
}

static const struct of_device_id axera_deb_gpio_lp_matches[] = {
	{.compatible = "axera, deb-gpio-lp"},
	{},
};

static struct platform_driver axera_deb_gpio_lp_driver = {
	.driver = {
		   .name = "axera, deb-gpio-lp",
		   .of_match_table = axera_deb_gpio_lp_matches,
		   },
	.probe = axera_deb_gpio_lp_probe,
	.remove = axera_deb_gpio_lp_remove,
};

module_platform_driver(axera_deb_gpio_lp_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Axera");
MODULE_ALIAS("platform:axera-deb-gpio-lp");
