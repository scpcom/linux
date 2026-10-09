// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/staging/android/ion/compat_ion.c
 *
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "ion.h"
#include "compat_ion.h"

/* See drivers/staging/android/uapi/ion.h for the definition of these structs */

struct compat_ion_custom_data {
	compat_uint_t cmd;
	compat_ulong_t arg;
};

#define COMPAT_ION_IOC_CUSTOM	_IOWR(ION_IOC_MAGIC, 6, \
				      struct compat_ion_custom_data)

long compat_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (!filp->f_op->unlocked_ioctl)
		return -ENOIOCTLCMD;

	switch (cmd) {
	case COMPAT_ION_IOC_CUSTOM: {
		struct compat_ion_custom_data data;

		if (copy_from_user(&data, compat_ptr(arg), sizeof(data)))
			return -EFAULT;
		return ion_custom_ioctl(data.cmd, (unsigned long)compat_ptr(data.arg));
	}
	default:
		return filp->f_op->unlocked_ioctl(filp, cmd,
						(unsigned long)compat_ptr(arg));
	}
}
