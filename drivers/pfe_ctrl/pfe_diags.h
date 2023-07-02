/*
 *  pfe_diags.h
 *
 *  Copyright (C) 2004,2005 Freescale Semiconductor, Inc.
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

#ifndef _PFE_DIAGS_H_
#define _PFE_DIAGS_H_

#include "pfe_mod.h"

#ifdef CFG_DIAGS
#include "fppdiag_lib.h"

#define NUM_PE_DIAGS 7 /**< No diags for the TMU PEs */


#define DEFAULT_RING_SIZE 32
#define MAX_RING_SIZE   256

#define FPPDIAG_MAX_AVBL_ENTRIES        100

#define CMD_FPPDIAG_ENABLE	0x1201
#define CMD_FPPDIAG_DISABLE	0x1202
#define CMD_FPPDIAG_UPDATE	0x1203
#define CMD_FPPDIAG_DUMP_CTRS 	0x1204


typedef struct fppdiag_config_host
{
        int ring_size;
}FPPCONFIG , *pFPPCONFIG;

typedef struct fppdiag_data
{
        int read_index;
        int entries_to_read;
}FPPDIAGDATA, *pFPPDIAGDATA;

typedef struct fppdiag_cmd
{
        union{
                unsigned short  log;
                unsigned int    module;
                unsigned int    state_size;
        }flags;
        unsigned int flags_enable;
}FPPDIAGCMD , *pFPPDIAGCMD;

typedef struct fppdiag_drv_info
{
        unsigned char   state;
        unsigned short  log;
        unsigned int    module;
}FPPDIAGINFO ,*pFPPDIAGINFO;

struct pfe_diags_info {
	unsigned long diags_str_base;
	unsigned long diags_str_size;
	char * diags_str_array;
};


#define FPPDIAG_MAJOR_NUMBER    200
#define FPPDIAG_DRIVER_NAME     "/dev/fppdiag/"


#define FPPDIAG_DRV_GET_RINGSIZE        _IOR(FPPDIAG_MAJOR_NUMBER,1,FPPCONFIG)
#define FPPDIAG_DRV_GETDATA             _IOR(FPPDIAG_MAJOR_NUMBER,2,FPPDIAGDATA)
#define FPPDIAG_DRV_FINDATA             _IOW(FPPDIAG_MAJOR_NUMBER,3,FPPDIAGDATA)
#define FPPDIAG_DRV_SET_STATE           _IOW(FPPDIAG_MAJOR_NUMBER,4,FPPDIAGCMD)
#define FPPDIAG_DRV_SET_MODULE          _IOW(FPPDIAG_MAJOR_NUMBER,5,FPPDIAGCMD)
#define FPPDIAG_DRV_SET_LOG             _IOW(FPPDIAG_MAJOR_NUMBER,6,FPPDIAGCMD)
#define FPPDIAG_DRV_GET_INFO            _IOR(FPPDIAG_MAJOR_NUMBER,7,FPPDIAGINFO)
#define FPPDIAG_DRV_DUMP_COUNTERS       _IOR(FPPDIAG_MAJOR_NUMBER,8,FPPDIAGINFO)



struct fppdiag_drv_dat
{
	struct fppdiag_config fpp_config;
	struct mutex lock;
	fppdiag_ctl_t *virt_fppdiagctl; /**< FPP diags control structure (points to same area as fpp_config.fppdiagctl, but with kernel space address) */
	void **virt_rng_baseaddr; /**< Pointer to an array of page buffer addresses (same as in fpp_config but pointer AND content are kernel space addresses) */
	dma_addr_t *pfe_rng_baseaddr; /**< Pointer to fpp_config.rng_baseaddr array (but with kernel space address) */
};

struct pfe_diags
{
	struct timer_list pfe_diags_timer;
	struct fppdiag_drv_dat *fppdiag_drv_data;
	struct pfe_diags_info class_diags_info;
	struct pfe_diags_info util_diags_info;
};

extern int comcerto_fpp_send_command(u16 fcode, u16 length, u16 *payload, u16 *resp_length, u16 *resp_payload);

#endif

int pfe_diags_init(struct pfe *pfe);
void pfe_diags_exit(struct pfe *pfe);
#endif /* _PFE_DIAGS_H_ */
