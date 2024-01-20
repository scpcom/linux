#ifndef __ARCH_ARM_LS1024A_PMU_H__
#define __ARCH_ARM_LS1024A_PMU_H__

#define HOST_UTILPE_COMPATIBLE "fsl,ls1024a-host-utilpe"

#define LS1024A_PMU_PCIe0_IRQ               (1 << 15)

#define LS1024A_PMU_PCIe1_IRQ               (1 << 16)
#define LS1024A_PMU_SATA_IRQ                (1 << 17)
#define LS1024A_PMU_SATA_MSI_IRQ            (1 << 18)
#define LS1024A_PMU_USB2p0_IRQ              (1 << 19)

#define LS1024A_PMU_USB3p0_IRQ              (1 << 20)
#define LS1024A_PMU_HFE_0_IRQ               (1 << 21)
#define LS1024A_PMU_WOL_IRQ                 (1 << 22)

extern unsigned host_utilpe_shared_pmu_bitmask;

/* Check for the Bit_Mask bit for IRQ, if it is enabled
 * then we are not going suspend the device, as by
 * this device, we will wake from System Resume.
 */
#define ls1024a_pm_bitmask_handled(irqmask) \
		(!(host_utilpe_shared_pmu_bitmask & irqmask))

#endif
