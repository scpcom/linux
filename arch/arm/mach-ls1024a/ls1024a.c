// SPDX-License-Identifier: GPL-2.0
/*
 * LS1024A DT board support code
 * Copyright (c) 2018 Hugo Grostabussiat <bonstra@bonstra.fr.eu.org>
 */

#include <linux/clocksource.h>
#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/system_info.h>

#include <mach/ls1024a-pmu.h>

#define GPIO_DEVICE_ID		0x50
#define C2K_REVISION_SHIFT	24

unsigned int host_utilpe_shared_pmu_bitmask;

static int ls1024a_set_system_info(void)
{
	struct regmap *gpioregs;
	unsigned int val;
	unsigned int rev;
	int res;

	gpioregs =
		syscon_regmap_lookup_by_compatible("fsl,ls1024a-gpio");
	if (IS_ERR(gpioregs)) {
		pr_warn("%s: failed to get GPIO registers: %ld\n",
		        __func__, PTR_ERR(gpioregs));
		return PTR_ERR(gpioregs);
	}

	/* Read SoC revision */
	res = regmap_read(gpioregs, GPIO_DEVICE_ID, &val);
	if (res)
		return res;
	rev = val >> C2K_REVISION_SHIFT;
	system_rev = rev & 0xf;

	return 0;
}

unsigned int ls1024a_pm_bitmask_show(void)
{
	return host_utilpe_shared_pmu_bitmask;
}

void ls1024a_pm_bitmask_store(unsigned int bitmask_value)
{
	struct device_node *node;
	void __iomem *base;
	unsigned int old_bitmask;

	node = of_find_compatible_node(NULL, NULL, HOST_UTILPE_COMPATIBLE);
	if (IS_ERR_OR_NULL(node)) {
		pr_err("Failed to find \"%s\" in dts.\n", HOST_UTILPE_COMPATIBLE);
		return;
	}

	base = of_iomap(node, 0);
	if (base == NULL) {
		pr_err("Unable to remap IO\n");
		return;
	}
	pr_info("Base addr of \"%s\" is %.8x\n", HOST_UTILPE_COMPATIBLE, (u32)base);

	old_bitmask = readl(base + 4);
	pr_info("Old pmu bitmask: %.8x\n", old_bitmask);

	/*
	 * Initialize the shared pmu bitmask
	 * This information can be configurable run time.
	 * Can be passed from bootloader also (Not Implimented Yet)
	 */
	//host_utilpe_shared_pmu_bitmask = 0xFFE7FFFF;
	host_utilpe_shared_pmu_bitmask = bitmask_value;

	pr_info("Shared pmu bitmask: %.8x\n", host_utilpe_shared_pmu_bitmask);
	/* Pass the bitmask info to UtilPE */
	writel(host_utilpe_shared_pmu_bitmask, base + 4);
}

static void __init ls1024a_init_machine(void)
{
	/* Default value for the bit mask */
	unsigned int default_host_utilpe_shared_bitmask = ~(USB2p0_IRQ|WOL_IRQ);

	ls1024a_set_system_info();

	/* Default bit mask is applied here , which will be passed to Util-Pe*/
	ls1024a_pm_bitmask_store(default_host_utilpe_shared_bitmask);
}

static void __init ls1024a_map_io(void)
{
	debug_ll_io_init();
}

static const char *const ls1024a_compat[] __initconst = {
	"fsl,ls1024a",
	NULL,
};

DT_MACHINE_START(LS1024A, "Freescale LS1024A")
	.map_io		= ls1024a_map_io,
	.init_machine	= ls1024a_init_machine,
	.dt_compat	= ls1024a_compat,
MACHINE_END

