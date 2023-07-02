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


#ifndef _MODULE_IPSEC_C2000_H_
#define _MODULE_IPSEC_C2000_H_

#include "types.h"
#include "channels.h"
#include "modules.h"
#include "fpart.h"
#include "fe.h"
#include "common_hdrs.h"
#include "layer2.h"
#include "pfe/cbus/class_csr.h"



#define NUM_LMEM_SA		64
#define HW_SA_VALID		(1 << 0)
#define HW_SA_USED		(1 << 1)
#define HW_SA_UPDATING		(1 << 2)
#define SA_CACHE_COUNTER	128


#define ESPAH_USE_PRNG 1
#define ELP_HW_BYTECNT          1       // uses elliptic hardware bytecount
//#define ESPAH_USE_TRNG 1
// TRNG would require semaphore as spacc (linux) uses it as well
#if defined(ESPAH_USE_TRNG) && defined(ESPAH_USE_PRNG)
#       error "only one of ESPAH_USE_TRNG or ESPAH_USE_PRNG may be defined"
#endif
#if     !defined(ESPAH_USE_TRNG) && !defined(ESPAH_USE_PRNG)
#       error "one of ESPAH_USE_TRNG or ESPAH_USE_PRNG needs to be defined"
#endif

typedef struct _tSA_lft_cur {
#if     !defined(ELP_HW_BYTECNT)||(ELP_HW_BYTECNT == 0)
        U64     bytes;
#else
        /* Elliptic hardware does byte accounting. */
#endif
        U64     packets;
}SA_lft_cur, *PSA_lft_cur;


typedef struct _tSAID {	
	union
	{
	       /*Unused	U32		a4; */
		U32 			a6[4];
		U32			top[4]; // alias
	} daddr;		
	U32		saddr[4];	// added for NAT-T transport mode
	U32		spi;
	U8		proto;
	U8		unused[3];
} SAID, *PSAID;	


#define SA_STATE_INIT		0x1
#define SA_STATE_VALID		0x2
#define SA_STATE_DEAD		0x3
#define SA_STATE_EXPIRED	0x4
#define SA_STATE_DYING		0x5


/*
 * _TSAEntry.flags values
 */
#define SA_NOECN	1
#define SA_DECAP_DSCP	2
#define SA_NOPMTUDISC	4
#define SA_WILDRECV	8
/* Local mirror of the sa flags.  */
#define	SA_ENABLED 	0x10		
#define SA_ALLOW_SEQ_ROLL 0x20


/* Hardware structure for elliptic security association */
typedef struct _tElliptic_SA {
	U32		 	seq; 		//+4
	U32		 	ext_seq;	// +0
	U64			anti_replay_mask;	//+8
	U32			auth_key1;		//+0x14
	U32			auth_key0;		//+0x10
	U32			auth_key3;		//+0x1c
	U32			auth_key2;		//+0x18
	U32			cipher0;	//+0x24
	U32			auth_key4;		//+0x20
	U32			cipher2;	//+0x2c
	U32			cipher1;	//+0x28	
	U32			cipher4;	//+0x34
	U32			cipher3;	//+0x30
	U32			cipher6;	//+0x3C
	U32			cipher5;	//+0x38
	union{
		U32			CTR_Nonce;	//+0x44
		U32			iv0;
	};
	U32			cipher7;	//+0x40
	union {					//+0x4C
	    U32			ext_auth_key1;
	    U32			iv2;
	};
	union {					//+0x48
	    U32			ext_auth_key0;
	    U32			iv1;
	};
	U32			spi;		//+0x54
	union {					//+0x50
	    U32			ext_auth_key2;
	    U32			iv3;
	};
	U8			pad1a[16];
	U32			hard_ttl_hi;	//+0x6c
	U8			pad1b[4];	//+0x68
	U32			soft_ttl_hi;	//+0x74
	U32			hard_ttl_lo;	//+0x70
	U16			flags;		//+0x7c
	U8			res3;
	U8			algo;
	U32			soft_ttl_lo;	//+0x78
} Elliptic_SA, *PElliptic_SA;


#ifdef CFG_STATS
typedef struct _tSAStatEntry {
       U32 total_pkts_processed;
       U32 last_pkts_processed;
       U64 total_bytes_processed;
       U64 last_bytes_processed;
}SAStatEntry , *PSAStatEntry;
#endif


#if defined(COMCERTO_2000_CONTROL)
typedef struct _tSAEntry {
	struct slist_entry      list_spi;
	struct slist_entry      list_h;
	U16			hash_by_h;
        U16			hash_by_spi;
        struct _tSAID           id;             // SA 3-tuple
        struct _tElliptic_SA *elp_sa;           // pointer to elliptic SA descriptor
	U32			elp_sa_dma_addr;
        U8                      family;         // v4/v6
        U8                      header_len;     // ipv4/ipv6 tunnel header
        U8                      mode;           // Tunnel / transport mode
        U8                      flags;          // ECN, TOS ...
        struct _tSA_lft_cur lft_cur;
        struct _tSA_lft_conf lft_conf;
        U8                      direction;      // inbound / outbound
	U8			state:7;          // valid / expired / dead / dying
	U8			notify:1;
        U8                      blocksz;
        U8                      icvzs;
        U16                     handle;
        U16                     mtu;            // used for Transport mode
        union                           // keep union 32 bits aligned !!!
        {
                ipv4_hdr_t      ip4;
                ipv6_hdr_t      ip6;
        } tunnel;
        U16                             dev_mtu;
        /*NAT-T modifications*/
        struct
        {
                unsigned short sport;
                unsigned short dport;
                void*          socket;
        }natt;
	struct _t_hw_sa_entry 	*hw_sa;    /*pointer to the software socket entry */

	U32 route_id;
	PRouteEntry pRtEntry;

} SAEntry, *PSAEntry;

/* 
** This hw_sa structure is being shared by both class and utilpe.
CLASS uses the initial part of structure, UTIL starts from hw_sa_flags
to the end of the structure. 

CLASS uses CLASS_PE_OFFSET and UTIL uses UTIL_PE_OFFSET(which are defined below)
 as the starting offset while fetching the entry into the PFE  

IMPORTANT: Whenever structure members are added/deleted to this structure 
pls take care of above assumption.
*/

typedef struct _t_hw_sa_entry
{
	/* only class requires this */
        union
        {
                ipv4_hdr_t      ip4;
                ipv6_hdr_t      ip6;
        } tunnel;

        struct _tSAID           id;
	struct hw_route		route;

	/* util and class */
	u32	hw_sa_flags;
	u32	dma_addr;
	u32 	next_h;
	u32	next_spi;

	u16	handle;
	u16	dev_mtu;
	u16	mtu;
	u16	unused1;

        u8	family;       // v4/v6
        u8	header_len;   // ipv4/ipv6 tunnel header
        u8	mode;         // Tunnel / transport mode
        u8	flags;        // ECN, TOS ...
        u8	state;       // valid / expired / dead / dying
	u8	unused[3];

	/* only utilpe */

        struct _tSA_lft_cur     lft_cur;

        struct _tElliptic_SA    *elp_sa;  // pointer to elliptic SA descriptor
	u32	unused3;

        struct
        {
                unsigned short sport;
                unsigned short dport;
                void*          socket;
        }natt;

        struct _tSA_lft_conf    lft_conf;

#ifdef CFG_STATS
	struct _tSAStatEntry stats;
#endif
	/* HOST only */

	struct dlist_head       list_h;
        struct dlist_head       list_spi;
	struct _tSAEntry 	*sw_sa;    /*pointer to the software socket entry */
	struct _tElliptic_SA 	*elp_virt_sa; /* pointer to the elliptic sa entry */ 
	struct _t_hw_sa_entry    *lmem_next;
        unsigned long           removal_time;

}hw_sa_entry;

#elif defined(COMCERTO_2000_CLASS)

typedef struct _tSAEntry {
	/* only class requires this */
	union
	{
		ipv4_hdr_t      ip4;
		ipv6_hdr_t      ip6;
	} tunnel;

	struct _tSAID           id;
	RouteEntry		route;

	/* util and class */
	u32     hw_sa_flags;
	u32     dma_addr;
	u32     next_h;
	u32     next_spi;

	u16     handle;
	u16     dev_mtu;
	u16     mtu;
        u16     unused1;

        u8      family;       // v4/v6
        u8      header_len;   // ipv4/ipv6 tunnel header
        u8      mode;         // Tunnel / transport mode
        u8      flags;        // ECN, TOS ...
        u8      state;       // valid / expired / dead / dying
        u8      unused[3];
}SAEntry , *PSAEntry;


#else
/* utilpe structures */
typedef struct _tSAEntry {
	/* util and class */
        u32     hw_sa_flags;
        u32     dma_addr;
        u32     next_h;
        u32     next_spi;

        u16     handle;
        u16     dev_mtu;
        u16     mtu;
        u16     unused1;

        u8      family;       // v4/v6
        u8      header_len;   // ipv4/ipv6 tunnel header
        u8      mode;         // Tunnel / transport mode
        u8      flags;        // ECN, TOS ...
        u8      state;       // valid / expired / dead / dying
        u8      unused[3];

	struct _tSA_lft_cur     lft_cur;

        struct _tElliptic_SA    *elp_sa;  // pointer to elliptic SA descriptor
        u32     unused2;

        struct
        {
                unsigned short sport;
                unsigned short dport;
                void*          socket;
        }natt;

        struct _tSA_lft_conf    lft_conf;

#ifdef CFG_STATS
	struct _tSAStatEntry stats;
#endif

}SAEntry , *PSAEntry;

typedef struct _tSACacheEntry {
	SAEntry sa;
	u32 cache_cnt;
}SACacheEntry , *PSACacheEntry;

#endif


#if defined(COMCERTO_2000_UTIL)
#define UTIL_LMEM_SA_START_OFFSET 	96
#define UTIL_LMEM_UPDATE_STATE_LEN	16
#endif
/* These are the offsets used to fetch the SA structure.
The initial part of SA structure is used to fetch in CLASS where as
later part is used in UTIL. 

Whenever there is a change in the SA structure (addition or deletion of structure members)
the offset should be modified */

#if defined (COMCERTO_2000_UTIL) || defined (COMCERTO_2000_CLASS)
#define UTIL_PE_OFFSET		96
#define CLASS_PE_OFFSET		0
#endif

/*
 * Hardware dependencies for Elliptic engine
 *
*/
#define ELP_SA_ALIGN 128
#define ELP_SA_ALIGN_LOG 7
#if	(ELP_SA_ALIGN != (1<< ELP_SA_ALIGN_LOG))
#	error "fix  ELP_SA_ALIGN or ELP_SA_ALIGN_LOG"
#endif

/*
 * Codes
*/
#define ELP_HMAC_NULL	0
#define ELP_HMAC_MD5	1
#define ELP_HMAC_SHA1	2
#define ELP_HMAC_SHA2	3
#define ELP_GCM64	4
#define ELP_GCM96	5
#define ELP_GCM128	6

#define ELP_CIPHER_NULL 0
#define ELP_CIPHER_DES	 1
#define ELP_CIPHER_3DES 2
#define ELP_CIPHER_AES128 3
#define ELP_CIPHER_AES192 4
#define ELP_CIPHER_AES256 5
#define ELP_CIPHER_AES128_GCM 6
#define ELP_CIPHER_AES192_GCM 7
#define ELP_CIPHER_AES256_GCM 8
#define ELP_CIPHER_AES128_CTR 9
#define ELP_CIPHER_AES192_CTR 10
#define ELP_CIPHER_AES256_CTR 11



// Elliptic ESP AH Offload Engine registers
#define ESPAH_BASE	0x00000
#define ESPAH_IRQ_EN                             (0x00)
#define		ESPAH_IRQ_OUTBND_CMD_EN		0x1
#define		ESPAH_IRQ_OUTBND_STAT_EN		0x2
#define		ESPAH_IRQ_INBND_CMD_EN		0x4
#define		ESPAH_IRQ_INBND_STAT_EN		0x8
#define		ESPAH_IRQ_ALL_EN		(ESPAH_IRQ_OUTBND_CMD_EN|ESPAH_IRQ_OUTBND_STAT_EN|ESPAH_IRQ_INBND_CMD_EN|ESPAH_IRQ_INBND_STAT_EN)
#define		ESPAH_IRQ_GLBL_EN		(1UL<<31)
#define ESPAH_IRQ_STAT                           (0x04)
#define		ESPAH_STAT_OUTBND_CMD		0x1
#define		ESPAH_STAT_OUTBND_STAT		0x2
#define		ESPAH_STAT_INBND_CMD		0x4
#define		ESPAH_STAT_INBND_STAT		0x8

#define ESPAH_DMA_BURST_SZ                       (0x08)
#define ESPAH_IV_RND                             (0x10)
#define ESPAH_OUT_SRC_PTR                        (0x20)
#define ESPAH_OUT_DST_PTR                        (0x24)
#define ESPAH_OUT_OFFSET                         (0x28)
#define ESPAH_OUT_ID                             (0x2C)
#define ESPAH_OUT_SAI                            (0x30)
#define ESPAH_OUT_POP                            (0x38)
#define ESPAH_OUT_STAT                           (0x3C)
#define ESPAH_IN_SRC_PTR                         (0x40)
#define ESPAH_IN_DST_PTR                         (0x44)
#define ESPAH_IN_OFFSET                          (0x48)
#define ESPAH_IN_ID                              (0x4C)
#define ESPAH_IN_SAI                             (0x50)
#define ESPAH_IN_POP                             (0x58)
#define ESPAH_IN_STAT                            (0x5C)

typedef struct _t_clp30_espah_regs {
  V32 espah_irq_en;	// 0x00
  V32 espah_irq_stat;	// 0x04
  V32 espah_dma_burst_sz;      //  0x0008  maximum in words (8-256)
  U32 _rsv0;
  V32 espah_iv_rnd;            //  0x0010  value to be added to the core's IV
  U32 _rsv1;
  V32 espah_endian_ctrl;       //  0x0018
  U32 _rsv2;
  V32 espah_out_src_ptr;       //  0x0020
  V32 espah_out_dst_ptr;       //  0x0024
  V32 espah_out_offset;        //  0x0028  DDT offset to start of packet
  V32 espah_out_id;            //  0x002c  software tag
  V32 espah_out_sai;           //  0x0030
  U32 _rsvd3;
  V32 espah_out_pop;           //  0x0038
  V32 espah_out_stat;          //  0x003C
  V32 espah_in_src_ptr;        //  0x0040
  V32 espah_in_dst_ptr;        //  0x0044
  V32 espah_in_offset;         //  0x0048  DDT offset to start of packet
  V32 espah_in_id;             //  0x004C  software tag
  V32 espah_in_sai;            //  0x0050
  U32 _rsvd4;
  V32 espah_in_pop;            //  0x0058
  V32 espah_in_stat;           //  0x005C
  U32 _rsvd5[8];
  V32 espah_sa_cache_flush;    //  0x0080
  V32 espah_sa_cache_ready;    //  0x0084
} clp30_espah_regs;

#define BITMASK(pos,width)	(((1UL<<(width))-1) << (pos))
// IN_STAT and OUT_STAT
#define ESPAH_STAT_LEN            0
#define ESPAH_STAT_LEN_BITS      16
#define ESPAH_STAT_LEN_MASK      BITMASK(ESPAH_STAT_LEN, ESPAH_STAT_LEN_BITS)

#define ESPAH_STAT_ID            16
#define ESPAH_STAT_ID_BITS        8
#define ESPAH_STAT_ID_MASK       BITMASK(ESPAH_STAT_ID, ESPAH_STAT_ID_BITS)

#define ESPAH_STAT_RET           24
#define ESPAH_STAT_RET_BITS       4
#define ESPAH_STAT_RET_MASK      BITMASK(ESPAH_STAT_RET, ESPAH_STAT_RET_BITS)

#define ESPAH_STAT_FIFO_FULL      30
#define ESPAH_STAT_FIFO_FULL_MASK      (1UL<<30)
#define ESPAH_STAT_FIFO_EMPTY     31
#define ESPAH_STAT_FIFO_EMPTY_MASK     (1UL<<31)
/* IN_OFFSET and OUT_OFFSET */
#define ESPAH_OFFSET_SRC          0
#define ESPAH_OFFSET_DST          16


typedef struct _tESPAH_XFORM {
  V32 src_ptr;
  V32 dst_ptr;
  V32 offset;
  V32 id;
  V32 sai;
  V32 pop;
  V32 stat;
} ESPAH_XFORM, *PESPAH_XFORM;

/* Elliptic SA Cache mgmt */
#if defined(COMCERTO_2000_CONTROL)
#	define	SA_CACHE_FLUSH		 *((volatile u32*)((u32)pfe->ipsec_baseaddr+ ESPAH_BASE + 0x80))
#	define	SA_CACHE_FLUSH_RDY	 *((volatile u32*)((u32)pfe->ipsec_baseaddr+ ESPAH_BASE + 0x84))
#else
#	define	SA_CACHE_FLUSH		*((volatile U32 *)  0x9a000080)
#	define	SA_CACHE_FLUSH_RDY	*((volatile U32 *)  0x9a000084)
#endif
/* Elliptic TRNG
Note that it is part of SPAcc and likely be protected by spin-lock to share with linux */
#	define	TRNG_CNTL	*((volatile U32 *)  0x0E010000)
#	define	TRNG_IRQ_EN	*((volatile U32 *)  0x0E010004)
#	define	TRNG_IRQ_STAT	*((volatile U32 *)  0x0E010008)
#	define	TRNG_DATA0	*((volatile U32 *)  0x0E010010)
#	define	TRNG_DATA1	*((volatile U32 *)  0x0E010014)
#	define	TRNG_DATA2	*((volatile U32 *)  0x0E010018)
#	define	TRNG_DATA3	*((volatile U32 *)  0x0E01001C)



/* SA flags (oxffest 0x7e) bits  */
#define ESPAH_ENABLED			0x0001
#define ESPAH_SEQ_ROLL_ALLOWED		0x0002
#define ESPAH_TTL_ENABLE		0x0004	
#define ESPAH_TTL_TYPE			0x0008 /* 0:byte 1:time */
#define ESPAH_AH_MODE			0x0010 /* 0:ESP 1:AH */
#define ESPAH_ANTI_REPLAY_ENABLE 	0x0080
#define ESPAH_COFF_EN			0x0400 /* 1:Crypto offload enable */
#define ESPAH_COFF_MODE			0x0800 /* Crypto offload mode: 0: ECB cypher or raw hash, 1 CBC cypher or HMAC hash */
#define ESPAH_IPV6_ENABLE 		0x1000 /* 1:IPv6 SA */ 
#define ESPAH_DST_OP_MODE 		0x2000 /* IPv6 dest opt treatment EDN-0277 page 16 */ 
#define ESPAH_EXTENDED_SEQNUM   	0x4000


/* STAT codes that go into the STAT_RET_CODE  of a register */
#define ESPAH_STAT_OK          	0
#define ESPAH_STAT_BUSY       	1
#define ESPAH_STAT_SOFT_TTL    	2
#define ESPAH_STAT_HARD_TTL    	3
#define ESPAH_STAT_SA_INACTIVE 	4
#define ESPAH_STAT_REPLAY      	5
#define ESPAH_STAT_ICV_FAIL    	6
#define ESPAH_STAT_SEQ_ROLL    	7
#define ESPAH_STAT_MEM_ERROR   	8
#define ESPAH_STAT_VERS_ERROR  	9
#define ESPAH_STAT_PROT_ERROR  	10
#define ESPAH_STAT_PYLD_ERROR  	11
#define ESPAH_STAT_PAD_ERROR   	12
#define ESPAH_DUMMY_PKT   	13


/*
** Implementation limits
*/

// HW fifo size
#define ESPAH_MAX_FIFO 			32
// DDT allocation size
#define ELP_WQ_RING_SIZE		ESPAH_MAX_FIFO
// Need that much extra space in the egress ddt
#define 	ELP_MAX_ESPAH_OVERHEAD	128	// At least ESP + IV + max pad + 2 + 12

/* ELP_MAX_DDT_PER_PKT is a max number of DDTs, which may be required
** Worst case consumption is worse on FPP then on linux because data buffers may spawn 64K boundary.
** worst packet-to-ddt-mapping is ipv6 packet which crosses 64k boundary:
**  - sop-to-64K boundary
**  - 64k boundary -to -eop
**  - null terminator
*/
#define ELP_MAX_DDT_PER_PKT 3

#define ELP_DDT_ENTRY_SIZE sizeof(struct elp_ddtdesc)
#define ELP_DDT_NUMBER 	(ELP_WQ_RING_SIZE * ELP_MAX_DDT_PER_PKT)
#define ELP_SA_ARAM_OFFSET (4*ELP_DDT_NUMBER*ELP_DDT_ENTRY_SIZE) // 4 fifos (src,dst) x (in,out)
/*
 * Soft formats and layouts
 */

/*
** elp_wqentry describes the packet queued to elliptic engine.
** it is functional equivalent of src_ddtring plus dst_ddtring plus pe_entry on the linux side
** All the sizes below are an important consideration because rings map onto dedicated aram
*/
struct elp_wqentry {
	struct elp_ddtdesc src_ddt[ELP_MAX_DDT_PER_PKT];	// 24 bytes
	struct elp_ddtdesc dst_ddt[ELP_MAX_DDT_PER_PKT];	// 24 bytes
};

// Structure above is 48 bytes to avoid division we use macros to convert between addresses and priv.ipsec.wq_idx
// macros will work as long as structure size is multiple of 32.
#define wqe_addr2pi(base,this_addr) ((((U32) this_addr) - ((U32) base))/16)
#define wqe_pi2addr(base,index) (void*) ((((U32) index) * 16) + ((U32) base))
//
//#if ((sizeof(struct elp_wqentry)) / 32) !=3 )
//#	error " redefine wqe_addr2pi and wqe_pi2addr macros above"
//#endif

/*
** ELP Engine descriptor
*/
struct elp_packet_engine{
        U32     wq_avail;
        struct elp_wqentry                      *pe_wq_ring;            /* work ring */
        struct elp_wqentry                      *pe_wq_ringlast;        /* last entry in work ring */
        struct elp_wqentry                      *pe_wq_free;            /* next wq entry  to use */
 };


typedef struct tIPSec_hw_context {
	// Elliptic driver structure
	clp30_espah_regs *ELP_REGS;
  	U32 ARAM_base;
  	int 	elp_irqm;
	int	irq_en_flags;

  	struct elp_packet_engine in_pe;
  	struct elp_packet_engine out_pe;
  	U32 ARAM_sa_base;
  	U32 ARAM_sa_base_end;
//#if	defined(IPSEC_DEBUG) && (IPSEC_DEBUG)
#if 0
  U32 flush_create_count;
  U32 flush_enable_count;
  U32 flush_disable_count;
  U32 inbound_counters[16];
  U32 outbound_counters[16];
#endif
} IPSec_hw_context;

#ifdef CFG_IPSEC_DEBUG
typedef struct tIPsec_dbg_cntrs {
	u32 dbg_outbnd_rcvd;
	u32 dbg_outbnd_xmit;
	u32 dbg_outbnd_expt;
	u32 dbg_outbnd_drops;
	u32 dbg_inbnd_rcvd;
	u32 dbg_inbnd_xmit;
	u32 dbg_inbnd_expt;
	u32 dbg_inbnd_drops;
	u32 dbg_wa_drops;
}IPSec_dbg_cntrs;
#endif


static __inline U32 elp_read( IPSec_hw_context *pctx, U32 elp_reg_offset) {
    	return  *((volatile U32 *) ((U32)pctx->ELP_REGS + elp_reg_offset));
}
static __inline U32 elp_write( IPSec_hw_context *pctx, U32 elp_reg_offset, U32 val) {
    	*((volatile U32 *) ((U32)pctx->ELP_REGS + elp_reg_offset)) = val;
    	return  val;
}

#if defined(COMCERTO_2000_CLASS)
/* SA notifications */
void M_IPS_IN_process_packet(PMetadata mtd);
void M_IPS_OUT_process_packet(PMetadata mtd);
void M_IPsec_inbound_process_from_util(PMetadata mtd, lmem_trailer_t *trailer);
int prepare_ipsec_oubound_packet(PMetadata mtd);
#endif

//#if	defined(IPSEC_DEBUG) && (IPSEC_DEBUG)
//static unsigned int M_ipsec_ttl_check_data(PSAEntry sa, U16 retcode);
//#else
//static void M_ipsec_ttl_check_data(PSAEntry sa, U16 retcode);
//#endif


static __inline void elp_cache_wait(void) {
	while (SA_CACHE_FLUSH_RDY == 0)
		;
}


static __inline void hw_sa_cache_flush(PElliptic_SA elp_sa_ptr, int sa_cache_flush)
{
#if 0
	printk(KERN_INFO "%s: cache_flush_ready ..:addr:%x - val:%x\n",__func__, ((u32)pfe->ipsec_baseaddr+ ESPAH_BASE + 0x84), SA_CACHE_FLUSH_RDY);
#endif
	if (sa_cache_flush) {
                elp_cache_wait();
                SA_CACHE_FLUSH = (U32)elp_sa_ptr; /* Any value causes flush-all. We write SA address for debug purposes */
                elp_cache_wait();
        }
}


#if defined (COMCERTO_2000_UTIL)
/* Extern declarations required */
extern struct elp_wqentry  dmem_ddt ;
extern u8 upe_dmem_pkt[];
extern void util_ipsec_init();
extern TIMER_ENTRY sa_timer;
#ifdef CFG_STATS
extern U32 gFpStatFeatureBitMask;
#endif
#endif

#if defined (COMCERTO_2000_CLASS) || defined (COMCERTO_2000_UTIL)
#ifdef CFG_IPSEC_DEBUG
extern IPSec_dbg_cntrs g_ipsec_dbg_cntrs;
#define IPSEC_DEBUG_COUNTER_INCREMENT(counter) do { (g_ipsec_dbg_cntrs.dbg_##counter) += 1; } while(0)
#endif

/* While adding GRE-IPV6 over IPV6 tunnel header, overall bytes to add before sending to utilpe 
 * is coming up to 130 bytes(98(total GRE +IPSEC headers) + 24 (util_rx_hdr) + 8 (class_tx_hdr)).
 * But the allowed DMEM to grow is 128 - 16 = 112 bytes). So inorder to make it work,
 * copy the packet's initial 16 bytes to the end of packet's DDR location
 * (DDR address + 2K -128 ) and copy these 16 bytes again into the data before
 * encryption (use the util_rx_hdr space in DMEM).*/

#define PKT_CPY_DATA_LEN_FROM_START	16
#define PKT_CPY_OFFSET_FROM_END		128
#endif

extern IPSec_hw_context g_ipsec_hw_ctx;
#if defined (COMCERTO_2000_CONTROL)
int hw_sa_set_digest_key(PSAEntry sa, U16 key_bits, U8* key);
int hw_sa_set_cipher_ALG_AESCTR(PSAEntry sa, U16 key_bits, U8* key , U8* algo);
int hw_sa_set_cipher_key(PSAEntry sa, U8* key);
int hw_sa_set_lifetime(CommandIPSecSetLifetime *cmd, PSAEntry sa);
PElliptic_SA allocElpSA(dma_addr_t*);
void freeElpSA(PElliptic_SA psa, dma_addr_t);
int freeAramElpSA(PElliptic_SA psa);
void disableElpSA(PElliptic_SA psa);
#endif

#endif	/* _MODULE_IPSEC_H_ */
