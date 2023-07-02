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


/*****************************************************************************************
*       scc.h                                                                                                                                       
*                                                                                                                                                        
*       ARM System Control Coprocessor Interface 
*                                                                                                                                                          
******************************************************************************************/ 
#ifndef SCC_HEADER
#define SCC_HEADER

/************************************************************************
 * 			Register Bit Fields Definitions
 ************************************************************************/

/*
 *  Main ID Register Bit Fields
 */
#define ID_VENDOR(x)  		(((x) >> 24) & 0x000000FF)
#define ID_VENDOR_ARM  		0x00000041
#define ID_VENDOR_DEC		0x00000044
#define ID_VENDOR_INTEL		0x00000069
#define ID_VAR_NUM(x)  		(((x) >> 20) & 0x0000000F)
#define ID_ARCH(x)		(((x) >> 16) & 0x0000000F)
#define ID_ARCH_4		0x00000001
#define ID_ARCH_4T		0x00000002
#define ID_ARCH_5		0x00000003
#define ID_ARCH_5T		0x00000004
#define ID_ARCH_5TE		0x00000005
#define ID_ARCH_6  		0x00000007
#define ID_PART_NUM(x) 		(((x) >> 4)  & 0x00000FFF)
#define ID_PART_ARM920T		0x00000920
#define ID_PART_ARM1136JS	0x00000B36
#define ID_REVISION(x)  	(((x) >> 0)  & 0x0000000F)

/*
 *  Cache Type Register Bit Fields
 */
#define CT_CTYPE(x)		(((x) >> 25) & 0x0000000F)
#define CT_CTYPE_WT     	0x00000000
#define CT_CTYPE_WB     	0x00000001
#define CT_CTYPE_WB_1   	0x00000002
#define CT_CTYPE_FMT_A  	0x00000006
#define CT_CTYPE_FMT_B  	0x00000007
#define CT_CTYPE_FMT_C  	0x0000000E
#define CT_S			0x01000000
#define CT_DSIZE(x)		(((x) >> 12) & 0x00000FFF)
#define CT_ISIZE(x)     	(((x) >> 0)  & 0x00000FFF)
#define CT_CACHE_LINE(s) 	(1 << (3 + ((x) & 0x3)))
#define CT_CACHE_M(x)		(2 + ((x) >> 2) & 0x1))
#define CT_CACHE_ASSOC(x)       (CT_CACHE_M(x) << ((((x) >> 3) & 0x7)-1)))
#define CT_CACHE_SIZE(x)	(CT_CACHE_M(x) << ((((x) >> 6) & 0x7)+8)))

/*
 *  Translation Table Base Register Bit Fields
 */
#define TTB_BASE_MASK		0xFFFFC000
#define TTB_BASE_SHIFT		14
#define TTB_RGN_MASK		0x00000018
#define TTB_RGN_NO_CACHE	(0 << 3)
#define TTB_RGN_CACHE_WB_WA	(1 << 3)
#define TTB_RGN_CACHE_WT	(2 << 3)
#define TTB_RGN_CACHE_WB	(3 << 3)
#define TTB_P			0x00000004
#define TTB_S			0x00000002
#define TTB_C			0x00000001

/*
 *  Translation Table Base Control Register Bit Fields
 */
#define TTBCR_TTB0_16K		0x00000000
#define TTBCR_TTB0_8K		0x00000001
#define TTBCR_TTB0_4K		0x00000002
#define TTBCR_TTB0_2K		0x00000003
#define TTBCR_TTB0_1K		0x00000004
#define TTBCR_TTB0_512		0x00000005
#define TTBCR_TTB0_256		0x00000006
#define TTBCR_TTB0_128		0x00000007

/*
 *  Domain Access Control Register Bit Fields
 */
#define DACR_Dx_SHIFT(x)	((x) << 1)
#define DACR_Dx_MASK(x)		(0x3 << DACR_Dx_SHIFT(x))
#define DACR_NO_ACCESS		0x00000000
#define DACR_CLIENT		0x00000001
#define DACR_MANAGER		0x00000003

/*
 *  TLB Type Register
 */
#define TLBTYPE_ILSIZE(x)	(((x) >> 16) & 0x000000FF)
#define TLBTYPE_DLSIZE(x)	(((x) >> 8)  & 0x000000FF)
#define TLBTYPE_U 		0x00000001

/*
 *  Control Register Bit Fields 
 */
#define CR_ASYNC		0xC0000000
#define CR_SYNC			0x40000000
#define CR_AFE			0x20000000      
#define CR_TRE			0x10000000	
#define CR_EE			0x02000000	
#define CR_VE			0x01000000	
#define CR_XP			0x00800000	
#define CR_U			0x00400000	
#define CR_FI			0x00200000	
#define CR_IT			0x00040000      
#define CR_DT			0x00010000      
#define CR_L4			0x00008000	
#define CR_RR			0x00004000	
#define CR_V			0x00002000	
#define CR_I			0x00001000	
#define CR_Z			0x00000800	
#define CR_F			0x00000400	
#define CR_R			0x00000200	
#define CR_S			0x00000100	
#define CR_B			0x00000080	
#define CR_D			0x00000020	
#define CR_P			0x00000010	
#define CR_W			0x00000008	
#define CR_C			0x00000004	
#define CR_A			0x00000002	
#define CR_M			0x00000001	


/*
 *  Auxiliary Control Register Bit fields
 */ 
#define ACR_CZ			0x00000040	
#define ACR_RV  		0x00000020	 
#define ACR_RA			0x00000010	
#define ACR_TR			0x00000008	
#define ACR_SB			0x00000004	
#define ACR_DB			0x00000002	
#define ACR_RS			0x00000001  	

/*
 *  Coprocessor Access Control Register Bit Fields
 */
#define CACR_CPx_SHIFT(x)	((x) << 1)
#define CACR_CPx_MASK(x)	(0x3 << CACR_CPx_SHIFT(x))
#define CACR_NO_ACCESS		0x00000000
#define CACR_SUPERVISOR		0x00000001
#define CACR_ALL_ACCESS		0x00000003


/*
 *  TCM Status Register Bit Fields
 */
#define TCMS_ITCM(x)		(((x) >>  0) & 0x3)
#define TCMS_DTCM(x)		(((x) >> 16) & 0x3)

/*
 *  Data TCM Region Register Bit Fields
 */
#define DTCM_BASE_MASK          0xFFFFF000
#define DTCM_BASE_SHIFT		12
#define DTCM_SIZE(x)		(((x) >> 2) & 0x1F)
#define DTCM_SIZE_4K		0x00000003
#define DTCM_SIZE_8K		0x00000004
#define DTCM_SIZE_16K		0x00000005
#define DTCM_SIZE_32K		0x00000006
#define DTCM_SIZE_64K		0x00000007
#define DTCM_SC			0x00000002
#define DTCM_En			0x00000001

/*
 *  Instruction TCM Region Register Bit Fields
 */
#define ITCM_BASE_MASK          0xFFFFF000
#define ITCM_BASE_SHIFT		12
#define ITCM_SIZE(x)		(((x) >> 2) & 0x1F)
#define ITCM_SIZE_4K		0x00000003
#define ITCM_SIZE_8K		0x00000004
#define ITCM_SIZE_16K		0x00000005
#define ITCM_SIZE_32K		0x00000006
#define ITCM_SIZE_64K		0x00000007
#define ITCM_SC			0x00000002
#define ITCM_En			0x00000001

/*
 * TLB Lockdown Register Bit Fields
 */
#define TLBLOCK_P		0x00000001

/*
 * Cache Dirty Status Register Bit Fields
 */
#define CDSR_C			0x00000001

/*
 * Data Fault Status Register Bit Fields
 */
#define DFSR_RW      		0x00000800
#define DFSR_DOMAIN(x)		(((x) >> 4) & 0x0F)
#define DFSR_DOMAIN_SHIFT	4
#define DFSR_STATUS(x)		(((x) & 0x0F) | (((x) >> 6) & 0x10))

/*
 * Instruction Fault Status Register Bit Fields
 */
#define IFSR_STATUS		((x) & 0x0F)

/*
 * Block Transfer Status Register Bit Fields
 */
#define BTSR_R			0x00000001


/*
 * Performance Monitor Control Register Bit Fields
 */
#define PMNC_EVT_COUNT0(x)	(((x) >> 20) & 0xFF)
#define PMNC_EVT_COUNT1(x)	(((x) >> 12) & 0xFF)
#define PMNC_X			0x00000400
#define PMNC_CCR		0x00000200
#define PMNC_CR1		0x00000100
#define PMNC_CR0		0x00000080
#define PMNC_EC1		0x00000020
#define PMNC_EC0		0x00000010
#define PMNC_D			0x00000008
#define PMNC_C			0x00000004
#define PMNC_P			0x00000002
#define PMNC_E			0x00000001

/************************************************************************
 * 			Internal API support macro definitions
 ************************************************************************/
typedef unsigned long _u32;

#define CP15_RD_API static __inline unsigned long
#define CP15_WR_API static __inline void

#define CP15_GET(Opcode_1, CRn, CRm, Opcode_2)				\
	register unsigned long value;					\
	__asm {mrc p15,##Opcode_1,value,##CRn, ##CRm, ##Opcode_2}	\
	return value;			

#define CP15_PUT(Opcode_1, value, CRn, CRm, Opcode_2)			\
	__asm {mcr p15,##Opcode_1,value,##CRn, ##CRm, ##Opcode_2}	

#define CP15_PUT2(Opcode_1, end_addr, start_addr, CRn)	\
	__asm {mcrr p15, ##Opcode_1, ##end_addr, ##start_addr, ##CRn}

#define CP15_SBZ(Opcode_1, CRn, CRm, Opcode_2)				\
	register unsigned long SBZ = 0;					\
	__asm {mcr p15,##Opcode_1,SBZ,##CRn, ##CRm, ##Opcode_2}	

#if !defined(COMCERTO_2000)
/************************************************************************
 * 			System control and configuraion
 ************************************************************************/
CP15_RD_API sys_read_cpuid()			{CP15_GET(0,c0,c0,0)}
CP15_RD_API sys_read_cachetype()		{CP15_GET(0,c0,c0,1)}
CP15_WR_API sys_write_control(_u32 val)		{CP15_PUT(0,val,c1,c0,0)}
CP15_RD_API sys_read_control()			{CP15_GET(0,c1,c0,0)}
CP15_WR_API sys_write_aux_control(_u32 val)	{CP15_PUT(0,val,c1,c0,1)}
CP15_RD_API sys_read_aux_control()		{CP15_GET(0,c1,c0,1)}
CP15_WR_API sys_write_access_control(_u32 val)	{CP15_PUT(0,val,c1,c0,2)}
CP15_RD_API sys_read_access_control()		{CP15_GET(0,c1,c0,2)}


/************************************************************************
 * 			MMU control and configuration
 ************************************************************************/
CP15_RD_API mmu_read_tlb_type()			{CP15_GET(0,c0,c0,3)}
CP15_WR_API mmu_write_ttb0(_u32 val) 		{CP15_PUT(0,val,c2,c0,0)}
CP15_RD_API mmu_read_ttb0()			{CP15_GET(0,c2,c0,0)}
CP15_WR_API mmu_write_ttb1(_u32 val) 		{CP15_PUT(0,val,c2,c0,1)}
CP15_RD_API mmu_read_ttb1()			{CP15_GET(0,c2,c0,1)}
CP15_WR_API mmu_write_tt_control(_u32 val)	{CP15_PUT(0,val,c2,c0,2)}
CP15_RD_API mmu_read_tt_control()		{CP15_GET(0,c2,c0,2)}
CP15_WR_API mmu_write_domain_access(_u32 val)	{CP15_PUT(0,val,c3,c0,0)}
CP15_RD_API mmu_read_domain_access()		{CP15_GET(0,c3,c0,0)}
CP15_WR_API mmu_write_DFS(_u32 val) 		{CP15_PUT(0,val,c5,c0,0)}
CP15_RD_API mmu_read_DFS() 			{CP15_GET(0,c5,c0,0)}
CP15_WR_API mmu_write_IFS(_u32 val) 		{CP15_PUT(0,val,c5,c0,1)}
CP15_RD_API mmu_read_IFS()			{CP15_GET(0,c5,c0,1)}
CP15_WR_API mmu_write_fault_address(_u32 val) 	{CP15_PUT(0,val,c6,c0,0)}
CP15_RD_API mmu_read_fault_address() 		{CP15_GET(0,c6,c0,0)}
CP15_WR_API mmu_tlb_ic_invalidate()		{CP15_SBZ(0,c8,c5,0)}
CP15_WR_API mmu_tlb_ic_invalidate_entry(_u32 val){CP15_PUT(0,val,c8,c5,1)}
CP15_WR_API mmu_tlb_dc_invalidate()		{CP15_SBZ(0,c8,c6,0)}
CP15_WR_API mmu_tlb_dc_invalidate_entry(_u32 val){CP15_PUT(0,val,c8,c6,1)}
CP15_WR_API mmu_tlb_invalidate()		{CP15_SBZ(0,c8,c7,0)}
CP15_WR_API mmu_tlb_invalidate_entry(_u32 val)	{CP15_PUT(0,val,c8,c7,1)}
CP15_WR_API mmu_tlb_lockdown(_u32 val)		{CP15_PUT(0,val,c10,c0,0)}
CP15_WR_API mmu_tlb_dc_lockdown(_u32 val)	{CP15_PUT(0,val,c10,c0,0)}
CP15_WR_API mmu_tlb_ic_lockdown(_u32 val)	{CP15_PUT(0,val,c10,c0,1)}


/************************************************************************
 * 				Cache Operations
 ************************************************************************/
CP15_WR_API cache_ic_invalidate() 		{CP15_SBZ(0,c7,c5,0)}
CP15_WR_API cache_invalidate()			{CP15_SBZ(0,c7,c7,0)}
CP15_WR_API cache_ic_invalidate_line(_u32 val)	{CP15_PUT(0,val,c7,c5,1)}
CP15_WR_API cache_dc_invalidate() 		{CP15_SBZ(0,c7,c6,0)}
CP15_WR_API cache_dc_clean()          		{CP15_SBZ(0,c7,c10,0)}
CP15_WR_API cache_dc_clean_invalidate()     	{CP15_SBZ(0,c7,c14,0)}
CP15_WR_API cache_dc_invalidate_line(_u32 val)	{CP15_PUT(0,val,c7,c6,1)}
CP15_WR_API cache_dc_clean_line(_u32 val)	{CP15_PUT(0,val,c7,c10,1)}
CP15_WR_API cache_dc_clean_invalidate_line(_u32 val) {CP15_PUT(0,val,c7,c14,1)}
CP15_WR_API drain_write_buffer()		{CP15_SBZ(0,c7,c10,4)}
CP15_WR_API cache_dc_memory_barrier()		{CP15_SBZ(0,c7,c10,5)}
CP15_WR_API wait_for_interrupt()		{CP15_SBZ(0,c7,c0,4)}
CP15_WR_API flush_prefetch_buffer() 		{CP15_SBZ(0,c7,c5,4)}
CP15_WR_API flush_branch_target()		{CP15_SBZ(0,c7,c5,6)}
CP15_WR_API flush_branch_target_entry(_u32 val)	{CP15_PUT(0,val,c7,c5,6)}
CP15_RD_API cache_dirty_status()		{CP15_GET(0,c7,c10,6)}
CP15_RD_API cache_block_status()		{CP15_GET(0,c7,c12,4)}

#if defined (M829_BUILD) || defined (M821_BUILD)
CP15_WR_API cache_ic_invalidate_range(_u32 sa, _u32 ea)   {CP15_PUT2(0,ea,sa,c5)}
CP15_WR_API cache_dc_invalidate_range(_u32 sa, _u32 ea)   {CP15_PUT2(0,ea,sa,c6)}
CP15_WR_API cache_dc_clean_range(_u32 sa, _u32 ea)        {CP15_PUT2(0,ea,sa,c12)}
CP15_WR_API cache_dc_clean_invalidate_range(_u32 sa, _u32 ea) {CP15_PUT2(0,ea,sa,c14)}
#endif


/************************************************************************
 * 			TCM control and configuration (ARM1136)
 ************************************************************************/
CP15_RD_API tcm_read_status()			{CP15_GET(0,c0,c0,2)}
CP15_RD_API tcm_read_data_region()		{CP15_GET(0,c9,c1,0)}
CP15_WR_API tcm_write_data_region(_u32 val)	{CP15_PUT(0,val,c9,c1,0)}
CP15_RD_API tcm_read_code_region()		{CP15_GET(0,c9,c1,1)}
CP15_WR_API tcm_write_code_region(_u32 val)	{CP15_PUT(0,val,c9,c1,1)}
                                     

/************************************************************************
 * 				DMA control (ARM1136)
 ************************************************************************/
CP15_RD_API dma_read_present()			{CP15_GET(0,c11,c0,0)}
CP15_RD_API dma_read_queued()			{CP15_GET(0,c11,c0,1)}
CP15_RD_API dma_read_running()			{CP15_GET(0,c11,c0,2)}
CP15_RD_API dma_read_interrupting()		{CP15_GET(0,c11,c0,3)}
CP15_RD_API dma_read_user_access()		{CP15_GET(0,c11,c1,0)}
CP15_WR_API dma_write_user_access(_u32 val)	{CP15_PUT(0,val,c11,c1,0)}
CP15_RD_API dma_read_channel_num()		{CP15_GET(0,c11,c2,0)}
CP15_WR_API dma_write_channel_num(_u32 val)	{CP15_PUT(0,val,c11,c2,0)}
CP15_WR_API dma_stop(_u32 val)			{CP15_PUT(0,val,c11,c3,0)}
CP15_WR_API dma_start(_u32 val)			{CP15_PUT(0,val,c11,c3,1)}
CP15_WR_API dma_clear(_u32 val)			{CP15_PUT(0,val,c11,c3,2)}
CP15_RD_API dma_read_control()                  {CP15_GET(0,c11,c4,0)}
CP15_RD_API dma_read_int_start_addr()           {CP15_GET(0,c11,c5,0)}
CP15_RD_API dma_read_ext_base_addr()            {CP15_GET(0,c11,c6,0)}
CP15_RD_API dma_read_int_end_addr()           	{CP15_GET(0,c11,c7,0)}
CP15_RD_API dma_read_chan_status()              {CP15_GET(0,c11,c8,0)}
CP15_WR_API dma_write_control(_u32 val)          {CP15_PUT(0,val,c11,c4,0)}
CP15_WR_API dma_write_int_start_addr(_u32 val)   {CP15_PUT(0,val,c11,c5,0)}
CP15_WR_API dma_write_ext_base_addr(_u32 val)    {CP15_PUT(0,val,c11,c6,0)}
CP15_WR_API dma_write_int_end_addr(_u32 val)     {CP15_PUT(0,val,c11,c7,0)}
CP15_WR_API dma_write_chan_status(_u32 val)      {CP15_PUT(0,val,c11,c8,0)}


/************************************************************************
 * 			System performance monitoring (ARM1136)
 ************************************************************************/
CP15_RD_API sysmon_read_control()		{CP15_GET(0,c15,c12,0)}
CP15_WR_API sysmon_write_control(_u32 val)	{CP15_PUT(0,val,c15,c12,0)}
CP15_RD_API sysmon_read_cycle_counter()		{CP15_GET(0,c15,c12,1)}
CP15_WR_API sysmon_write_cycle_counter(_u32 val)	{CP15_PUT(0,val,c15,c12,1)}
CP15_RD_API sysmon_read_counter0()		{CP15_GET(0,c15,c12,2)}
CP15_WR_API sysmon_write_counter0(_u32 val)	{CP15_PUT(0,val,c15,c12,2)}
CP15_RD_API sysmon_read_counter1()		{CP15_GET(0,c15,c12,3)}
CP15_WR_API sysmon_write_counter1(_u32 val)	{CP15_PUT(0,val,c15,c12,3)}

#endif

#endif /* SCC_HEADER */
