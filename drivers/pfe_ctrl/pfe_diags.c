#include<linux/ioctl.h>

/*
 * pfe_diag.c
 *
 * Copyright (C) 2004,2005 Freescale Semiconductor, Inc.
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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/stat.h> /* TODO check if needed */
#include <linux/cdev.h> /* TODO check if needed */
#include <linux/ioctl.h>
#include <linux/semaphore.h>
#include <linux/cpumask.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/memory.h>

#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/timer.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
#include <mach/hardware.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,19,0)
#include <mach/debug.h>
#include <mach/memory.h>
#endif
#include <linux/sched.h>
#endif

#include "pfe_ctrl_hal.h"
#include "pfe_mod.h"
#include "pfe_diags.h"

#ifdef CFG_DIAGS

struct fppdiag_config CLASS_DMEM_SH2(fppdiagconfig) __attribute__((aligned(8)));
struct fppdiag_config UTIL_DMEM_SH2(fppdiagconfig) __attribute__((aligned(8)));

static int pe_id[NUM_PE_DIAGS] = { CLASS0_ID, CLASS1_ID, CLASS2_ID, CLASS3_ID, CLASS4_ID, CLASS5_ID, UTIL_ID };

static char pe_names[7][4] = {
	"PE0",
	"PE1",
	"PE2",
	"PE3",
	"PE4",
	"PE5",
	"UPE"};



/** Enables the FPP diagnostics for a single PE by allocating and filling the ring buffers with the given ring size.
 */
static int pfediag_enable(int pe_index, int ring_size)
{
	int i, j;
	struct fppdiag_drv_dat *fppdiag_driver_data = &pfe->diags.fppdiag_drv_data[pe_index];
	struct fppdiag_config fppconfig;

	if(ring_size <= 0)
		return -1;

	/* Allocate memory to store a ring_size of pages */
	fppdiag_driver_data->pfe_rng_baseaddr = pfe_kzalloc((sizeof(u32) * ring_size), GFP_KERNEL);
	if(!fppdiag_driver_data->pfe_rng_baseaddr)
		return -1; /* Failure */

	fppdiag_driver_data->virt_rng_baseaddr = pfe_kzalloc((sizeof(void *) * ring_size), GFP_KERNEL);
	if(!fppdiag_driver_data->virt_rng_baseaddr)
		goto err_virt;


	/* Allocate pages */
	for( i = 0; i < ring_size; i++)
	{
		void *ring_entry;
		dma_addr_t phys_ring_entry;
		ring_entry = (void *) get_zeroed_page(GFP_KERNEL);

		if(!ring_entry)
			break;
		fppdiag_driver_data->virt_rng_baseaddr[i] = ring_entry;
		/* Invalidate the cache for later use by FPP */
		phys_ring_entry = dma_map_single(pfe->dev, ring_entry, PAGE_SIZE, DMA_FROM_DEVICE);
		fppdiag_driver_data->pfe_rng_baseaddr[i] = cpu_to_be32((u32) phys_ring_entry);
	}

	/* In Failure case freeing the other pages */
	if (i != ring_size)
		goto err_ring;

	fppdiag_driver_data->fpp_config.rng_baseaddr = (u32 *) dma_map_single(pfe->dev, fppdiag_driver_data->pfe_rng_baseaddr, (sizeof(void **) * ring_size), DMA_TO_DEVICE);

	writel(0, &fppdiag_driver_data->virt_fppdiagctl->read_index);
	writel(0, &fppdiag_driver_data->virt_fppdiagctl->write_index);

	fppdiag_driver_data->fpp_config.rng_size = ring_size;
	fppdiag_driver_data->fpp_config.diag_ctl_flag |= FPPDIAG_CTL_ENABLE;

	fppconfig.rng_baseaddr = (u32 *) cpu_to_be32((u32) fppdiag_driver_data->fpp_config.rng_baseaddr);
	fppconfig.rng_size = cpu_to_be32(fppdiag_driver_data->fpp_config.rng_size);
	fppconfig.diag_ctl_flag = cpu_to_be16(fppdiag_driver_data->fpp_config.diag_ctl_flag);
	fppconfig.diag_log_flag = cpu_to_be16(fppdiag_driver_data->fpp_config.diag_log_flag);
	fppconfig.diag_mod_flag = cpu_to_be32(fppdiag_driver_data->fpp_config.diag_mod_flag);
	fppconfig.fppdiagctl = (fppdiag_ctl_t *) cpu_to_be32((u32) fppdiag_driver_data->fpp_config.fppdiagctl);

	// Copy fpp_config to PE DMEM
	pe_sync_stop(&pfe->ctrl, pe_id[pe_index]);
	if (pe_id[pe_index] != UTIL_ID) {
		pe_dmem_memcpy_to32(pe_id[pe_index], virt_to_class_dmem(&class_fppdiagconfig), &fppconfig, sizeof(struct fppdiag_config));
	}
	else {
		pe_dmem_memcpy_to32(pe_id[pe_index], virt_to_util_dmem(&util_fppdiagconfig), &fppconfig, sizeof(struct fppdiag_config));
	}
	pe_start(&pfe->ctrl, pe_id[pe_index]);

	printk(KERN_INFO "PFE diags enabled for %sdiagctl = 0x%08x, rng_base_addr = 0x%08x\n",
			pe_names[pe_index],
			(u32) fppdiag_driver_data->fpp_config.fppdiagctl,
			(u32) fppdiag_driver_data->fpp_config.rng_baseaddr);


	return 0;

err_ring:
	/* Memory allocation failed above */
	for (j = 0; j < i; j++) {
		dma_unmap_single(pfe->dev, be32_to_cpu(fppdiag_driver_data->pfe_rng_baseaddr[j]), PAGE_SIZE, DMA_FROM_DEVICE);
		free_page((u32) fppdiag_driver_data->virt_rng_baseaddr[j]);
	}

	pfe_kfree(fppdiag_driver_data->virt_rng_baseaddr);
	fppdiag_driver_data->virt_rng_baseaddr = NULL;
err_virt:
	pfe_kfree(fppdiag_driver_data->pfe_rng_baseaddr);
	fppdiag_driver_data->pfe_rng_baseaddr = NULL;
	fppdiag_driver_data->fpp_config.rng_baseaddr = NULL;


	return -1; /* Failure */
}


/** Enables FPP diagnostics for all PEs.
 *
 */
static void pfediag_enable_all(int ring_size)
{
	int i;

	for (i=0; i< NUM_PE_DIAGS; i++) {
		pfediag_enable(i, ring_size);
	}
}


/* This function disables the FPP diagnostics for a single PE and frees the memory allocated. */
static int pfediag_disable(int pe_index)
{
	struct fppdiag_drv_dat *fppdiag_driver_data = &pfe->diags.fppdiag_drv_data[pe_index];
	int i;
	struct fppdiag_config fppconfig;

	/* if fppdiag has already been disabled, we wouldn't want to disable it again, 
	   to mark this we will check the validity of the rng array  */
	if(!fppdiag_driver_data->fpp_config.rng_baseaddr)
		return 0;

#if 0
	if(comcerto_fpp_send_command(CMD_FPPDIAG_DISABLE, 0, NULL, 
                                    &rlen, (unsigned short *)rmsg))
		return -1;
#endif

	fppconfig.diag_ctl_flag &= cpu_to_be32(~FPPDIAG_CTL_ENABLE);
	fppconfig.rng_baseaddr = NULL;
	fppconfig.rng_size = cpu_to_be32(DEFAULT_RING_SIZE);

	// Copy fpp_config to PE DMEM
	pe_sync_stop(&pfe->ctrl, pe_id[pe_index]);
	if (pe_id[pe_index] != UTIL_ID) {
		pe_dmem_memcpy_to32(pe_id[pe_index], virt_to_class_dmem(&class_fppdiagconfig), &fppconfig, sizeof(struct fppdiag_config));
	}
	else {
		pe_dmem_memcpy_to32(pe_id[pe_index], virt_to_class_dmem(&util_fppdiagconfig), &fppconfig, sizeof(struct fppdiag_config));
	}
	pe_start(&pfe->ctrl, pe_id[pe_index]);

	writel(0, &fppdiag_driver_data->virt_fppdiagctl->read_index);
	writel(0, &fppdiag_driver_data->virt_fppdiagctl->write_index);

    for( i = 0; i < fppdiag_driver_data->fpp_config.rng_size; i++)
	{	
		if ( fppdiag_driver_data->pfe_rng_baseaddr[i])
		{
			/* Invalidate the cache */
			dma_unmap_single(pfe->dev, be32_to_cpu(fppdiag_driver_data->pfe_rng_baseaddr[i]), PAGE_SIZE, DMA_FROM_DEVICE);
			free_page((u32) fppdiag_driver_data->virt_rng_baseaddr[i]);
		}
	}
    dma_unmap_single(pfe->dev, (dma_addr_t) fppdiag_driver_data->fpp_config.rng_baseaddr,
    					(sizeof(void **) * fppdiag_driver_data->fpp_config.rng_size), DMA_TO_DEVICE);
	pfe_kfree(fppdiag_driver_data->pfe_rng_baseaddr);
	fppdiag_driver_data->pfe_rng_baseaddr = NULL;
	fppdiag_driver_data->fpp_config.rng_baseaddr = NULL;

	pfe_kfree(fppdiag_driver_data->virt_rng_baseaddr);
	fppdiag_driver_data->virt_rng_baseaddr = NULL;
	
	fppdiag_driver_data->fpp_config.rng_size = DEFAULT_RING_SIZE;
	fppdiag_driver_data->fpp_config.diag_ctl_flag &= ~FPPDIAG_CTL_ENABLE;

	printk(KERN_INFO "PFE diagnostics for %s disabled.\n", pe_names[pe_index]);
	return 0;
}

/** Disables FPP diagnostics for all PEs.
 *
 */
static void pfediag_disable_all(void)
{
	int i;

	for (i=0; i< NUM_PE_DIAGS; i++)
		pfediag_disable(i);
}

#if 0 // TODO update code to work with PFE.
/* This function checks for available entries written into the common buffers */
static int fppdiag_get_avail_entries(int* fppdiag_read_index)
{
	int read_index, write_index;
	fppdiag_ctl_t *fppdiagctl= (fppdiag_ctl_t *)(ARAM_DIAG_CTL_ADDR);

	write_index   = readl( &fppdiagctl->write_index);
	read_index    = readl( &fppdiagctl->read_index);
	*fppdiag_read_index = read_index;
	if(write_index < read_index )
	{
		return (fppdiag_drv_data->fpp_config.rng_size * FPPDIAG_ENTRIES_PER_PAGE)
			 - (read_index - write_index);
	}
	else
		return (write_index - read_index);
}

static int fppdiag_update_fpp(struct fppdiag_config * fpp_config)
{
	short rlen = 1;
	unsigned char rmsg[2] = {};
	struct fppdiag_config msg = {} ;
	msg.diag_mod_flag = fpp_config->diag_mod_flag;
	msg.diag_log_flag = fpp_config->diag_log_flag;

	if(comcerto_fpp_send_command(CMD_FPPDIAG_UPDATE, sizeof(msg), 
                                                    (unsigned short *)&msg,
                                                    &rlen, rmsg))
		return -1;
	
	return 0;
		
}

static int fppdiag_update_diagmodule(pFPPDIAGCMD pFppCmd)
{
	if (pFppCmd->flags_enable)
		fppdiag_drv_data->fpp_config.diag_mod_flag |= pFppCmd->flags.module;
	else
		fppdiag_drv_data->fpp_config.diag_mod_flag &= ~(pFppCmd->flags.module);
	fppdiag_update_fpp(&fppdiag_drv_data->fpp_config);
	return 0;
}

static int fppdiag_update_diaglog(pFPPDIAGCMD pFppCmd)
{
        if (pFppCmd->flags_enable)
                fppdiag_drv_data->fpp_config.diag_log_flag |= pFppCmd->flags.log;
	else
		fppdiag_drv_data->fpp_config.diag_log_flag &= ~(pFppCmd->flags.log);
	fppdiag_update_fpp(&fppdiag_drv_data->fpp_config);
	return 0;
}

/* This function gets the diag information to present it to the user */
static int fppdiag_get_diaginfo(pFPPDIAGINFO p_fppdiag_info)
{
	p_fppdiag_info->state  = fppdiag_drv_data->fpp_config.diag_ctl_flag;
	p_fppdiag_info->module = fppdiag_drv_data->fpp_config.diag_mod_flag;
	p_fppdiag_info->log    = fppdiag_drv_data->fpp_config.diag_log_flag; 
	
	return 0;
}



/* This function updates the read index once the information is updated from
 * the user */
static int fppdiag_update_read_index(pFPPDIAGDATA pFppData)
{
	fppdiag_ctl_t *fppdiagctl= (fppdiag_ctl_t *)(ARAM_DIAG_CTL_ADDR);
	int read_index	 = readl( &fppdiagctl->read_index);
	int diag_total_entries = fppdiag_drv_data->fpp_config.rng_size * FPPDIAG_ENTRIES_PER_PAGE;
	unsigned long diag_data_addr;
	unsigned int diag_page_addr;
	int i = 0;
	
	if (!fppdiag_drv_data->fpp_config.rng_baseaddr)
	{
		printk(KERN_INFO "ring base address is NULL\n");
		return 0;
	}

        // TODO: This should be possible in a more efficient manner.
	/* Invalidate the cache here for the entries read 
	 *  		(old read_index to new read_index */

	for (i = 0; i < pFppData->entries_to_read; i++)
	{
		diag_page_addr = (void *)phys_to_virt(be32_to_cpu(
			fppdiag_drv_data->fpp_config.rng_baseaddr[read_index/FPPDIAG_ENTRIES_PER_PAGE]));
		diag_data_addr = diag_page_addr + 
			((read_index & FPPDIAG_ENTRY_MASK) << FPPDIAG_ENTRY_SHIFT );
		dma_sync_single_for_cpu(pfe->dev, diag_data_addr, FPPDIAG_ENTRY_SIZE, DMA_FROM_DEVICE);

		read_index++;
		if (read_index == diag_total_entries)
			read_index = 0;
	}
	

	writel(pFppData->read_index, &fppdiagctl->read_index);

	return 0;
}

static unsigned int fppdiag_poll(struct file *filp, poll_table *wait)
{
	int read_index = 0;
	int avail_entries = fppdiag_get_avail_entries(&read_index);

	avail_entries = fppdiag_get_avail_entries(&read_index);
	if(avail_entries == 0)
	{
		schedule();
	}
	return POLLIN; 
}


static int fppdiag_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int fppdiag_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long fppdiag_ioctl(struct file *filp,
			unsigned int cmd, unsigned long param)
{
	int rc = 0;
	mutex_lock(&fppdiag_drv_data->lock);
	switch (cmd)
	{
		case FPPDIAG_DRV_GET_RINGSIZE:
		{
			FPPCONFIG fppconfig;
			pFPPCONFIG pFppConfig = (FPPCONFIG*) param;
			fppconfig.ring_size = fppdiag_drv_data->fpp_config.rng_size;;
			if (copy_to_user(pFppConfig, &fppconfig ,sizeof(FPPCONFIG))) {
	     			printk(KERN_INFO " FPPDIAG_CONFIG Error");
				rc = -EINVAL;
				break;
			}
		}
		break;
		case FPPDIAG_DRV_GETDATA:
		{
			int read_index = 0;
			int avail_entries = 0;
			FPPDIAGDATA fppdiag_start_data;
			pFPPDIAGDATA pFppDiagData = (FPPDIAGDATA*) param;
			avail_entries = fppdiag_get_avail_entries(&read_index);
			if (avail_entries > FPPDIAG_MAX_AVBL_ENTRIES)
				avail_entries = FPPDIAG_MAX_AVBL_ENTRIES;

			fppdiag_start_data.read_index = read_index;
			fppdiag_start_data.entries_to_read = avail_entries;

			if (copy_to_user(pFppDiagData, &fppdiag_start_data,
							sizeof(FPPDIAGDATA))) {
                                printk(KERN_INFO " FPPDIAG_GETDATA:Error");
				rc = -EINVAL;
				break;
                        }
		}
		break;
		case FPPDIAG_DRV_FINDATA:
		{
			FPPDIAGDATA fppdiag_fin_data;
                        pFPPDIAGDATA pFppDiagData = (FPPDIAGDATA*) param;
			
			if (copy_from_user(&fppdiag_fin_data,pFppDiagData,sizeof(FPPDIAGDATA))) {
                                printk(KERN_INFO " FPPDIAG_FINDATA:Error");
				rc = -EINVAL;
				break;
                        }
			fppdiag_update_read_index(&fppdiag_fin_data);
				
		}
		break;
		case FPPDIAG_DRV_SET_STATE:
		{
			
			FPPDIAGCMD fppdiag_state_cmd;
                        pFPPDIAGCMD pFppDiagCmd = (FPPDIAGCMD*) param;
			
			if (copy_from_user(&fppdiag_state_cmd,pFppDiagCmd,
					sizeof(FPPDIAGCMD))) {
                                printk(KERN_INFO " FPPDIAG_STATE:Error");
				rc = -EINVAL;
				break;
                        }
			pfediag_disable();
			if(fppdiag_state_cmd.flags.state_size & 0xFF)
			{
				/* state_size holds MS 2 bytes of the ring buffer size and the LS 1 byte
 				   holds enable/disable flag. */
				if(pfediag_enable(&fppdiag_drv_data[0] /* FIXME */, fppdiag_state_cmd.flags.state_size >> 8 ) < 0)
				{
					printk(KERN_INFO " FPPDIAG_DRV_SET_STATE Error");
					rc = -EINVAL;
					break;
				}
			}
			//fppdiag_update_diagstate(&fppdiag_state_cmd);
		}
		break;
		case FPPDIAG_DRV_SET_MODULE:
                {

                        FPPDIAGCMD fppdiag_state_cmd;
                        pFPPDIAGCMD pFppDiagCmd = (FPPDIAGCMD*) param;

                        if (copy_from_user(&fppdiag_state_cmd,pFppDiagCmd,
                                        sizeof(FPPDIAGCMD))) {
                                printk(KERN_INFO " FPPDIAG_MODULE:Error");
				rc = -EINVAL;
				break;
                        }
                        fppdiag_update_diagmodule(&fppdiag_state_cmd);
                }
                break;
		case FPPDIAG_DRV_SET_LOG:
                {
                        FPPDIAGCMD fppdiag_state_cmd;
                        pFPPDIAGCMD pFppDiagCmd = (FPPDIAGCMD*) param;

                        if (copy_from_user(&fppdiag_state_cmd,pFppDiagCmd,
                                        sizeof(FPPDIAGCMD))) {
                                printk(KERN_INFO " FPPDIAG_LOG:Error");
				rc = -EINVAL;
				break;
                        }
                        fppdiag_update_diaglog(&fppdiag_state_cmd);
                }
                break;
		case FPPDIAG_DRV_GET_INFO:
		{
			FPPDIAGINFO	fppdiag_info;
			pFPPDIAGINFO	p_fppdiag_info = (pFPPDIAGINFO) param;

			fppdiag_get_diaginfo(&fppdiag_info);
			
			if (copy_to_user(p_fppdiag_info, &fppdiag_info ,
							sizeof(FPPDIAGINFO))) {
                                printk(KERN_INFO " FPPDIAG_DRV_GET_INFO Error");
				rc = -EINVAL;
				break;
                        }
		}
		break;
		case  FPPDIAG_DRV_DUMP_COUNTERS:
		{
			short rlen = 1;
			unsigned char rmsg[2] = {};
			struct fppdiag_config msg = {} ;
			if(comcerto_fpp_send_command(CMD_FPPDIAG_DUMP_CTRS, sizeof(msg),
						(unsigned short *)&msg,
						&rlen, rmsg))
				return -1;

		}
		break;
		default:
		{
			printk(KERN_INFO "fppdiag_ioctl: cmd : %d is not implemented", cmd);
			rc = -EINVAL;
			break;
		}
	}
	mutex_unlock(&fppdiag_drv_data->lock);
	return rc;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)


static int  fppdiag_vma_fault(struct vm_area_struct *vma ,  struct vm_fault *vmf)
{
	struct page *pageptr = NULL;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long physaddr = (unsigned long)vmf->virtual_address - vma->vm_start +
				offset;
	unsigned long pageframe = physaddr >> PAGE_SHIFT;
	void* page_ptr = NULL; 

        //printk (KERN_DEBUG "diag_vm_nopage:start: %lx, end: %lx, address: %lx offset: %lx pageframe : %x\n", vma->vm_start, vma->vm_end, (unsigned long)vmf->virtual_address, offset, pageframe);
	
	page_ptr = (void*)phys_to_virt(be32_to_cpu(fppdiag_drv_data->fpp_config.rng_baseaddr[pageframe]));
	pageptr = virt_to_page(page_ptr);
	get_page(pageptr);
	vmf->page = pageptr;
	if(pageptr)
	        return 0; 
	return VM_FAULT_SIGBUS;
}

#else

static struct page *fppdiag_vma_nopage (struct vm_area_struct *vma, 
					unsigned long address, int *type)
{
	struct page *pageptr = NOPAGE_SIGBUS;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long physaddr = address - vma->vm_start + offset;
	unsigned long pageframe = physaddr >> PAGE_SHIFT;
	void* page_ptr = NULL; 

        //printk (KERN_DEBUG "diag_vm_nopage:start: %lx, end: %lx, address: %lx offset: %lx pageframe : %x\n", vma->vm_start, vma->vm_end, address,offset,pageframe);
	
	page_ptr = (void*)phys_to_virt(be32_to_cpu(fppdiag_drv_data->fpp_config.rng_baseaddr[pageframe]));
	pageptr = virt_to_page(page_ptr);
	get_page(pageptr);
	if (type)
        	*type = VM_FAULT_MINOR;
        return pageptr;
}

#endif

void fppdiag_vma_open(struct vm_area_struct *vma)
{
	printk(KERN_NOTICE "Simple VMA open, virt %lx, phys %lx\n",
			vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void fppdiag_vma_close(struct vm_area_struct *vma)
{
	printk(KERN_NOTICE "Simple VMA close.\n");
}


static struct vm_operations_struct fppdiag_vm_ops = {
	.open		= fppdiag_vma_open,
	.close		= fppdiag_vma_close,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
        .fault          = fppdiag_vma_fault,
#else 
        .nopage         = fppdiag_vma_nopage,
#endif
};



static int fppdiag_mmap(struct file *file, struct vm_area_struct *vma)
{
        unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
        unsigned long vsize = vma->vm_end - vma->vm_start;


        printk (KERN_DEBUG "start: %lx, end: %lx off: %lx, vsize: %lx, \n",
                        vma->vm_start, vma->vm_end, offset, vsize);

	if (offset >=  __pa(high_memory) || (file->f_flags & O_SYNC))
	{
		printk(KERN_INFO "High Memory \n");
        	vma->vm_flags |= VM_IO;
	}

	vma->vm_flags |= VM_RESERVED;
    	vma->vm_ops = &fppdiag_vm_ops;
	fppdiag_vma_open(vma);
	
	return 0;
}


struct file_operations fppdiag_fops = {
        .owner =        THIS_MODULE,
        .open =         fppdiag_open,
        .release =      fppdiag_release,
        .unlocked_ioctl = fppdiag_ioctl,
        .mmap =         fppdiag_mmap,
	.poll = 	fppdiag_poll,
};
#endif // #if 0


void pfe_diags_print(fppdiag_entry_t *entry, unsigned int pe_num)
{
	FPPDIAG_ARG_TYPE *args = (FPPDIAG_ARG_TYPE *)(entry->buffer + FPPDIAG_BUFFER_SIZE-FPPDIAG_MAX_ARGS*sizeof(FPPDIAG_ARG_TYPE));
	struct pfe_diags_info * pfe_diags_info;
	unsigned long str_offset = be32_to_cpu(*(u32 *)entry->buffer);
	char string_buffer[256];

	if (pe_id[pe_num] == UTIL_ID)
		pfe_diags_info = &pfe->diags.util_diags_info;
	else
		pfe_diags_info = &pfe->diags.class_diags_info;

	if (!pfe_diags_info) {
		printk(KERN_WARNING "PFE diags: print message was received but diags strings could not be extracted from firmware, skipping.\n");
		return;
	}

	str_offset -= pfe_diags_info->diags_str_base;
	if (str_offset >= pfe_diags_info->diags_str_size) {
		printk(KERN_WARNING "PFE diags: string offset passed by PFE %s is out of bounds: %ld", pe_names[pe_num], str_offset);
		return;
	}

	snprintf(string_buffer, 256, "%s %s: %s", KERN_INFO, pe_names[pe_num], (char *)(pfe_diags_info->diags_str_array+str_offset));
	printk(string_buffer, ntohl(args[0]), ntohl(args[1]), ntohl(args[2]), ntohl(args[3]));
}

void pfe_diags_dump(fppdiag_entry_t *entry, char *pe_name)
{
	unsigned int i;

	for (i=0; i < FPPDIAG_BUFFER_SIZE; i++)
	{
		if ((i & 0x7) == 0)
			printk(KERN_INFO "\n%s: 0x%02x  ", pe_name, i);
		printk(KERN_INFO "%02x ", entry->buffer[i]);
	}
	printk(KERN_INFO " \n");
}

void pfe_diags_exception_dump(fppdiag_entry_t *entry, char *pe_name)
{
	u32 *registers = (u32 *) entry->buffer;

	printk(KERN_INFO "%s: Exception: EPC: %8x ECAS: %8x EID: %8x ED: %8x\n%s: r0/sp: %8x r1/ra: %8x r2/fp: %8x r3: %8x\n%s: r4: %8x r5: %8x r6: %8x r7: %8x\n%s: r8: %8x r9: %8x r10: %8x\n",
		pe_name, ntohl(registers[0]), ntohl(registers[1]), ntohl(registers[2]), ntohl(registers[3]),
		pe_name, ntohl(registers[4]), ntohl(registers[5]), ntohl(registers[6]), ntohl(registers[7]),
		pe_name, ntohl(registers[8]), ntohl(registers[9]), ntohl(registers[10]), ntohl(registers[11]),
		pe_name, ntohl(registers[12]), ntohl(registers[13]), ntohl(registers[14]) );
}

fppdiag_entry_t * pfe_diags_get_entry(unsigned int pe_num, unsigned int *pread_index)
{
	unsigned int read_index, page_index, previous_page_index, total_size;
	fppdiag_ctl_t *fppdiagctl = pfe->diags.fppdiag_drv_data[pe_num].virt_fppdiagctl;
	void *pageaddr;

	total_size =  pfe->diags.fppdiag_drv_data[pe_num].fpp_config.rng_size * FPPDIAG_ENTRIES_PER_PAGE;

	read_index = be32_to_cpu(fppdiagctl->read_index);
	previous_page_index = read_index/FPPDIAG_ENTRIES_PER_PAGE;
	read_index++;
	if (read_index == total_size)
		read_index = 0;
	page_index = read_index/FPPDIAG_ENTRIES_PER_PAGE;

	if (read_index % FPPDIAG_ENTRIES_PER_PAGE == 0)
		dma_sync_single_for_cpu(pfe->dev, pfe->diags.fppdiag_drv_data[pe_num].pfe_rng_baseaddr[previous_page_index], PAGE_SIZE, DMA_FROM_DEVICE);

	pageaddr = pfe->diags.fppdiag_drv_data[pe_num].virt_rng_baseaddr[page_index];
	*pread_index = read_index;
	return (fppdiag_entry_t *) (pageaddr+(read_index % FPPDIAG_ENTRIES_PER_PAGE)*FPPDIAG_ENTRY_SIZE);
}

unsigned int pfe_diags_show_current(unsigned int pe_num)
{
    fppdiag_entry_t *entry;
	unsigned int read_index;


	entry = pfe_diags_get_entry(pe_num, &read_index);

	switch (entry->flags)
	{
		case FPPDIAG_EXPT_ENTRY:
                	pfe_diags_exception_dump(entry, pe_names[pe_num]);
                	//pfe_diags_dump(entry, pe_names[pe_num]);
			break;
		case FPPDIAG_DUMP_ENTRY:
	                pfe_diags_dump(entry, pe_names[pe_num]);
			break;
		default:
			pfe_diags_print(entry, pe_num);
			break;
	}

	return read_index;
}

unsigned int fppdiag_show_one(unsigned int pe_start, unsigned int pe_end)
{
	fppdiag_ctl_t *fppdiagctl;
	int pe_num = pe_start;

	fppdiagctl = pfe->diags.fppdiag_drv_data[pe_num].virt_fppdiagctl;

	while (be32_to_cpu(fppdiagctl->read_index) == be32_to_cpu(fppdiagctl->write_index))
	{
		pe_num++;
		if (pe_num > pe_end)
			pe_num = pe_start;
		fppdiagctl = pfe->diags.fppdiag_drv_data[pe_num].virt_fppdiagctl;
	}

	fppdiagctl->read_index = cpu_to_be32(pfe_diags_show_current(pe_num));

	return 0;
}


void pfe_diags_loop(unsigned long arg)
{
	int pe_num, count;
	fppdiag_ctl_t *fppdiagctl;

	for (pe_num=0; pe_num < NUM_PE_DIAGS; pe_num++) {
		fppdiagctl = pfe->diags.fppdiag_drv_data[pe_num].virt_fppdiagctl;
		count = 40;
		while (count && (be32_to_cpu(fppdiagctl->read_index) != be32_to_cpu(fppdiagctl->write_index)))
		{
			fppdiagctl->read_index = cpu_to_be32(pfe_diags_show_current(pe_num));
			count--;
		}
	}
	add_timer(&pfe->diags.pfe_diags_timer);
}



int pfe_diags_init(struct pfe *pfe)
{
	int i;
	int ret = 0;
	fppdiag_ctl_t *phys_fppdiagctl, *virt_fppdiagctl;
	struct fppdiag_drv_dat *fppdiag_drv_data;

	printk(KERN_INFO "\n Fppdiag Driver initializing.\n");

	fppdiag_drv_data =  pfe_kzalloc(NUM_PE_DIAGS*sizeof(struct fppdiag_drv_dat), GFP_KERNEL);
	if(!fppdiag_drv_data)
	{
		ret = -ENOMEM;
		goto err0;
	}
	pfe->diags.fppdiag_drv_data = fppdiag_drv_data;

	//fppdiagctl= (fppdiag_ctl_t *)(ARAM_DIAG_CTL_ADDR);
	//ctl_phys_address = FPPDIAG_CTL_BASE_ADDR;
	//fppdiagctl =  pfe_kmalloc(NUM_PE_DIAGS*sizeof(fppdiag_ctl_t), GFP_KERNEL);
	virt_fppdiagctl = dma_alloc_coherent(pfe->dev, NUM_PE_DIAGS*sizeof(fppdiag_ctl_t), (dma_addr_t *) &phys_fppdiagctl, GFP_KERNEL);
	printk(KERN_INFO "PFE diags ctrl_phys_address = 0x%08x\n", (u32) phys_fppdiagctl);
	if(!virt_fppdiagctl)
	{
		ret = -ENOMEM;
		goto err0;
	}

#if 0 // TODO Update code to work with PFE
	if(register_chrdev(FPPDIAG_MAJOR_NUMBER, FPPDIAG_DRIVER_NAME, &fppdiag_fops) < 0){
		ret = -1;
		printk(KERN_INFO "Diag register chrdev failed ");
		goto err1;
	}
	else {
		printk(KERN_INFO "DIAG driver allocated " );
	}
#endif

	/* Initialize control structures */
	for (i=0; i<NUM_PE_DIAGS; i++) {
		fppdiag_drv_data[i].fpp_config.rng_baseaddr = NULL;
		fppdiag_drv_data[i].fpp_config.diag_ctl_flag = FPPDIAG_CTL_FREERUN;
		fppdiag_drv_data[i].fpp_config.diag_log_flag = 0xff;
		fppdiag_drv_data[i].fpp_config.diag_mod_flag = 0xff;
		fppdiag_drv_data[i].fpp_config.rng_size = DEFAULT_RING_SIZE;
		fppdiag_drv_data[i].fpp_config.fppdiagctl = &phys_fppdiagctl[i];
		fppdiag_drv_data[i].virt_fppdiagctl = &virt_fppdiagctl[i];
		fppdiag_drv_data[i].virt_rng_baseaddr = NULL;

		mutex_init(&fppdiag_drv_data[i].lock);

		/* Reset the read and write index */
		writel(0, &fppdiag_drv_data[i].virt_fppdiagctl->read_index);
		writel(0, &fppdiag_drv_data[i].virt_fppdiagctl->write_index);
	}

	/* Until full diags control path is ready, enable diags now, and poll and dump diags from kernel-space */
	pfediag_enable_all(8);

	//Start timer to dump diags periodically
	init_timer(&pfe->diags.pfe_diags_timer);
	pfe->diags.pfe_diags_timer.function = pfe_diags_loop;
	pfe->diags.pfe_diags_timer.expires = jiffies + 2;
	add_timer(&pfe->diags.pfe_diags_timer);
	//timer_init(&pfe->diags.pfe_diags_timer, pfe_diags_loop);


	return 0;
//err1:
	pfe_kfree(fppdiag_drv_data);
err0:
	return ret;
	
}

void pfe_diags_exit(struct pfe *pfe)
{
	del_timer(&pfe->diags.pfe_diags_timer);

	/* Disable the memory allocated for the modules */
	pfediag_disable_all();

#if 0
	unregister_chrdev(FPPDIAG_MAJOR_NUMBER,FPPDIAG_DRIVER_NAME);
#endif

	/* Release the data initialized for this module */
	pfe_kfree(pfe->diags.fppdiag_drv_data);
}

#else
#include "pfe_mod.h"
int pfe_diags_init(struct pfe *pfe)
{
	return 0;
}

void pfe_diags_exit(struct pfe *pfe)
{
}
#endif
