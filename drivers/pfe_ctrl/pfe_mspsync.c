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

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>

#else
#include "platform.h"
#endif


#include "pfe_mod.h"

int msp_register_pfe(struct pfe_info *pfe_sync_info);
void msp_unregister_pfe(void);

struct pfe_info pfe_sync_info;

int pfe_msp_sync_init(struct pfe *pfe)
{
	pfe_sync_info.owner = THIS_MODULE;
	pfe_sync_info.cbus_baseaddr = pfe->cbus_baseaddr;
	pfe_sync_info.ddr_baseaddr = pfe->ddr_baseaddr;

	if (msp_register_pfe(&pfe_sync_info)) {
		printk(KERN_ERR "%s: Failed to register with msp\n",__func__);
		return -EIO;
	}

	return 0;
}

void pfe_msp_sync_exit(struct pfe *pfe)
{
	msp_unregister_pfe();
}
