/*
 *
 *  Copyright (C) 2007 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "pfe_mod.h"

static u64 dma_mask = 0xffffffff;

/**
 * pfe_emulation_probe -
 *
 *
 */
static int pfe_emulation_probe(struct platform_device *pdev)
{
	int rc;

	printk(KERN_INFO "%s\n", __func__);

	pfe = kzalloc(sizeof(struct pfe), GFP_KERNEL);
	if (!pfe) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	platform_set_drvdata(pdev, pfe);

	pfe->ddr_phys_baseaddr = 0x00020000;	/* should match the value used on the BSP */
	pfe->ddr_size = 12 * SZ_1M;		/* should match the value used on the BSP */
	pfe->ddr_baseaddr = vmalloc(pfe->ddr_size);
	if (!pfe->ddr_baseaddr) {
		printk(KERN_INFO "vmalloc() ddr failed\n");
		rc = -ENOMEM;
		goto err_ddr;
	}

	pfe->cbus_baseaddr = vmalloc(16 * SZ_1M);
	if (!pfe->cbus_baseaddr) {
		printk(KERN_INFO "vmalloc() cbus failed\n");
		rc = -ENOMEM;
		goto err_cbus;
	}

	pfe->apb_baseaddr =  vmalloc(16 * SZ_1M);
	if (!pfe->cbus_baseaddr) {
		printk(KERN_INFO "vmalloc() apb failed\n");
		rc = -ENOMEM;
		goto err_apb;
	}

	pfe->iram_baseaddr = vmalloc(128 * SZ_1K);
	if (!pfe->iram_baseaddr) {
		printk(KERN_INFO "vmalloc() iram failed\n");
		rc = -ENOMEM;
		goto err_iram;
	}

	pfe->hif_irq = 0;
	pfe->dev = &pdev->dev;
	pfe->dev->dma_mask = &dma_mask;	

	pfe->ctrl.sys_clk = 250000;
	rc = pfe_probe(pfe);
	if (rc < 0)
		goto err_probe;

	return 0;

err_probe:
	vfree(pfe->iram_baseaddr);

err_iram:
	vfree(pfe->apb_baseaddr);

err_apb:
	vfree(pfe->cbus_baseaddr);

err_cbus:
	vfree(pfe->ddr_baseaddr);

err_ddr:
	platform_set_drvdata(pdev, NULL);

	kfree(pfe);

err_alloc:
	return rc;
}


/**
 * pfe_emulation_remove -
 *
 *
 */
static int pfe_emulation_remove(struct platform_device *pdev)
{
	struct pfe *pfe = platform_get_drvdata(pdev);
	int rc;

	printk(KERN_INFO "%s\n", __func__);

	rc = pfe_remove(pfe);

	vfree(pfe->iram_baseaddr);
	vfree(pfe->apb_baseaddr);
	vfree(pfe->cbus_baseaddr);
	vfree(pfe->ddr_baseaddr);
	platform_set_drvdata(pdev, NULL);

	kfree(pfe);

	return rc;
}


static struct platform_driver pfe_platform_driver = {
	.probe = pfe_emulation_probe,
	.remove = pfe_emulation_remove,
	.driver = {
		.name = "pfe",
	},
};


static void pfe_device_release(struct device *dev)
{

}


static struct platform_device pfe_platform_device = {
	.name		= "pfe",
	.id		= 0,
	.dev.release	= pfe_device_release,
};

static int __init pfe_module_init(void)
{
	printk(KERN_INFO "%s\n", __func__);

	platform_device_register(&pfe_platform_device);

	return platform_driver_register(&pfe_platform_driver);
}


static void __exit pfe_module_exit(void)
{
	platform_driver_unregister(&pfe_platform_driver);

	platform_device_unregister(&pfe_platform_device);

	printk(KERN_INFO "%s\n", __func__);
}

MODULE_LICENSE("GPL");
module_init(pfe_module_init);
module_exit(pfe_module_exit);
