/* needed to detect kernel version specific code */
#include <linux/version.h>

/*
#include "ump_osk.h"
#include "ump_uk_types.h"
#include "ump_ukk.h"
#include "ump_kernel_common.h"
*/
#include <linux/module.h>            /* kernel module definitions */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/memory.h>
#include <asm/uaccess.h>                        /* to verify pointers from user space */
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
#include <linux/dma-map-ops.h>
#endif

void _ump_osk_flush_dcache_area(void *virt, u32 size)
{
#ifdef CONFIG_ARM64
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
		dcache_clean_inval_poc((unsigned long)virt, (unsigned long)virt + size);
#else
		__flush_dcache_area(virt, size);
#endif
#else
		__cpuc_flush_dcache_area(virt, size);
#endif
}
EXPORT_SYMBOL(_ump_osk_flush_dcache_area);

void _ump_osk_set_dma_ops(struct device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	set_dma_ops(dev, &dma_dummy_ops);
#endif
}
EXPORT_SYMBOL(_ump_osk_set_dma_ops);
