/*
 * Copyright (C) 2016 Hardkernel Co. Ltd.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/version.h>

#include <ump/ump_kernel_interface_ref_drv.h>
#include <ump/ump_kernel_interface.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,5,0)
#include <drm/drmP.h>
#endif
#include <drm/drm_fb_helper.h>

static int _drm_get_ump_secure_id(struct fb_info *info, 
	struct drm_fb_helper *g_fbi, unsigned long arg, int buf)
{
	u32 __user *psecureid = (u32 __user *) arg;
	ump_secure_id secure_id;

	if (!g_fbi->ump_wrapped_buffer[info->node][buf]) {
		ump_dd_physical_block ump_memory_description;
		printk("ump: create disp: %d\n", buf);

		ump_memory_description.addr = info->fix.smem_start;
		ump_memory_description.size = info->fix.smem_len;
		g_fbi->ump_wrapped_buffer[info->node][buf] =
			ump_dd_handle_create_from_phys_blocks(
				&ump_memory_description, 1);
	}
	secure_id = ump_dd_secure_id_get(
			g_fbi->ump_wrapped_buffer[info->node][buf]);
			
	return put_user((unsigned int)secure_id, psecureid);
}

static int __init drm_ump_module_init(void)
{
	int ret = 0;
	drm_get_ump_secure_id = _drm_get_ump_secure_id;
	return ret;
}

static void __exit drm_ump_module_exit(void)
{
	drm_get_ump_secure_id = NULL;
}

module_init(drm_ump_module_init);
module_exit(drm_ump_module_exit);

MODULE_AUTHOR("Mauro Ribeiro <mauro.ribeiro@hardkernel.com>");
MODULE_DESCRIPTION("UMP Glue for DRM Framebuffer");
MODULE_LICENSE("GPL");
