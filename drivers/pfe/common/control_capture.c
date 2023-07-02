/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */

#include "module_capture.h"
#include "fe.h"
#include "fpp.h"
#include "module_hidrv.h"
#include "module_Rx.h"

#ifdef CFG_PCAP

#if defined(COMCERTO_2000)
static U8 CLASS_DMEM_SH2(g_pcap_enable);
static CAPCtrl CLASS_DMEM_SH2(gCapCtrl)[GEM_PORTS]__attribute__((aligned(8)));
static CAPflf_hw CLASS_PE_LMEM_SH2(gCapFilter)[GEM_PORTS]__attribute__((aligned(8)));
static U8 UTIL_DMEM_SH2(g_pcap_enable);
static CAPCtrl UTIL_DMEM_SH2(gCapCtrl)[GEM_PORTS]__attribute__((aligned(8)));
static u8 g_ctx_pcap_enable = 0;
static int pktcap_update_flf_to_pfe(u8 ifindex , u32 flen, Capbpf_insn* filter);

#endif

static Capbpf_insn* pktcap_flf_alloc(int flen);
static void pktcap_flf_free(Capbpf_insn* filter);

#if !defined (COMCERTO_2000)
static void M_pktcap_Flush_channel()
{
	PMetadata mtd;

	while ((mtd = CHANNEL_GET(EVENT_PKTCAP)) != NULL)
	{
		_M_Rx_put_channel(mtd);
	}
}
#endif

/* This function enables the packet capture module. This is called
   by the capture driver to enable while starting and to disable while stopping
   the driver */
static U16 M_pktcap_enable(U16 *p,U16 Length )
{
	U16 enable_flag;
	CAPCtrl cap_ctrl[GEM_PORTS];
	int i,id;
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
#endif

	if (Length != 2)
		return ERR_WRONG_COMMAND_SIZE;

	enable_flag = *p;

	switch (enable_flag)
	{
		case CAP_STATUS_ENABLED:
			{
#if defined(COMCERTO_2000)
				if(g_ctx_pcap_enable == CAP_STATUS_ENABLED)
					return ERR_PKTCAP_ALREADY_ENABLED;

				/* local varaible */
				g_ctx_pcap_enable	= CAP_STATUS_ENABLED;
				/* global variable used to enable pcap in PFE */
				g_pcap_enable	= CAP_STATUS_DISABLED;


#else
				if (gCapExpt.cap_enable == CAP_STATUS_ENABLED)
					return ERR_PKTCAP_ALREADY_ENABLED;

				gCapExpt.cap_Q_PTR 		= 	*(U32*) SMI_CAP_EXPTQ_ADDR;
				gCapExpt.cap_enable 		= 	CAP_STATUS_ENABLED;
				gCapExpt.cap_wrt_index	= 	0;
#endif

				for (i = 0; i < GEM_PORTS; i++)
				{
					gCapCtrl[i].cap_status = CAP_IFSTATUS_DISABLED;
					gCapCtrl[i].cap_slice	= CAP_DEFAULT_SLICE;
#if !defined(COMCERTO_2000)
					if(gCapCtrl[i].cap_flf.filter)
						Heap_Free(gCapCtrl[i].cap_flf.filter);
					gCapCtrl[i].cap_flf.flen = 0;
					gCapCtrl[i].cap_flf.filter= NULL;
#endif
				}

			}
			break;
		case CAP_STATUS_DISABLED:
			{
#if defined(COMCERTO_2000)
				if (g_ctx_pcap_enable == CAP_STATUS_DISABLED)
					return ERR_PKTCAP_NOT_ENABLED;

				g_ctx_pcap_enable = CAP_STATUS_DISABLED;
				g_pcap_enable  = CAP_STATUS_DISABLED;
				/* TODO -to flush the packets in the H/W Queues */
#else
				if (gCapExpt.cap_enable == CAP_STATUS_DISABLED)
					return ERR_PKTCAP_NOT_ENABLED;

				// send the packets to forwarder in pktcap channel
				M_pktcap_Flush_channel();

				gCapExpt.cap_Q_PTR 		= 	0;
				gCapExpt.cap_enable 		= 	CAP_STATUS_DISABLED;
				gCapExpt.cap_wrt_index	= 	0;	
#endif
				for (i = 0; i < GEM_PORTS; i++)
				{
					gCapCtrl[i].cap_status = CAP_IFSTATUS_DISABLED;
					gCapCtrl[i].cap_slice	= CAP_DEFAULT_SLICE;
#if !defined(COMCERTO_2000)
					gCapCtrl[i].cap_flf.flen = 0;
					if(gCapCtrl[i].cap_flf.filter)
						Heap_Free(gCapCtrl[i].cap_flf.filter);
					gCapCtrl[i].cap_flf.filter= NULL;
#endif
				}

			}
			break;
	}
#if defined(COMCERTO_2000)
	for (i = 0; i < GEM_PORTS; i++)
	{
		cap_ctrl[i].cap_status = cpu_to_be16(gCapCtrl[i].cap_status);
		cap_ctrl[i].cap_slice = cpu_to_be16(gCapCtrl[i].cap_slice);
	}
	pe_sync_stop(ctrl, CLASS_MASK);
	/* update the DMEM in class-pe */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++){
		pe_dmem_writeb(id, g_pcap_enable,virt_to_class_dmem(&class_g_pcap_enable));
		pe_dmem_memcpy_to32(id, virt_to_class_dmem(&class_gCapCtrl[0]), &cap_ctrl[0], GEM_PORTS*sizeof(CAPCtrl));
	}
	for (i = 0; i < GEM_PORTS; i++)
		pktcap_update_flf_to_pfe(i,0,0);

	pe_start(ctrl, CLASS_MASK);

	pe_sync_stop(ctrl, UTIL_MASK);

	pe_dmem_writeb(UTIL_ID, g_pcap_enable,virt_to_util_dmem(&util_g_pcap_enable));
	pe_dmem_memcpy_to32(UTIL_ID, virt_to_util_dmem(&util_gCapCtrl[0]), &cap_ctrl[0], GEM_PORTS*sizeof(CAPCtrl));

	pe_start(ctrl, UTIL_MASK);

	for (i = 0; i < GEM_PORTS; i++)
	{
		if(gCapFilter[i].filter)
			pktcap_flf_free(gCapFilter[i].filter);
		gCapFilter[i].filter= NULL;
		gCapFilter[i].flen = 0;
	}


#endif
	return NO_ERR;
}


/* This function enables/disables the packet capture for an interface.
   This function is initiated by the cmm command */

static U16 M_pktcap_cmd_status(U16 * p,U16 Length)
{
	CAPStatCmd cap_cmd;
	int id,i;
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
#endif
	// Check length
	if (Length != sizeof(CAPStatCmd))
		return ERR_WRONG_COMMAND_SIZE;

#if !defined(COMCERTO_2000)
	if (gCapExpt.cap_enable == CAP_STATUS_DISABLED)
		return ERR_PKTCAP_NOT_ENABLED;
#else
	if (g_ctx_pcap_enable == CAP_STATUS_DISABLED)
		return ERR_PKTCAP_NOT_ENABLED;
#endif

	SFL_memcpy((U8*)&cap_cmd, (U8*)p,  sizeof(CAPStatCmd));
	if (cap_cmd.ifindex >=  GEM_PORTS)
		return ERR_UNKNOWN_INTERFACE;

	if (cap_cmd.status == CAP_IFSTATUS_ENABLED)
		gCapCtrl[cap_cmd.ifindex].cap_status = CAP_IFSTATUS_ENABLED;
	else if (cap_cmd.status ==  CAP_IFSTATUS_DISABLED)
		gCapCtrl[cap_cmd.ifindex].cap_status = CAP_IFSTATUS_DISABLED;
	else
		return ERR_CREATION_FAILED;

#if defined(COMCERTO_2000)
	/* Check to see if packet capture is enabled on atleast one interface , if so then enable the global pcap enable. If the packet capture is disabled on all interfaces then disable the global pcap enable */
	g_pcap_enable 		= 	CAP_STATUS_DISABLED;
	for (i = 0; i < GEM_PORTS; i++)
	{
		if (gCapCtrl[i].cap_status == CAP_IFSTATUS_ENABLED)
		{
			g_pcap_enable 		= 	CAP_STATUS_ENABLED;
			break;
		}

	}
	pe_sync_stop(ctrl, CLASS_MASK);

	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++){
		pe_dmem_writeb(id, g_pcap_enable,virt_to_class_dmem(&class_g_pcap_enable));
		pe_dmem_writew(id, cpu_to_be16(gCapCtrl[cap_cmd.ifindex].cap_status) ,virt_to_class_dmem(&class_gCapCtrl[cap_cmd.ifindex].cap_status));
	}
	pe_start(ctrl, CLASS_MASK);

	pe_sync_stop(ctrl, UTIL_MASK);

	pe_dmem_writeb(UTIL_ID, g_pcap_enable,virt_to_util_dmem(&util_g_pcap_enable));
	pe_dmem_writew(UTIL_ID, cpu_to_be16(gCapCtrl[cap_cmd.ifindex].cap_status) ,virt_to_util_dmem(&util_gCapCtrl[cap_cmd.ifindex].cap_status));

	pe_start(ctrl, UTIL_MASK);

#endif
	return NO_ERR;
}

/* This function configures the slice value for a particular interface */
static U16 M_pktcap_cmd_slice(U16 * p,U16 Length)
{
	CAPSliceCmd cap_cmd;
	int id;
#if defined(COMCERTO_2000)
	struct pfe_ctrl *ctrl = &pfe->ctrl;
#endif
	// Check length
	if (Length != sizeof(CAPSliceCmd))
		return ERR_WRONG_COMMAND_SIZE;

#if defined(COMCERTO_2000)
	if (g_ctx_pcap_enable == CAP_STATUS_DISABLED)
		return ERR_PKTCAP_NOT_ENABLED;
#else
	if (gCapExpt.cap_enable == CAP_STATUS_DISABLED)
		return ERR_PKTCAP_NOT_ENABLED;
#endif

	SFL_memcpy((U8*)&cap_cmd, (U8*)p,  sizeof(CAPSliceCmd));
	if (cap_cmd.ifindex >=  GEM_PORTS)
		return ERR_UNKNOWN_INTERFACE;

	gCapCtrl[cap_cmd.ifindex].cap_slice = cap_cmd.slice;
#if defined(COMCERTO_2000)
	pe_sync_stop(ctrl, CLASS_MASK);
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++){
		pe_dmem_writew(id, cpu_to_be16(gCapCtrl[cap_cmd.ifindex].cap_slice) ,virt_to_class_dmem(&class_gCapCtrl[cap_cmd.ifindex].cap_slice));
	}
	pe_start(ctrl, CLASS_MASK);
	if (pe_sync_stop(ctrl, UTIL_MASK) < 0){
		return CMD_ERR;
	}

	pe_dmem_writew(UTIL_ID, cpu_to_be16(gCapCtrl[cap_cmd.ifindex].cap_slice) ,virt_to_util_dmem(&util_gCapCtrl[cap_cmd.ifindex].cap_slice));
	pe_start(ctrl, UTIL_MASK);
#endif


	return NO_ERR;
}


static U16 M_pktcap_query(U16 *p, U16 *retlen)
{
	CAPQueryCmd *cap_cmd = (CAPQueryCmd*)p;
	int ii;

	for (ii = 0; ii < GEM_PORTS; ii++){
		cap_cmd[ii].slice   = gCapCtrl[ii].cap_slice;
		cap_cmd[ii].status = gCapCtrl[ii].cap_status;
	}

	*retlen = GEM_PORTS * sizeof(CAPQueryCmd);

	return NO_ERR;
}


#if defined(COMCERTO_2000)
static int pktcap_convert_inst_to_be(Capbpf_insn* dst,Capbpf_insn*src, u32 inst_len)
{
	int i;
	for (i = 0; i < inst_len; i++)
	{
		dst[i].code = cpu_to_be16(src[i].code);
		dst[i].jt = src[i].jt;
		dst[i].jf = src[i].jf;
		dst[i].k = cpu_to_be32(src[i].k);
	}

	return 0;
}
static int pktcap_update_flf_to_pfe(u8 ifindex , u32 flen, Capbpf_insn* filter)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	u32* dst = (u32*)&class_gCapFilter[ifindex].filter[0];

	pe_sync_stop(ctrl, CLASS_MASK);

	/* Writing address to class pe lmem */
	class_pe_lmem_memcpy_to32((u32)virt_to_class_pe_lmem(dst), filter , (flen * sizeof(Capbpf_insn)));
	class_bus_write(cpu_to_be32(flen), virt_to_class_pe_lmem(&class_gCapFilter[ifindex].flen), 4);
	pe_start(ctrl, CLASS_MASK);

	return NO_ERR;
}

static Capbpf_insn* pktcap_flf_alloc(int flen)
{
	return (pfe_kzalloc((flen * sizeof(Capbpf_insn)),GFP_KERNEL));

}
static void pktcap_flf_free(Capbpf_insn* filter)
{
	pfe_kfree(filter);
}

/* This function needs to be merged with common function of c1k
   by moving the  gCapFilter inside gCapCtrl and store everything in pe-lmem*/

static int M_pktcap_filter(CAPcfgflf * p,U16 Length)
{
	static U8 sWaitForSeq =0;
	static CAPflf sCap_flf ={};
	u32 flen;

	printk(KERN_INFO "filter recieved:%x-%x\n",p->flen,p->ifindex);
	if(!p->flen) /* Filter reset */
	{

		if(gCapFilter[p->ifindex].filter)
			pktcap_flf_free(gCapFilter[p->ifindex].filter);
		gCapFilter[p->ifindex].flen = 0;
		gCapFilter[p->ifindex].filter= NULL;

		if ((sWaitForSeq) && (sCap_flf.filter))
		{
			pktcap_flf_free(sCap_flf.filter);
			sWaitForSeq = 0;
		}

		/* update PFE before freeing the memory */
		pktcap_update_flf_to_pfe(p->ifindex,0, 0);

		return NO_ERR;
	}


	/* If fragmented update the filter */
	if(sWaitForSeq)
	{
		if(sWaitForSeq == (p->mfg & 0x7)) /* Verifies the sequence no. */
		{
			//SFL_memcpy(&sCap_flf.filter[sWaitForSeq * CAP_MAX_FLF_INSTRUCTIONS], p->filter, p->flen * sizeof(Capbpf_insn));
			pktcap_convert_inst_to_be(&sCap_flf.filter[sWaitForSeq * CAP_MAX_FLF_INSTRUCTIONS], p->filter, p->flen);

			if(p->mfg & 0x8)
				sWaitForSeq++;
			else
			{
				sWaitForSeq = 0;
				goto activate_filter;
			}
		}
		else /* Out of order sequence received, reset filter */
		{
			/* Reset filter */

			if (sCap_flf.filter)
			{
				pktcap_flf_free(sCap_flf.filter);
				sCap_flf.filter= NULL;
			}
			return ERR_PKTCAP_FLF_RESET;
		}
	}
	else
	{
		if (p->flen > PCAP_MAX_BPF_INST_SUPPORTED )
			return ERR_NOT_ENOUGH_MEMORY;
		if (p->flen > CAP_MAX_FLF_INSTRUCTIONS)
			flen = CAP_MAX_FLF_INSTRUCTIONS;
		else
			flen = p->flen;

		sCap_flf.filter= pktcap_flf_alloc(p->flen);
		if(!sCap_flf.filter)
			return ERR_NOT_ENOUGH_MEMORY;

		sCap_flf.flen = p->flen;

		/* Convert all the instructions to BE */
		//SFL_memcpy (sCap_flf.filter,  p->filter, inst_len * sizeof(Capbpf_insn));
		pktcap_convert_inst_to_be(sCap_flf.filter,p->filter,flen);

		if(p->mfg & 0x8) /* more fragments are set */
			sWaitForSeq = 1; /* expect first fragment */
		else
			goto activate_filter;
	}
	return NO_ERR;

activate_filter:

	/* Nullify old filter */
	if (gCapFilter[p->ifindex].filter)
	{
		printk(KERN_INFO "nullify oldfilter\n");
		pktcap_flf_free(gCapFilter[p->ifindex].filter);
		gCapFilter[p->ifindex].filter = NULL;
	}
	/* Activate new filter */
	gCapFilter[p->ifindex].flen= sCap_flf.flen;
	gCapFilter[p->ifindex].filter= sCap_flf.filter;
	sCap_flf.filter= NULL;

	pktcap_update_flf_to_pfe(p->ifindex, gCapFilter[p->ifindex].flen, gCapFilter[p->ifindex].filter);


	printk(KERN_INFO "Filter configured\n");
	return NO_ERR;

}
#else

static Capbpf_insn* pktcap_flf_alloc(int flen)
{
	return (Heap_Alloc(p->flen * sizeof(Capbpf_insn)));

}
static void pktcap_flf_free(Capbpf_insn* filter)
{
	Heap_Free(filter);
}

static int M_pktcap_filter(CAPcfgflf * p,U16 Length)
{

	static U8 sWaitForSeq =0;
	static CAPflf sCap_flf ={};
	u32 flen;

	if(!p->flen) /* Filter reset */
	{
		if(gCapCtrl[p->ifindex].cap_flf.filter)
			pktcap_flf_free(gCapCtrl[p->ifindex].cap_flf.filter);
		gCapCtrl[p->ifindex].cap_flf.flen = 0;
		gCapCtrl[p->ifindex].cap_flf.filter= NULL;
		return NO_ERR;
	}

	/* If fragmented update the filter */
	if(sWaitForSeq) 
	{
		if(sWaitForSeq == p->mfg & 0x7) /* Verifies the sequence no. */
		{
			SFL_memcpy(&sCap_flf.filter[sWaitForSeq * CAP_MAX_FLF_INSTRUCTIONS], p->filter, p->flen * sizeof(Capbpf_insn));

			if(p->mfg & 0x8)
				sWaitForSeq++;
			else
			{	
				sWaitForSeq = 0;
				goto activate_filter;
			}	
		}
		else /* Out of order sequence received, reset filter */
		{
			/* Reset filter */

			if (sCap_flf.filter) 
			{	
				pktcap_flf_free(sCap_flf.filter);
				sCap_flf.filter= NULL;	
			}	
			return ERR_PKTCAP_FLF_RESET; 
		}
	}
	else
	{
		if (p->flen > PCAP_MAX_BPF_INST_SUPPORTED )
			return ERR_NOT_ENOUGH_MEMORY;
		if (p->flen > CAP_MAX_FLF_INSTRUCTIONS)
			flen = CAP_MAX_FLF_INSTRUCTIONS;
		else
			flen = p->flen;

		sCap_flf.filter= (Capbpf_insn*)pktcap_flf_alloc(p->flen);

		if(!sCap_flf.filter)
			return ERR_NOT_ENOUGH_MEMORY;

		sCap_flf.flen = p->flen;
		SFL_memcpy (sCap_flf.filter,  p->filter, flen * sizeof(Capbpf_insn));

		if(p->mfg & 0x8) /* more fragments are set */
			sWaitForSeq = 1; /* expect first fragment */
		else
			goto activate_filter;
	}
	return NO_ERR;

activate_filter:

	/* Nullify old filter */
	if (gCapCtrl[p->ifindex].cap_flf.filter)  
	{
		pktcap_flf_free(gCapCtrl[p->ifindex].cap_flf.filter);
		gCapCtrl[p->ifindex].cap_flf.filter = NULL;
	}
	/* Activate new filter */				
	gCapCtrl[p->ifindex].cap_flf.flen= sCap_flf.flen; 
	gCapCtrl[p->ifindex].cap_flf.filter= sCap_flf.filter; 
	sCap_flf.filter= NULL;

	return NO_ERR;

}
#endif
static U16 M_pktcap_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd)
{
	U16 rc;
	U16 retlen = 2;
	U16 action;

	switch (cmd_code)
	{
		case CMD_PKTCAP_ENABLE:
			rc = M_pktcap_enable(pcmd, cmd_len);
			break;

		case CMD_PKTCAP_IFSTATUS:
			action = *pcmd;
			rc = M_pktcap_cmd_status(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				retlen += sizeof(CAPStatCmd);
			break;

		case CMD_PKTCAP_SLICE:
			action = *pcmd;
			rc = M_pktcap_cmd_slice(pcmd, cmd_len);
			if (rc == NO_ERR && (action == ACTION_QUERY || action == ACTION_QUERY_CONT))
				retlen += sizeof(CAPSliceCmd);
			break;

		case CMD_PKTCAP_FLF:
			rc = M_pktcap_filter((CAPcfgflf*)pcmd, cmd_len);
			break;

		case CMD_PKTCAP_QUERY:
			rc = M_pktcap_query(pcmd, &retlen);
			break;

		default:
			rc = ERR_UNKNOWN_COMMAND;
			break;
	}

	*pcmd = rc;
	return retlen;
}

int pktcap_init(void)
{
	int i;
	/* Entry point and Channel registration */
#if !defined(COMCERTO_2000)
	set_event_handler(EVENT_PKTCAP, M_pktcap_entry);
#endif

	set_cmd_handler(EVENT_PKTCAP, M_pktcap_cmdproc);
#if defined(COMCERTO_2000)
	g_pcap_enable 	= 0;
	g_ctx_pcap_enable = 0;
#else
	gCapExpt.cap_wrt_index	= 	0;
	gCapExpt.cap_irqm 		= 	IRQM_PKTCAP;
	gCapExpt.cap_enable		= 	0;
	gCapExpt.cap_Q_PTR 		= 	0;
#endif

	for (i = 0; i < GEM_PORTS; i++)
	{

		gCapCtrl[i].cap_status = CAP_IFSTATUS_DISABLED;
		gCapCtrl[i].cap_slice   = CAP_DEFAULT_SLICE;
#if !defined(COMCERTO_2000)
		gCapCtrl[i].cap_flf.flen = 0;
		gCapCtrl[i].cap_flf.filter= NULL;
#else
		gCapFilter[i].flen = 0;
		gCapFilter[i].filter = NULL;
#endif
	}

	return 0;
}

void pktcap_exit(void)
{
	return ;
}


#endif /* CFG_PCAP */
