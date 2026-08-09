/*
 * Copyright (C) 2012-2017 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.h
 * Defines types and interface exposed by the Mali platform
 */

#ifndef __MALI_PLATFORM_H__
#define __MALI_PLATFORM_H__

#if defined(MALI_FAKE_PLATFORM_DEVICE)
#if defined(CONFIG_MALI_DT)
extern int mali_platform_device_init(struct platform_device *device);
extern int mali_platform_device_deinit(struct platform_device *device);
#else
extern int mali_platform_device_register(void);
extern int mali_platform_device_unregister(void);
#endif
#endif

extern int rk_platform_init_opp_table(struct device *dev);

#endif
