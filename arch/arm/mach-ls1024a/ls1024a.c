// SPDX-License-Identifier: GPL-2.0
/*
 * LS1024A DT board support code
 * Copyright (c) 2018 Hugo Grostabussiat <bonstra@bonstra.fr.eu.org>
 */

#include <linux/clocksource.h>
#include <linux/irqchip.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/system_info.h>

#define GPIO_DEVICE_ID		0x50
#define C2K_REVISION_SHIFT	24

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

static void __init ls1024a_init_machine(void)
{
	ls1024a_set_system_info();
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

