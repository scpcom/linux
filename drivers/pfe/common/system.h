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


#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include "types.h"

/*
 * Build switches
 */

/* When NO_CNG is defined to 1 - ethernet ingress 
 * congestion control is excluded 
*/
// #define NO_CNG 1

#include "hal.h"

#include "list.h"


#if !defined(GCC_TOOLCHAIN)
__asm void copy16( void *dst, void *src);
__asm void copy20( void *dst, void *src);
__asm void copy24( void *dst, void *src);
__asm void copy28( void *dst, void *src);
__asm void copy40( void *dst, void *src);


__asm void SFL_burstmemcpy(void *dst, void *src, U32 length);
__asm void *SFL_memset(void *dst, int val, unsigned long length);

__asm U64 Read64(const U64* p);
__asm void Write64(U64 data, const U64* p);
#endif

#if defined(COMCERTO_100) || defined(COMCERTO_1000)
#define PKT_ALLOC_SIZE	(2048+2*CACHE_LINE_BYTES)

#define HOSTMSG_NB	1024

#define	CACHE_LINE_BYTES	0x20
#define CACHE_ALIGN(_v)	(((U32)(_v)+CACHE_LINE_BYTES-1)&~(CACHE_LINE_BYTES-1))

#if defined(SIM)
#define DDR_HEAP_SIZE		(256*K)
#else
#define DDR_HEAP_SIZE		(2048*K)
#endif

#define DDR_MAXBLOCKSIZE	16384
#define DDR_MINBLOCKSIZE        64
#define ARAM_MAXBLOCKSIZE	256
#define ARAM_MINBLOCKSIZE       16

#define NUM_FRAG6_Q		128
#define NUM_FRAG4_Q		NUM_FRAG6_Q

/*
 * Size (and alignment) of exception path buffer
*/
#define EXPT_BUF_SIZE		0x1000
#define	EXPT_EOBUF(x) 		((U32)(x) |(EXPT_BUF_SIZE -1))

#endif

#if defined(COMCERTO_1000)

extern  unsigned char Image$$fpp_poolA$$Length;

#ifdef CFG_WIFI_OFFLOAD
#define 		METADATA_NB		 204
#else
#define 		METADATA_NB		 227
#endif

#define		MTD_LOCKDOWN_SIZE	(16*K)
#define 		NUM_RX_DESC_NT		512
#define 		NUM_RX_DESC_NT_LOG	9


#define 		ARAM_HEAP_SIZE		(9728) /* 9.5K used by forward engine (route, ct, arp caches)*/
#define		A_POOL_SIZE 		(0x3800 >> 2) //((U32) Image$$fpp_poolA$$Length  >> 2 )
#define 		METADATA_DDR_NB	 2048

//
// Maximum queue size for ipsec inbound and outbound processors.
// if #define'd to 0 will make queue size unlimited
#define		IPSEC_MAX_OCHNL_QSIZE	40		
#define		IPSEC_MAX_ICHNL_QSIZE	40		

#define	NUM_FAST_CTDATA		((38400) / FAST_CTDATA_ENTRY_SIZE) // 37.5 K

#define CTENTRY_UNIDIR_SIZE	64	/* must be power of two */

#elif defined(COMCERTO_100)

#define 		STAGGER_SIZE	13

#ifdef CFG_WIFI_OFFLOAD
#define 		METADATA_NB		 227
#else
#define 		METADATA_NB		 256
#endif

#define 		PKT_POOL_SIZE		 768		// 4*128 (rx rings) + 256 pkts in flight 
#define		MTD_LOCKDOWN_SIZE	(8*K)
#define 		ARAM_HEAP_SIZE		(4*K) /* used by forward engine (route, ct, arp caches)*/

#define 		METADATA_DDR_NB	 1024
#define 		DDR_PKT_POOL_SIZE	 1408

//
// Maximum queue size for ipsec inbound and outbound processors.
// if #define'd to 0 will make queue size unlimited
#define			IPSEC_MAX_OCHNL_QSIZE	48		
#define			IPSEC_MAX_ICHNL_QSIZE	48

#define	NUM_FAST_CTDATA		((27 * K) / FAST_CTDATA_ENTRY_SIZE)

/*
 * MC_REFCOUNT_OFFSET defines a byte-offset of 32 bit multicast reference count into data buffer
 */
#define MC_REFCOUNT_OFFSET  0

#define CTENTRY_UNIDIR_SIZE	64	/* must be power of two */

#elif defined(COMCERTO_2000)

#define NUM_FRAG6_Q		64
#define NUM_FRAG4_Q		NUM_FRAG6_Q

#define CTENTRY_UNIDIR_SIZE	128	/* must be power of two */

#define CONNTRACK_MAX		32768		/* hard limit on the number of conntracks allowed in cmm */

#endif


#define CTENTRY_BIDIR_SIZE	(2 * (CTENTRY_UNIDIR_SIZE))
#define	FAST_CTDATA_ENTRY_SIZE	CTENTRY_BIDIR_SIZE

#ifdef CFG_WIFI_OFFLOAD
#define	PORT_WIFI_IDX			WIFI0_PORT
#define IS_WIFI_PORT(port)		(((port) >= WIFI0_PORT) && ((port) < (WIFI0_PORT + MAX_WIFI_VAPS)))
#define IS_EXPT_WIFI_PORT(port)		(((port) >= EXPT_WIFI0_PORT) && ((port) < (EXPT_WIFI0_PORT + MAX_WIFI_VAPS)))
#else
#define IS_WIFI_PORT(port)		0
#define IS_EXPT_WIFI_PORT(port)		0
#endif

#if defined(CFG_WIFI_GUEST) && (defined(COMCERTO_2000_CLASS) || defined(COMCERTO_2000_UTIL))
extern U32 wifi_guest_mask;
#define IS_WIFI_GUEST_PORT(port)	((wifi_guest_mask & (1 << ((port) - WIFI0_PORT))) != 0)
#endif

#define IS_HIF_PORT(port)		(((port) >= GEM_PORTS) && ((port) < PCAP_PORT))

#if defined(COMCERTO_1000) || defined(COMCERTO_2000)
/* Software definition of input/output logical ports */
enum LOGICAL_PORT {
	GEM0_PORT = 0,
	GEM1_PORT,
#if defined(COMCERTO_2000)
	GEM2_PORT,
#endif
	GEM_PORTS,

#if defined(CFG_WIFI_OFFLOAD)
	WIFI0_PORT = GEM_PORTS,
	WIFI1_PORT,
	WIFI2_PORT,
#endif

	MAX_PHY_PORTS = GEM_PORTS + MAX_WIFI_VAPS,
	EXPT_PORT = MAX_PHY_PORTS,
	EXPT_GEM0_PORT = MAX_PHY_PORTS,
	EXPT_GEM1_PORT,
	EXPT_GEM2_PORT,

#if defined(CFG_WIFI_OFFLOAD)
	EXPT_WIFI0_PORT,
	EXPT_WIFI1_PORT,
	EXPT_WIFI2_PORT,
#endif

	PCAP_PORT = EXPT_PORT + MAX_PHY_PORTS,
	MSP_PORT,
	UTIL_PORT,
};

#if defined(CFG_WIFI_OFFLOAD)
#define MAX_PHY_PORTS_FAST	(WIFI2_PORT)
#else
#define MAX_PHY_PORTS_FAST	(GEM2_PORT)
#endif
#define MAX_PHY_PORTS_SLOW	(MAX_PHY_PORTS - MAX_PHY_PORTS_FAST)

#if defined(COMCERTO_2000) && !defined(COMCERTO_2000_CONTROL)
static inline u32 __get_tx_phy(u32 port)
{
	return port;
}

static inline u32 get_tx_phy(u32 port)
{
	if (port >= GEM_PORTS)
		return TX_PHY_TMU3;
	else
		return __get_tx_phy(port);
}

static inline u32 get_rx_phy(u32 port)
{
	if (port >= GEM_PORTS)
		return RX_PHY_HIF;
	else
		return port;
}

/* Returns client id for a given output port.
Must be called only for HIF output ports */
static inline u32 get_client_id(u32 port)
{
	if (port >= EXPT_GEM0_PORT)
		return port - EXPT_GEM0_PORT;
	else
		return port; /* only works for wifi ports */
}

static inline u32 get_expt_tx_output_port(u32 port)
{
	return port + EXPT_GEM0_PORT;
}

static inline u32 get_expt_rx_output_port(u32 port)
{
	return port - EXPT_GEM0_PORT;
}

#endif

#endif

#define SA_MAX_OP 2


/*
 * BaseOffset defines how far to offset the packet within the packet buffer
 * preserving room for any needed encapsulation headers
 */
#if defined(COMCERTO_2000)
#if defined(COMCERTO_2000_UTIL)
#define BaseOffset(mtd)  ((mtd)->base_offset)
#else
#define BaseOffset  96
#endif
#elif defined(COMCERTO_1000)
/* Align first cache line to src mac address, to improve PPPoE performance */
#define BaseOffset 90
#elif defined(COMCERTO_100)
/* Ethernet aligned to improve GEMAC DMA efficiency */
#define BaseOffset  64
#endif


#ifdef GCC_TOOLCHAIN
static __inline void DISABLE_INTERRUPTS(void)
{
}

static __inline void ENABLE_INTERRUPTS(void)
{
}

static __inline U32 SWP(U32 v, volatile U32 *wordp)
{
	return v;
}

static __inline void L1_dc_invalidate(void *start, void* end)
{
}

static __inline void L1_dc_flush(void *start, void* end) // Clean and Invalidate
{
}

static __inline void L1_dc_clean_invalidate(void *start, void* end) // Clean and Invalidate
{
}

static __inline void L1_dc_clean(void *start, void* end)
{
}

static __inline void L1_dc_drain(void)
{
}

static __inline void L1_dc_invalidate_line(void *p)
{
}

static __inline void L1_dc_clean_line(void *p)
{
}

static __inline void PRELOAD_PKT(void)
{
}

static __inline void PRELOAD_PKT2(void)
{
}

static __inline void PRELOAD(void *p)
{
}

#else
static __inline void DISABLE_INTERRUPTS(void)
{
	__schedule_barrier();  // needed to prevent compiler from moving CPSID instruction up
	// intrinsics below are merged into single instruction (cpsid if)
	__disable_irq();
	__disable_fiq();
	__schedule_barrier();  // needed to prevent compiler from moving CPSID instruction down
}

static __inline void ENABLE_INTERRUPTS(void)
{
	__schedule_barrier();  // needed to prevent compiler from moving CPSID instruction up
	// intrinsics below are merged into single instruction (cpsie if)
	__enable_fiq();
	__enable_irq();
	__schedule_barrier();  // needed to prevent compiler from moving CPSID instruction down
}


static __inline U32 SWP(U32 v, volatile U32 *wordp)

{
    U32 var = v;
	__asm {
		SWP	var,var,[wordp]
	}
	return (U32)(var);
}


static __inline U32 SWPB(U8 v, volatile U8 *bytep)
{
    U32 var = v;
	__asm {
		SWPB	var,var,[bytep]
	}
	return (U32)(var);
}

static __inline void
L1_dc_prefetch_cond(void *start, void* end) // Clean and Invalidate
{
    U32 tmp;
    __asm { 
	mrc p15,0,tmp,c7,c12,4 
	   tst tmp, #1
	   mcrreq	p15,2,end,start,c12		// prefetch range
    }
	return;
}


static __inline void
L1_dc_prefetch(void *start, void* end) // Clean and Invalidate
{
	__asm
	{	   	
	   MCRR	p15,2,end,start,c12		// prefetch range
	}	
	return;
}


static __inline void
DMB(void) // Data Memory Barrier
{
	register U32 r0;
	__schedule_barrier();			// Do not reorder across
	__asm
	{	   	
	   MOV	r0,#0
	   MCR	p15,0,r0,c7,c10,5		
	}	
	return;
}

static __inline void
DSB(void) // Data Memory Barrier
{
	register U32 r0;
	__schedule_barrier();			// Do not reorder across
	__asm
	{	   	
	   MOV	r0,#0
	   MCR	p15,0,r0,c7,c10,4		
	}	
	return;
}


static __inline void
L1_ic_dc_cache_invalidate(void) // Clean and Invalidate whole cache
{
	register U32 r0;
	__asm
	{
	   MOV	r0,#0
	   MCR	p15,0,r0,c7,c7,0		
	   MCR	p15,0,r0,c7,c10,4		// drain write buffer
	}	
	return;
}


static __inline void
L1_dc_flush(void *start, void* end) // Clean and Invalidate
{
	register U32 r0;
	__asm
	{	   	
	   MCRR	p15,0,end,start,c14		// clean+invalidate
	   MOV	r0,#0
	   MCR	p15,0,r0,c7,c10,4		// drain write buffer
	}	
	return;
}
static __inline void
L1_dc_clean_invalidate(void *start, void* end) // Clean and Invalidate
{
	__asm
	{	   	
	   MCRR	p15,0,end,start,c14		// clean+invalidate
	}	
	return;
}


static __inline void
L1_dc_clean(void *start, void* end)
{
	register U32 r0;

	__asm
	{
	   MCRR	p15,0,end,start,c12		// clean
	   MOV	r0,#0
	   MCR	p15,0,r0,c7,c10,4		// drain write buffer
	}
	return;
}

static __inline void
L1_dc_drain(void)
{
	register U32 r0;

	__asm
	{
	   MOV	r0,#0
	   MCR	p15,0,r0,c7,c10,4		// drain write buffer
	}
	return;
}

static __inline void
L1_dc_invalidate(void *start, void* end)
{
	register U32 r0;

	__asm
	{
	   TST	start,#0x1f			// is start (r0) cache aligned ?
	   BIC	start,start,#0x1f
	   MCRNE	p15,0,start,c7,c10,1	// clean first DC line to avoid aliasing with previous data if not cache aligned
	   TST	end,#0x1f
	   BIC	end,end,#0x1f
	   MCRNE	p15,0,end,c7,c14,1	// clean/invalidate last DC line to avoid aliasing with next data if needed
	   CMP	end,start	   		
	   MCRRNE p15,0,end,start,c6	// block invalidate if mreo than 1 DC line involved
	   MOV	r0,#0
	   MCR	p15,0,r0,c7,c10,4	// drain write buffer
	}
	return;
}

static __inline void
L1_dc_fastclean(void *start, void *end) 
{
    __asm
    {
	MCRR p15,0, end, start,c6
    }
}

static __inline void
L1_dc_linefill_enable(void)
{
	register U32 r0; 

	__asm
	{
		MRC p15, 7, r0, c15, c0, 0
		BIC r0, r0, #0x1
		MCR p15, 7, r0, c15, c0, 0
	}
} 


static __inline void
L1_dc_linefill_disable(void)
{
	register U32 r0; 

	__asm
	{
		MRC p15, 7, r0, c15, c0, 0
		ORR r0, r0, #0x1
		MCR p15, 7, r0, c15, c0, 0
	}
} 

static __inline void
L1_dc_invalidate_line(void *p)
{
	__asm
	{
		MCR	p15,0,p,c7,c6,1		// invalidate 1 line (only)
	}
}

static __inline void
L1_dc_clean_line(void *p)
{
	__asm
	{
		MCR	p15,0,p,c7,c14,1	// clean+invalidate 1 line
	}
}


static __inline void PRELOAD(void *p) 
{
	__schedule_barrier();  // needed to prevent compiler from moving PLD instruction up
	__asm
	{
		PLD [p]
	}
	__schedule_barrier();  // needed to prevent compiler from moving PLD instruction down
}

static __inline void PRELOAD_next(void *p) 
{
	__schedule_barrier();  // needed to prevent compiler from moving PLD instruction up
	__asm
	{
		PLD [p,CACHE_LINE_BYTES]
	}
	__schedule_barrier();  // needed to prevent compiler from moving PLD instruction down
}

#define PRELOAD_PKT() do { if (preload_addr) PRELOAD(preload_addr); } while (0)
#define PRELOAD_PKT2() do { if (preload_addr) PRELOAD_next(preload_addr); } while (0)

#endif /* GCC_TOOLCHAIN */

void some_delay(U32 count);

static inline void setbit_in_array(U8 *pbits, U32 bitindex, U32 bitval)
{
	if (bitval)
		pbits[bitindex >> 3] |= 1 << (bitindex & 0x07);
	else
		pbits[bitindex >> 3] &= ~(1 << (bitindex & 0x07));
}

static inline U32 testbit_in_array(U8 *pbits, U32 bitindex)
{
	U8 x;
	U32 bitmask;
	x = pbits[bitindex >> 3];
	bitmask = 1 << (bitindex & 0x07);
	return (x & bitmask);
}

#endif /* _SYSTEM_H_ */
