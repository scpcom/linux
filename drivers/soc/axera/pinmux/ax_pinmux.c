#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "ax_pinmux.h"

static unsigned int ax620Q_EVB_pinmux[] = {
#include "AX620Q_EVB_pinmux.h"
};

static unsigned int ax620Q_Demo_pinmux[] = {
#include "AX620Q_DEMO_pinmux.h"
};

static unsigned int ax630C_EVB_pinmux[] = {
#include "AX630C_EVB_pinmux.h"
};

static unsigned int ax630C_Demo_pinmux[] = {
#include "AX630C_DEMO_pinmux.h"
};

static struct pinmux ax620E_pinmux_tbl[AX620E_BOARD_MAX] = {
	[AX620Q_LP4_EVB_V1_0] =
	    {ax620Q_EVB_pinmux,
	     sizeof(ax620Q_EVB_pinmux) / sizeof(unsigned int)},
	[AX620Q_LP4_DEMO_V1_0] =
	    {ax620Q_Demo_pinmux,
	     sizeof(ax620Q_Demo_pinmux) / sizeof(unsigned int)},
	[AX630C_EVB_V1_0] =
	    {ax630C_EVB_pinmux,
	     sizeof(ax630C_EVB_pinmux) / sizeof(unsigned int)},
	[AX630C_DEMO_V1_0] =
	    {ax630C_Demo_pinmux,
	     sizeof(ax630C_Demo_pinmux) / sizeof(unsigned int)},
};

static int ax_get_board_id(void)
{
	int board_id;
	misc_info_t *regs;

	regs = (misc_info_t *)ioremap(MISC_INFO_ADDR, sizeof(misc_info_t));
	board_id = regs->board_id;
	iounmap((void *)regs);

	return board_id;
}

static int ax_pinmux_index_conv(int index)
{
	int ret;

	switch (index) {
	case AX630C_DEMO_LP4_V1_0:
	case AX630C_DEMO_LP4_V1_1:
		/* fall through */
	case AX630C_DEMO_V1_1:
		ret = AX630C_DEMO_V1_0;
		break;
	case AX620Q_LP4_DEMO_V1_1:
		ret = AX620Q_LP4_DEMO_V1_0;
		break;
	default :
		ret = index;
		break;
	}
	return ret;
}

static int ax_pin_init(struct platform_device *pdev)
{
	int i, ret = 0;
	int index = ax_get_board_id();
	u32 offset, base_reg = 0;
	u8 is_dphytx;
	void __iomem *reg = NULL;
	void __iomem *dphy_rst_set = NULL;
	void __iomem *dphy_mipi_en = NULL;

	if (index < 0 || index > AX620E_BOARD_MAX - 1) {
		ret = -EFAULT;
		goto end;
	}
	index = ax_pinmux_index_conv(index);

	dphy_rst_set = ioremap(DPHYTX_SW_RST_SET, 0x4);
	if (!dphy_rst_set) {
		pr_err("%s:ioremap(DPHYTX_SW_RST_SET) failed\n", __func__);
		ret = - ENOMEM;
		goto end;
	}
	dphy_mipi_en = ioremap(DPHYTX_MIPI_EN, 0x4);
	if (!dphy_rst_set) {
		pr_err("%s:ioremap(DPHYTX_MIPI_EN) failed\n", __func__);
		ret = - ENOMEM;
		goto err_ioremap;
	}
	for (i = 0; i < ax620E_pinmux_tbl[index].size; i += 2) {
		is_dphytx =
		    ax620E_pinmux_tbl[index].data[i] - DPHYTX_BASE <
		    DPHY_REG_LEN ? 1 : 0;
		offset = ax620E_pinmux_tbl[index].data[i] - base_reg;
		if (!base_reg || offset >= REG_REMAP_SIZE) {
			base_reg = ax620E_pinmux_tbl[index].data[i];
			offset = ax620E_pinmux_tbl[index].data[i] - base_reg;
			if (reg)
				iounmap(reg);
			reg =
			    ioremap(ax620E_pinmux_tbl[index].data[i],
				    REG_REMAP_SIZE);
			if (!reg) {
				pr_err("%s:ioremap(pinmux) failed\n", __func__);
				ret = - ENOMEM;
				goto err_ioremap;
			}
		}
		//when dphytx select gpio func 1.set reset 2.mipi disable 3.func sel & config
		if (is_dphytx
		    && (ax620E_pinmux_tbl[index].data[i + 1] & PINMUX_FUNC_SEL)) {
			writel(DPHYTX_SW_RST_SHIFT, dphy_rst_set);
			writel(0, dphy_mipi_en);
		}
		writel(ax620E_pinmux_tbl[index].data[i + 1], reg + offset);
	}
err_ioremap:
	if (reg)
		iounmap(reg);
	if (dphy_rst_set)
		iounmap(dphy_rst_set);
	if (dphy_mipi_en)
		iounmap(dphy_mipi_en);
end:
	pr_debug("ax_pin_init ret %d index %d\n", ret, index);
	return ret;
}

static int ax_pinmux_probe(struct platform_device *pdev)
{
	int ret;

	ret = ax_pin_init(pdev);
	if (ret)
		pr_err("err: ax_pin_init failed!\n");
	return ret;
}

static int ax_pinmux_remove(struct platform_device *pdev)
{
	return 0;
}

static void ax_pinmux_release(struct device *dev)
{
	return;
}

static struct platform_device ax_pinmux_dev = {
	.name = "ax_pinmux",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.release=ax_pinmux_release,
	},
};

static struct platform_driver ax_pinmux_drv = {
	.probe = ax_pinmux_probe,
	.remove = ax_pinmux_remove,
	.driver = {
		.name = "ax_pinmux",
	},
};

static int __init ax_pinmux_init(void)
{
	platform_device_register(&ax_pinmux_dev);
	return platform_driver_register(&ax_pinmux_drv);
}

static void ax_pinmux_exit(void)
{
	platform_device_unregister(&ax_pinmux_dev);
	platform_driver_unregister(&ax_pinmux_drv);
}

arch_initcall(ax_pinmux_init);
module_exit(ax_pinmux_exit);
MODULE_DESCRIPTION("axera ax_pinmux driver");
MODULE_AUTHOR("Axera Inc.");
MODULE_LICENSE("GPL v2");
