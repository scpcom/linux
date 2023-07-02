
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

#ifndef _MODULE_CAPTURE_H_
#define _MODULE_CAPTURE_H_

#include "types.h"
#include "modules.h"
#include "channels.h"
#include "system.h"
#include "module_Rx.h"

#ifdef CFG_PCAP

#if !defined(COMCERTO_2000)
#define SMI_CAP_EXPTQ_ADDR 	0x0A0004A0
#define MAX_FILL_CAP_DESC 		RX_BUDGET// This is dependent on RX_BUDGET parameter.
#endif

#define MAX_CAP_QUERY_IFACES 	2


#define CAP_STATUS_ENABLED 	1
#define CAP_STATUS_DISABLED	0

#define CAP_DEFAULT_SLICE		96
#define CAP_MAX_FLF_INSTRUCTIONS 	30


#define CAP_IFSTATUS_ENABLED 	1
#define CAP_IFSTATUS_DISABLED	0

#define PCAP_MAX_BPF_INST_SUPPORTED	128

/* BPF Instruction class - Begin */

/*
 * Instruction classes
 */

#define BPF_CLASS(code) ((code) & 0x07)
#define         BPF_LD          0x00
#define         BPF_LDX         0x01
#define         BPF_ST          0x02
#define         BPF_STX         0x03
#define         BPF_ALU         0x04
#define         BPF_JMP         0x05
#define         BPF_RET         0x06
#define         BPF_MISC        0x07

/* ld/ldx fields */
#define BPF_SIZE(code)  ((code) & 0x18)
#define         BPF_W           0x00
#define         BPF_H           0x08
#define         BPF_B           0x10
#define BPF_MODE(code)  ((code) & 0xe0)
#define         BPF_IMM         0x00
#define         BPF_ABS         0x20
#define         BPF_IND         0x40
#define         BPF_MEM         0x60
#define         BPF_LEN         0x80
#define         BPF_MSH         0xa0

/* alu/jmp fields */
#define BPF_OP(code)    ((code) & 0xf0)
#define         BPF_ADD         0x00
#define         BPF_SUB         0x10
#define         BPF_MUL         0x20
#define         BPF_DIV         0x30
#define         BPF_OR          0x40
#define         BPF_AND         0x50
#define         BPF_LSH         0x60
#define         BPF_RSH         0x70
#define         BPF_NEG         0x80
#define         BPF_JA          0x00
#define         BPF_JEQ         0x10
#define         BPF_JGT         0x20
#define         BPF_JGE         0x30
#define         BPF_JSET        0x40
#define BPF_SRC(code)   ((code) & 0x08)
#define         BPF_K           0x00
#define         BPF_X           0x08

/* ret - BPF_K and BPF_X also apply */
#define BPF_RVAL(code)  ((code) & 0x18)
#define         BPF_A           0x10

/* misc */
#define BPF_MISCOP(code) ((code) & 0xf8)
#define         BPF_TAX         0x00
#define         BPF_TXA         0x80

#ifndef BPF_MAXINSNS
#define BPF_MAXINSNS 4096
#endif

/*
 * Macros for filter block array initializers.
 */
#ifndef BPF_STMT
#define BPF_STMT(code, k) { (unsigned short)(code), 0, 0, k }
#endif
#ifndef BPF_JUMP
#define BPF_JUMP(code, k, jt, jf) { (unsigned short)(code), jt, jf, k }
#endif

/*
 * Number of scratch memory words for: BPF_ST and BPF_STX
 */
#define BPF_MEMWORDS 16


/* BPF Instruction class - End */

#if !defined (COMCERTO_2000)

typedef struct tCAPEXPT_context {
	U32 cap_Q_PTR;		// capture descriptors base address
	U16 cap_enable;
	U16 rsvd;
	U32 cap_irqm;		// capture interrrupt
	U32 cap_wrt_index;

} CAPEXPT_context, *PCAPEXPT_context;


//STATUS BITS in cap_status

#define CAP_OWN		(1<<29)
#define CAP_WRAP	(1<<30)

#define CAP_LENGTH_MASK 0x1FFF
#define CAP_SLICE_BIT	  16

#define CAP_IFINDEX  29
#define CAP_IFINDEX_MASK 3



//typedef struct __attribute__((aligned(16))) tCapDesc {

typedef struct  tCapDesc {
	U32 cap_data;
	U32 cap_len; 
	U32 cap_status;
	U32 cap_tmpstmp;
} CAPDesc , *pCAPDesc;

typedef struct  tCapDesc_short {
	U32 cap_len; 
	U32 cap_tmpstmp;
} CAPDesc_short , *pCAPDesc_short;

#endif

#define ACTION_IF_STATUS	0x1
#define ACTION_SLICE	 	0x2

typedef struct tcmdCapStat
{
	U16 		action;
	U8		ifindex;
	U8		status;
}CAPStatCmd, *PCAPStatCmd;

typedef struct tcmdCapSlice
{
	U16 		action;
	U8		ifindex;
	U8		rsvd;
	U16 		slice;
}CAPSliceCmd, *PCAPSliceCmd;

typedef struct tcmdCapQuery{
	U16     slice;
	U16     status;
} __attribute__((packed)) CAPQueryCmd;

typedef  struct tCapbpf_insn/* bpf instruction */{
	U16 code;   /* Actual filter code */
	U8   jt;     /* Jump true */
	U8   jf;     /* Jump false */
	U32  k;      /* Generic multiuse field */
}Capbpf_insn;

#if defined(COMCERTO_2000_CLASS)
typedef struct tCapflf/* First level filter */{
	U32 flen; /* filter length */
	Capbpf_insn filter[PCAP_MAX_BPF_INST_SUPPORTED];
}CAPflf;
#else
typedef struct tCapflf/* First level filter */{
	U16 flen; /* filter length */
	Capbpf_insn *filter;
}CAPflf;
#endif

#if defined (COMCERTO_2000_CONTROL)
typedef struct tCapflf_hw/* First level filter */{
	U32 flen; /* filter length */
	Capbpf_insn filter[PCAP_MAX_BPF_INST_SUPPORTED];
}CAPflf_hw;
#endif


typedef struct tCapCfgflf/* First level filter */{
	U16 flen; /* filter length */
	U8  ifindex;
	U8  mfg; /* set to inform fpp of fragment no, last fragment has the field set to 0 */
	Capbpf_insn filter[CAP_MAX_FLF_INSTRUCTIONS];
}CAPcfgflf;

typedef struct tCAPCtrl
{
	U16 cap_status;
	U16 cap_slice;
#if !defined (COMCERTO_2000)
	CAPflf  cap_flf;
#endif
}CAPCtrl;

#if defined (COMCERTO_2000)
extern u8 g_pcap_enable;
extern u32 cap_pkt_cnt;
extern CAPCtrl gCapCtrl[GEM_PORTS];
extern CAPflf gCapFilter[GEM_PORTS];
#if !defined(COMCERTO_2000_CONTROL)
void pcap_tstamp_upd_lmem ();
u32 pcap_get_tstamp_in_us();
void M_PKTCAP_process_packet(PMetadata mtd);
void M_pktcap_process(PMetadata mtd, U8 ifindex);
void M_pktcap_process_ddrpkt(u32 port, void *ddr_addr, u32 len);
void M_pktcap_process_mcast(PMetadata mtd, void *tx_hdr_start);
#if defined(COMCERTO_2000_CLASS)
int M_pktcap_filter_frame(u8* pkt,u16 pkt_len,Capbpf_insn * filter, U32 flen) __attribute__((section ("slow_pmem")));
void pcap_init(void);
extern int (*pktcap_flf_fnptr)(u8* pkt, u16 pkt_len,  Capbpf_insn * filter, U32 flen);
#endif
static inline int M_pktcap_chk_enable(U8 ifindex)
{
	if (g_pcap_enable && (gCapCtrl[ifindex].cap_status & CAP_STATUS_ENABLED))
		return 1;

	return 0;
}
#define CAP_LMEM_WRAPS_ADDR		0xC0308100
#define CAP_LMEM_CYCLES_ADDR		0xC0308104

#if defined (COMCERTO_2000_CLASS)
#define CAP_DMEM_BUFFER			CLASS_GP_DMEM_BUF
#define CAP_DMEM_BUFFER_SIZE		CLASS_GP_DMEM_BUF_SIZE
#endif
#if defined (COMCERTO_2000_UTIL)
#include "util_dmem_storage.h"
#define CAP_DMEM_BUFFER                MEMCPY_TMP_BUF
#define CAP_DMEM_BUFFER_SIZE   MEMCPY_TMP_BUF_SIZE
#endif

#endif
#else
extern CAPEXPT_context gCapExpt;
extern CAPCtrl gCapCtrl[GEM_PORTS];
extern U32 gCapData[MAX_FILL_CAP_DESC];
extern CAPDesc_short gCapCtlShort[MAX_FILL_CAP_DESC];

void M_PKTCAP_process_packet(PMetadata mtd);
void M_pktcap_entry(void) __attribute__((section ("fast_path")));
BOOL M_pktcap_init(PModuleDesc pModule);
int M_pktcap_process(PMetadata mtd, U8 ifindex)  __attribute__((section ("fast_path")));

static inline int M_pktcap_chk_enable(PMetadata mtd , U8 ifindex)
{
	if (gCapExpt.cap_enable && (gCapCtrl[ifindex].cap_status & CAP_STATUS_ENABLED))
		return 1;

	return 0;
}
#endif
int pktcap_init(void);
void pktcap_exit(void);

#endif /* CFG_PCAP */
#endif /* _MODULE_CAPTURE_H_ */

