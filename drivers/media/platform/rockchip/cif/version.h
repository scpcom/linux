/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 Fuzhou Rockchip Electronics Co., Ltd. */

#ifndef _RKCIF_VERSION_H
#define _RKCIF_VERSION_H
#include <linux/version.h>

/*
 *RKCIF DRIVER VERSION NOTE
 *
 *v0.1.0:
 *1. First version;
 *v0.1.1
 *support the mipi vc multi-channel input in cif driver for rk1808
 *v0.1.2
 *Compatible with cif only have single dma mode in driver
 *v0.1.2
 *support output yuyv fmt by setting the input mode to raw8
 *
 */

#define RKCIF_DRIVER_VERSION KERNEL_VERSION(0, 1, 0x2)

#endif
