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
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/reset.h>

#include "pfe_platform.h"

#include "pfe_mod.h"

static u64 comcerto_pfe_dma_mask = DMA_BIT_MASK(32);

#ifdef CONFIG_OF
static struct resource *pfe_get_linked_resource(struct device_node *np, const char *name, struct resource *rp)
{
	struct device_node *iram_node;
	int ret;

	iram_node = of_parse_phandle(np, name, 0);
	if (!iram_node)
		return NULL;

	ret = of_address_to_resource(np, 0, rp);

	of_node_put(iram_node);

	if (ret < 0)
		return NULL;

	return rp;
}
#endif

/**
 * pfe_platform_probe -
 *
 *
 */
static int pfe_platform_probe(struct platform_device *pdev)
{
	struct resource *r;
	int rc;
	struct clk *clk_axi;
#ifdef CONFIG_OF
	struct reset_control *rst_axi;
	struct resource res;
#endif

	printk(KERN_INFO "%s\n", __func__);

	pfe = kzalloc(sizeof(struct pfe), GFP_KERNEL);
	if (!pfe) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	platform_set_drvdata(pdev, pfe);

	pdev->dev.dma_mask		= &comcerto_pfe_dma_mask;
	pdev->dev.coherent_dma_mask	= DMA_BIT_MASK(32);
	pdev->dev.platform_data = (void *)of_device_get_match_data(&pdev->dev);

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ddr");
	if (!r) {
		printk(KERN_INFO "platform_get_resource_byname(ddr) failed\n");
		rc = -ENXIO;
		goto err_ddr;
	}
	
	pfe->ddr_phys_baseaddr = r->start;
	pfe->ddr_size = resource_size(r);

	pfe->ddr_baseaddr = ioremap(r->start, resource_size(r));
	pr_err("pfe->ddr_baseaddr = %p size %u\n", pfe->ddr_baseaddr, resource_size(r));
	if (!pfe->ddr_baseaddr) {
		printk(KERN_INFO "ioremap(%p %08x) ddr failed\n", (void*) r->start, resource_size(r));
		rc = -ENOMEM;
		goto err_ddr;
	}
	
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "axi");
	if (!r) {
		printk(KERN_INFO "platform_get_resource_byname(axi) failed\n");
		rc = -ENXIO;
		goto err_axi;
	}

	pfe->cbus_baseaddr = ioremap(r->start, resource_size(r));
	pr_err("cbus start %08x size %08x\n", r->start, resource_size(r));
	if (!pfe->cbus_baseaddr) {
		printk(KERN_INFO "ioremap() axi failed\n");
		rc = -ENOMEM;
		goto err_axi;
	}

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apb");
	if (!r) {
		printk(KERN_INFO "platform_get_resource_byname(apb) failed\n");
		rc = -ENXIO;
		goto err_apb;
	}

	pfe->apb_baseaddr = ioremap(r->start, resource_size(r));
	if (!pfe->apb_baseaddr) {
		printk(KERN_INFO "ioremap() apb failed\n");
		rc = -ENOMEM;
		goto err_apb;
	}

#ifdef CONFIG_OF
	r = pfe_get_linked_resource(pdev->dev.of_node, "fsl,ls1024a-pfe-iram", &res);
#else
	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "iram");
#endif
	if (!r) {
		printk(KERN_INFO "platform_get_resource_byname(iram) failed\n");
		rc = -ENXIO;
		goto err_iram;
	}

	pfe->iram_phys_baseaddr = r->start;
	pfe->iram_baseaddr = ioremap(r->start, resource_size(r));
	if (!pfe->iram_baseaddr) {
		printk(KERN_INFO "ioremap() iram failed\n");
		rc = -ENOMEM;
		goto err_iram;
	}

        r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ipsec");
        if (!r) {
                printk(KERN_INFO "platform_get_resource_byname(ipsec) failed\n");
                rc = -ENXIO;
                goto err_ipsec;
        }

        pfe->ipsec_phys_baseaddr = r->start;
	/* Just map only initial 1MB , as its enough to access espah engine
	*/
        //pfe->ipsec_baseaddr = ioremap(r->start, resource_size(r));
        pfe->ipsec_baseaddr = ioremap(r->start, 1*1024*1024);
        if (!pfe->ipsec_baseaddr) {
                printk(KERN_INFO "ioremap() ipsec failed\n");
                rc = -ENOMEM;
                goto err_ipsec;
        }

        printk(KERN_INFO "ipsec: baseaddr :%x --- %x\n",  (u32)pfe->ipsec_phys_baseaddr,  (u32)pfe->ipsec_baseaddr);

#ifndef CONFIG_OF
	pfe->hif_irq = platform_get_irq_byname(pdev, "hif");
	if (pfe->hif_irq < 0) {
		printk(KERN_INFO "platform_get_irq_byname(hif) failed\n");
		rc = pfe->hif_irq;
		goto err_hif_irq;
	}
#endif

	pfe->dev = &pdev->dev;

#ifdef CONFIG_OF
	rst_axi = devm_reset_control_get_exclusive(&pdev->dev, "axi");
	if (IS_ERR(rst_axi)) {
		dev_err(&pdev->dev, "Failed to get pfe reset control\n");
		return PTR_ERR(rst_axi);
	}

	pfe->ctrl.rst_axi = rst_axi;
	reset_control_assert(pfe->ctrl.rst_axi);
	mdelay(1);
	rc = reset_control_deassert(pfe->ctrl.rst_axi);
	if (rc) {
		dev_err(&pdev->dev, "Failed to put pfe out of reset\n");
		return rc;
	}

#else
	/* FIXME this needs to be done at the BSP level with proper locking */
	pfe_writel(pfe_readl(PFE_AXI_RESET) | PFE_SYS_AXI_RESET_BIT, PFE_AXI_RESET);
	mdelay(1);
	pfe_writel(pfe_readl(PFE_AXI_RESET) & ~PFE_SYS_AXI_RESET_BIT, PFE_AXI_RESET);
#endif

	/* Get the system clock */
	clk_axi = clk_get(&pdev->dev, "axi");
	if (IS_ERR(clk_axi)) {
		printk(KERN_INFO "clk_get call failed\n");
		rc = -ENXIO;
		goto err_clk;
	}

	pfe->ctrl.clk_axi = clk_axi;
	pfe->ctrl.sys_clk = clk_get_rate(clk_axi) / 1000;  // save sys_clk value as KHz

	rc = pfe_probe(pfe);
	if (rc < 0)
		goto err_probe;

	return 0;

err_probe:
	clk_put(clk_axi);
err_clk:
#ifndef CONFIG_OF
err_hif_irq:
#endif
        iounmap(pfe->ipsec_baseaddr);
err_ipsec:
	iounmap(pfe->iram_baseaddr);
err_iram:
	iounmap(pfe->apb_baseaddr);

err_apb:
	iounmap(pfe->cbus_baseaddr);

err_axi:
	iounmap(pfe->ddr_baseaddr);

err_ddr:
	platform_set_drvdata(pdev, NULL);

	kfree(pfe);

err_alloc:
	return rc;
}


/**
 * pfe_platform_remove -
 *
 *
 */
static int pfe_platform_remove(struct platform_device *pdev)
{
	struct pfe *pfe = platform_get_drvdata(pdev);
	int rc;
	
	printk(KERN_INFO "%s\n", __func__);

	rc = pfe_remove(pfe);

#ifdef CONFIG_OF
	reset_control_assert(pfe->ctrl.rst_axi);
#else
	/* FIXME this needs to be done at the BSP level with proper locking */
	pfe_writel(pfe_readl(PFE_AXI_RESET) | PFE_SYS_AXI_RESET_BIT, PFE_AXI_RESET);
#endif

	clk_put(pfe->ctrl.clk_axi);
	iounmap(pfe->ipsec_baseaddr);
	iounmap(pfe->iram_baseaddr);
	iounmap(pfe->apb_baseaddr);
	iounmap(pfe->cbus_baseaddr);
	iounmap(pfe->ddr_baseaddr);

	platform_set_drvdata(pdev, NULL);

	kfree(pfe);

	return rc;
}

#ifdef CONFIG_PM

#ifdef CONFIG_PM_SLEEP
static int pfe_platform_suspend(struct device *dev)
{
	struct pfe *pfe = platform_get_drvdata(to_platform_device(dev));
	struct net_device *netdev;
	int i;

	printk(KERN_INFO "%s\n", __func__);

	pfe->wake = 0;

	for (i = 0; i < (NUM_GEMAC_SUPPORT - 1); i++ ) {
		netdev = pfe->eth.eth_priv[i]->dev;

		netif_device_detach(netdev);

		if (netif_running(netdev))
			if(pfe_eth_suspend(netdev))
				pfe->wake =1;
	}

	/* Shutdown PFE only if we're not waking up the system */
	if (!pfe->wake) {
		pfe_ctrl_suspend(&pfe->ctrl);
		pfe_hif_exit(pfe);
		pfe_hif_lib_exit(pfe);

		class_disable();
		tmu_disable(0xf);
#if !defined(CONFIG_UTIL_DISABLED)
		util_disable();
#endif
		pfe_hw_exit(pfe);
#ifdef CONFIG_OF
		reset_control_assert(pfe->ctrl.rst_axi);
#else
		pfe_writel(pfe_readl(PFE_AXI_RESET) | PFE_SYS_AXI_RESET_BIT, PFE_AXI_RESET);
#endif
		clk_disable(pfe->hfe_clock);
	}

	return 0;
}

static int pfe_platform_resume(struct device *dev)
{
	struct pfe *pfe = platform_get_drvdata(to_platform_device(dev));
	struct net_device *netdev;
	int i;

	printk(KERN_INFO "%s\n", __func__);

	if (!pfe->wake) {
		/* Sequence follows VLSI recommendation (bug 71927) */
#ifdef CONFIG_OF
		reset_control_assert(pfe->ctrl.rst_axi);
		mdelay(1);
		reset_control_deassert(pfe->ctrl.rst_axi);
#else
		pfe_writel(pfe_readl(PFE_AXI_RESET) | PFE_SYS_AXI_RESET_BIT, PFE_AXI_RESET);
		mdelay(1);
		pfe_writel(pfe_readl(PFE_AXI_RESET) & ~PFE_SYS_AXI_RESET_BIT, PFE_AXI_RESET);
#endif
		clk_enable(pfe->hfe_clock);

		pfe_hw_init(pfe, 1);
		pfe_hif_lib_init(pfe);
		pfe_hif_init(pfe);
#if !defined(CONFIG_UTIL_DISABLED)
		util_enable();
#endif
		tmu_enable(0xf);
		class_enable();
		pfe_ctrl_resume(&pfe->ctrl);
	}

	for(i = 0; i < (NUM_GEMAC_SUPPORT - 1); i++) {
		netdev = pfe->eth.eth_priv[i]->dev;

		if (pfe->eth.eth_priv[i]->mii_bus)
			pfe_eth_mdio_reset(pfe->eth.eth_priv[i]->mii_bus);

		if (netif_running(netdev))
			pfe_eth_resume(netdev);

		netif_device_attach(netdev);
	}
	return 0;
}
#else
#define pfe_platform_suspend NULL
#define pfe_platform_resume NULL
#endif

static const struct dev_pm_ops pfe_platform_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pfe_platform_suspend, pfe_platform_resume)
};

#endif

static struct comcerto_pfe_platform_data comcerto_pfe_pdata = {
	.comcerto_eth_pdata[0] = {
		.name = GEM0_ITF_NAME,
		.device_flags = CONFIG_COMCERTO_GEMAC,
		.mii_config = CONFIG_COMCERTO_USE_RGMII,
		.gemac_mode = GEMAC_SW_CONF | GEMAC_SW_FULL_DUPLEX | GEMAC_SW_SPEED_1G,
		.phy_flags = GEMAC_PHY_RGMII_ADD_DELAY,
		.bus_id = 0,
		.phy_id = 4,
		.gem_id = 0,
		.mac_addr = (u8[])GEM0_MAC,
	},

	.comcerto_eth_pdata[1] = {
		.name = GEM1_ITF_NAME,
		.device_flags = CONFIG_COMCERTO_GEMAC,
		.mii_config = CONFIG_COMCERTO_USE_RGMII,
		.gemac_mode = GEMAC_SW_CONF | GEMAC_SW_FULL_DUPLEX | GEMAC_SW_SPEED_1G,
		.phy_flags = GEMAC_PHY_RGMII_ADD_DELAY,
		.bus_id = 0,
		.phy_id = 6,
		.gem_id = 1,
		.mac_addr = (u8[])GEM1_MAC,
	},
#if 0
	.comcerto_eth_pdata[2] = {
		.name = GEM2_ITF_NAME,
		.device_flags = CONFIG_COMCERTO_GEMAC,
		.mii_config = CONFIG_COMCERTO_USE_RGMII,
		.gemac_mode = GEMAC_SW_CONF | GEMAC_SW_FULL_DUPLEX | GEMAC_SW_SPEED_1G,
		.phy_flags = GEMAC_NO_PHY,
		.gem_id = 2,
		.mac_addr = (u8[])GEM2_MAC,
	},
#endif
	/**
	 * There is a single mdio bus coming out of C2K.  And that's the one
	 * connected to GEM0. All PHY's, switchs will be connected to the same
	 * bus using different addresses. Typically .bus_id is always 0, only
	 * .phy_id will change in the different comcerto_eth_pdata[] structures above.
	 */
	.comcerto_mdio_pdata[0] = {
		.enabled = 1,
		.phy_mask = 0xFFFFFFAF,
		.mdc_div = 96,
		.irq = {
			[4] = PHY_POLL,
			[6] = PHY_POLL,
		},
	},
};

static const struct of_device_id of_pfe_platform_match[] = {
	{ .compatible = "fsl,ls1024a-pfe-ctrl", .data = &comcerto_pfe_pdata },
	{},
};

static struct platform_driver pfe_platform_driver = {
	.probe = pfe_platform_probe,
	.remove = pfe_platform_remove,
	.driver = {
		.name = "pfe",
		.of_match_table = of_pfe_platform_match,
#ifdef CONFIG_PM
		.pm = &pfe_platform_pm_ops,
#endif
	},
};

#ifdef CONFIG_OF
module_platform_driver(pfe_platform_driver);

#else
static int __init pfe_module_init(void)
{
	printk(KERN_INFO "%s\n", __func__);

	return platform_driver_register(&pfe_platform_driver);
}


static void __exit pfe_module_exit(void)
{
	platform_driver_unregister(&pfe_platform_driver);

	printk(KERN_INFO "%s\n", __func__);
}

module_init(pfe_module_init);
module_exit(pfe_module_exit);
#endif
MODULE_LICENSE("GPL");
