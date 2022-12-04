/**
 * Copyright (C) Arm Limited 2010-2016. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include "gator_builtin.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
struct module *__gator_module_address(unsigned long addr)
{
	return __module_address(addr);
}
EXPORT_SYMBOL_GPL(__gator_module_address);
#endif
