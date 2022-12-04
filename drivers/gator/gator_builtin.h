/**
 * Copyright (C) Arm Limited 2010-2016. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GATOR_BUILTIN_H_
#define GATOR_BUILTIN_H_

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
extern struct module *__gator_module_address(unsigned long addr);
#else
#define __gator_module_address __module_address
#endif

#endif
