// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
// Copyright (C) 2016-2018, Allwinner Technology CO., LTD.
// Copyright (C) 2019-2020, Cerno
#ifndef __SUNXI_IOMMU_H
#define __SUNXI_IOMMU_H

#if IS_ENABLED(CONFIG_SUNXI_IOMMU) || IS_ENABLED(CONFIG_SUN50I_IOMMU)
extern void sunxi_reset_device_iommu(unsigned int master_id);
extern void sunxi_enable_device_iommu(unsigned int mastor_id, bool flag);
#endif

#endif  /* __SUNXI_IOMMU_H */
