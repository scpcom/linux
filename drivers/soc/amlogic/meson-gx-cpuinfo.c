/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 */

#include <linux/crc32.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <asm/system_info.h>

static int meson_gx_cpuinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvmem_cell *cell;
	unsigned char *efuse_buf, buf[17];
	size_t len;
	int i;

	cell = nvmem_cell_get(dev, "sn");
	if (IS_ERR(cell)) {
		dev_err(dev, "failed to get sn cell: %ld\n", PTR_ERR(cell));
		if (PTR_ERR(cell) == -EPROBE_DEFER)
			return PTR_ERR(cell);
		return PTR_ERR(cell);
	}
	efuse_buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (len != 16) {
		kfree(efuse_buf);
		dev_err(dev, "invalid sn len: %zu\n", len);
		return -EINVAL;
	}

	for (i = 0; i < 16; i++) {
		buf[i] = efuse_buf[i];
	}
	buf[16] = 0;

	kfree(efuse_buf);

	system_serial = kasprintf(GFP_KERNEL, "%s", buf);

	dev_info(dev, "Serial\t\t: %s\n", system_serial);

	return 0;
}

static const struct of_device_id meson_gx_cpuinfo_of_match[] = {
	{ .compatible = "amlogic,meson-gx-cpuinfo", },
	{ },
};
MODULE_DEVICE_TABLE(of, meson_gx_cpuinfo_of_match);

static struct platform_driver meson_gx_cpuinfo_driver = {
	.probe = meson_gx_cpuinfo_probe,
	.driver = {
		.name = "meson-gx-cpuinfo",
		.of_match_table = meson_gx_cpuinfo_of_match,
	},
};
module_platform_driver(meson_gx_cpuinfo_driver);

MODULE_DESCRIPTION("Amlogic Meson GX CPU info driver");
MODULE_AUTHOR("SCP <scpcom@gmx.de>");
MODULE_LICENSE("GPL");
