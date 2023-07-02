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

#include "module_ipsec.h"
#include "system.h"

#include <mach/hardware.h>

/*
 * Allocation control mask for SAs. One word describes 32 SAs
*/
#if	!defined(IPSEC_ARAM_MAX_BYTESIZE)
#	define	IPSEC_ARAM_MAX_BYTESIZE	(1024 * 28)
#endif

#define	NUM_ARAM_SADSC (((((IPSEC_ARAM_MAX_BYTESIZE - ELP_SA_ARAM_OFFSET) + ELP_SA_ALIGN-1)>>ELP_SA_ALIGN_LOG)+31)>>5)
static unsigned int aram_sa_amask[NUM_ARAM_SADSC];

IPSec_hw_context UTIL_DMEM_SH2(g_ipsec_hw_ctx)  __attribute__((aligned(8)));

int hw_sa_set_digest_key(PSAEntry sa, U16 key_bits, U8* key)
{
        struct _tElliptic_SA *elp_sa = sa->elp_sa;
        U32     key_len = (key_bits + 7) >>3;

	*(U32*)&elp_sa->auth_key0 =  ntohl(*(U32*)&key[0]);
	*(U32*)&elp_sa->auth_key1 =  ntohl(*(U32*)&key[4]);
	*(U32*)&elp_sa->auth_key2 =  ntohl(*(U32*)&key[8]);
	*(U32*)&elp_sa->auth_key3 =  ntohl(*(U32*)&key[12]);
	*(U32*)&elp_sa->auth_key4 =  ntohl(*(U32*)&key[16]);

	if (key_len > 20) {
		*(U32*)&elp_sa->ext_auth_key0 = ntohl(*(U32*)&key[20]);
		*(U32*)&elp_sa->ext_auth_key1 = ntohl(*(U32*)&key[24]);
		*(U32*)&elp_sa->ext_auth_key2 = ntohl(*(U32*)&key[28]);
	}
	
	return 0;
}

int hw_sa_set_cipher_ALG_AESCTR(PSAEntry sa, U16 key_bits, U8* key , U8* algo)
{
	struct _tElliptic_SA *elp_sa = sa->elp_sa;
	if (key_bits == 128+32) {
                        *algo = ELP_CIPHER_AES128_CTR;
                        *(U32*)&elp_sa->CTR_Nonce =  ntohl(*(U32*)&key[16]);
                }
                else if (key_bits == 192+32) {
                        *algo = ELP_CIPHER_AES192_CTR;
                        *(U32*)&elp_sa->CTR_Nonce =  ntohl(*(U32*)&key[24]);
                }
                else if (key_bits == 256+32) {
                        /* mat-20090325 - key structure needs to be expanded to carry 288 bits (see IPSEC_MAX_KEY_SIZE) */
                        *algo = ELP_CIPHER_AES256_CTR;
                        *(U32*)&elp_sa->CTR_Nonce =  ntohl(*(U32*)&key[32]);
                }
                else
                        return -1;

		sa->blocksz = 4;
		return 0;
}
int hw_sa_set_cipher_key(PSAEntry sa, U8* key)
{

	struct _tElliptic_SA *elp_sa = sa->elp_sa;

        *(U32*)&elp_sa->cipher0 =  ntohl(*(U32*)&key[0]);
        *(U32*)&elp_sa->cipher1 =  ntohl(*(U32*)&key[4]);
        *(U32*)&elp_sa->cipher2 =  ntohl(*(U32*)&key[8]);
        *(U32*)&elp_sa->cipher3 =  ntohl(*(U32*)&key[12]);
        *(U32*)&elp_sa->cipher4 =  ntohl(*(U32*)&key[16]);
        *(U32*)&elp_sa->cipher5 =  ntohl(*(U32*)&key[20]);
        *(U32*)&elp_sa->cipher6 =  ntohl(*(U32*)&key[24]);
        *(U32*)&elp_sa->cipher7 =  ntohl(*(U32*)&key[28]);
	
	return 0;

}

int hw_sa_set_lifetime(CommandIPSecSetLifetime *cmd, PSAEntry sa)
{
	if (sa->lft_conf.soft_byte_limit) {
		sa->elp_sa->soft_ttl_hi = (cmd->soft_time.bytes[1]);
		sa->elp_sa->soft_ttl_lo = (cmd->soft_time.bytes[0]);
        }
        if (sa->lft_conf.hard_byte_limit) {
                sa->elp_sa->hard_ttl_hi = (cmd->hard_time.bytes[1]);
                sa->elp_sa->hard_ttl_lo = (cmd->hard_time.bytes[0]);
        }
        if ((sa->lft_conf.soft_byte_limit) || (sa->lft_conf.hard_byte_limit))
	{
#ifdef CONTROL_IPSEC_DEBUG
		printk (KERN_INFO "hw_set_lifetime:bytes:%llu - %llu\n",sa->lft_conf.soft_byte_limit, sa->lft_conf.hard_byte_limit);
#endif
                sa->elp_sa->flags |= ESPAH_TTL_ENABLE;
	}

        consistent_elpctl(sa->elp_sa,0);
	return 0;
}

static int isAramElpSA(void *p) {
  // Checks if address of elliptic sa is within aram range
  // Returns 1 if it is, 0 if it is not
  IPSec_hw_context *pipsc = &gIpSecHWCtx;
  if (pipsc->ARAM_sa_base) {
    if ((((U32) p ) >= pipsc->ARAM_sa_base ) &&
	(((U32) p ) <= pipsc->ARAM_sa_base_end))
      return 1;
  }
  return 0;
}


static PElliptic_SA allocAramElpSA(void) {
  // Attempt to allocate elliptic SA from aram
  // returns NULL if no SA's are available
  // and DRAM allocation is needed
  U32 idx,widx;
  IPSec_hw_context *pipsc = &gIpSecHWCtx;

  for(widx=0;widx<NUM_ARAM_SADSC;widx++) {
      if (aram_sa_amask[widx]) {
	  __asm__ (
	      //"clz idx, aram_sa_amask[widx]"
		"clz %0, %1" : "=r"(idx) : "r"(aram_sa_amask[widx])
	  );
	  aram_sa_amask[widx] &= 0xffffffffUL ^ (0x80000000UL >> idx);
	  return (PElliptic_SA) (pipsc->ARAM_sa_base + ((idx + widx*32)* ELP_SA_ALIGN));
      }
  }

  return NULL;
}


int freeAramElpSA(PElliptic_SA psa)
{
  // Attempt to free elliptic SA to aram
  // returns 1 if SA was freed and 0 if it is non-aram SA
  // and freeing needs to be retried.
  U32 idx,widx;
  IPSec_hw_context *pipsc = &gIpSecHWCtx;

  if (isAramElpSA(psa)) {
    idx = (((U32) psa) -  pipsc->ARAM_sa_base) >>ELP_SA_ALIGN_LOG ;
    // Split into bit index and wordindex
    widx = idx >> 5;
    idx &= 0x1f;
    aram_sa_amask[widx] |= (0x80000000UL >> idx);
    return 1;
  }
  return 0;
}


PElliptic_SA allocElpSA(dma_addr_t* dma_addr) {
	// Allocate Hardware SA descriptor.
	// Try private aram, then DRAM.
	PElliptic_SA psa;
	void *p;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

	if ((psa = allocAramElpSA()) != NULL )
	{
		*dma_addr = virt_to_phys_iram((void*)psa);
		return psa;
	}
	// No aram SAs - try dram
	// We do not consider aram heap to avid wasting space
	// Note that buddy allocator will return 256-byte aligned pointer
	/* modify this to get it from DDR with alignment*/
	p = dma_pool_alloc (ctrl->dma_pool, GFP_ATOMIC , dma_addr);
	return (PElliptic_SA) p;
}

void disableElpSA(PElliptic_SA psa)
{
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "Disabling ELP SA \n");
#endif
	if (psa == NULL)
		return;
	psa->flags = 0;	// Clear ESPAH_ENABLED
	consistent_elpctl(psa,1);
}

void freeElpSA(PElliptic_SA psa, dma_addr_t dma_addr) {
	//U32 tmp;
	//void *p;
	struct pfe_ctrl *ctrl = &pfe->ctrl;

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "Freeing ELP SA \n");
#endif
	if ( freeAramElpSA(psa))
		return;

	 dma_pool_free(ctrl->dma_pool, psa, dma_addr);
#if 0
	tmp  = (U32) psa;

	p = *(void **) ( tmp+ELP_SA_ALIGN);
	Heap_Free(p);
#endif
}


static U32 ipsec_init_pe(U32 memaddr, struct elp_packet_engine *ppe)
{

	ppe->wq_avail =  ELP_WQ_RING_SIZE;
	ppe->pe_wq_ring = (struct elp_wqentry*) memaddr;
	ppe->pe_wq_ringlast = ppe->pe_wq_ring + ELP_WQ_RING_SIZE-1;
	ppe->pe_wq_free =  ppe->pe_wq_ring;

	memset((void*) ppe->pe_wq_ring, 0, ELP_WQ_RING_SIZE * sizeof(struct elp_wqentry));
	memaddr += sizeof(struct elp_wqentry) * ELP_WQ_RING_SIZE;

	return memaddr;
}


static void elp_start_device(IPSec_hw_context *sc)
{

#if 0
	// enable ipsec clocks in case ACP power management disabled them.
	*(volatile U32 *)CLKCORE_CLK_PWR_DWN &= ~(IPSEC_AHBCLK_PD | IPSEC2_AHBCLK_PD | IPSEC_CORECLK_PD);
#endif


#if 0
	/* TODO Need to check if these are exactly needed */
	/* This is already done in barebox */
	writel(CLKCORE_IPSEC_CLK_CNTRL, 0x3);
	writel(CLKCORE_IPSEC_CLK_DIV_CNTRL, 0x2);
#endif
	/* Reset the IPSEC block at the time of initialization */
	/* This is required when we stop and start the driver */

#define IPSEC_EAPE_AXI_RESET_BIT	(1<<1)
	/* Put the IPSEC EAPE (AXI clock domain) device in reset state */
	writel(readl(APB_VADDR(AXI_RESET_1)) | IPSEC_EAPE_AXI_RESET_BIT, APB_VADDR(AXI_RESET_1));
	// c2000_block_reset(COMPONENT_AXI_IPSEC_EAPE, 1);
	mdelay(1);
	/* Put the IPSEC EAPE devices in Out-Of-Reset state */
	writel(readl(APB_VADDR(AXI_RESET_1)) & ~IPSEC_EAPE_AXI_RESET_BIT, APB_VADDR(AXI_RESET_1));
	// c2000_block_reset(COMPONENT_AXI_IPSEC_EAPE, 0);

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s --- Started \n", __func__);
#endif

#if	defined(ESPAH_USE_TRNG)
	// de-assert ipsec reset
	*(volatile U32 *)(CLKCORE_BLK_RESET)  |= IPSEC_RST_N;
	// Enable random number generator
	while ( TRNG_CNTL & 0x80000000UL)
		;
	TRNG_CNTL =  0x80000000UL;
	while ( TRNG_CNTL & 0x80000000UL)
		;
	// Make first request
	TRNG_CNTL = 1;
	int i;
	for(i=0;i<48;) {
		while ( TRNG_CNTL & 0x80000000UL)
			;
		sc->ELP_REGS->ESPAH_IV_RND = TNG_DATA0;
		sc->ELP_REGS->ESPAH_IV_RND = TNG_DATA1;
		sc->ELP_REGS->ESPAH_IV_RND = TNG_DATA2;
		sc->ELP_REGS->ESPAH_IV_RND = TNG_DATA3;
		TRNG_CNTL = 1;
		i+=4;

	}
#elif	defined(ESPAH_USE_PRNG)
#if 0
	*(volatile U32 *)(CLKCORE_BLK_RESET)  |=  ( IPSEC_RST_N | PRNG_RST_N);
	{
		U32 i,j;
		for(i=0;i<48;i++) {
			j= *((V32*) CLKCORE_PRNG_STATUS);
			j = j<<16;
			j |= *((V32*) CLKCORE_PRNG_STATUS);
		sc->ELP_REGS->espah_iv_rnd = j;
	    }
	}
#endif
	/* use kernel random number generator to get the random number */
	{
            U32 i,j;
            for(i=0;i<48;i++) {
                get_random_bytes(&j, 4);
                //sc->ELP_REGS->espah_iv_rnd = j;
#ifdef CONTROL_IPSEC_DEBUG
		printk(KERN_INFO "addr: %x random: %x\n", &sc->ELP_REGS->espah_iv_rnd, j);
#endif
		writel(j, &sc->ELP_REGS->espah_iv_rnd);
#ifdef CONTROL_IPSEC_DEBUG
		printk(KERN_INFO "value :%x - %x\n", sc->ELP_REGS->espah_iv_rnd, readl(&sc->ELP_REGS->espah_iv_rnd));
#endif
            }
        }

#endif
	// All the offsets are coded through ddt.
	// Set hw offsets to 0
	//sc->ELP_REGS->espah_out_offset =  0;
	writel(0, &sc->ELP_REGS->espah_out_offset);
	//sc->ELP_REGS->espah_in_offset =  0;
	writel(0, &sc->ELP_REGS->espah_in_offset);
	// Acknowledge all interrupts
	//sc->ELP_REGS->espah_irq_stat  =  ESPAH_STAT_OUTBND_CMD|ESPAH_STAT_OUTBND_STAT|ESPAH_STAT_INBND_CMD|ESPAH_STAT_INBND_STAT;

	writel(ESPAH_STAT_OUTBND_CMD|ESPAH_STAT_OUTBND_STAT|ESPAH_STAT_INBND_CMD|ESPAH_STAT_INBND_STAT, &sc->ELP_REGS->espah_irq_stat);
#if 0
	HAL_clear_interrupt(IRQM_IPSEC_EPEA);
#if	!defined(IPSEC_POLL) || (!IPSEC_POLL)
	HAL_arm1_fiq_enable(IRQM_IPSEC_EPEA);
	// enable interrupts
	sc->irq_en_flags = ESPAH_IRQ_OUTBND_STAT_EN | ESPAH_IRQ_INBND_STAT_EN|ESPAH_IRQ_GLBL_EN;
	sc->ELP_REGS->espah_irq_en =  sc->irq_en_flags;
#endif
#endif
}

int ipsec_send_ctx_utilpe(IPSec_hw_context* sc);

void ipsec_common_hard_init(IPSec_hw_context *sc)
{
	// Hardware init - done once
	// elliptic is known to be owned by this arm.
	U32 aram_addr;
	U32 i,j;
	int m;
#if 0
	U32 IPSEC_ARAM_BYTESIZE = ((unsigned int) Image$$aram_ipsec$$Length + (unsigned int) Image$$aram_ipsec$$ZI$$Length);
#else
	U32 IPSEC_ARAM_BYTESIZE =  IPSEC_IRAM_SIZE;
#endif

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s .. Started..\n", __func__);
#endif
	sc->ELP_REGS = (clp30_espah_regs *)((u32)pfe->ipsec_baseaddr + ESPAH_BASE);
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s ipsec_baseaddr:%x - espah_base:%x\n", __func__,  (u32)pfe->ipsec_baseaddr, (u32)((u32)pfe->ipsec_baseaddr + ESPAH_BASE));
#endif
	//sc->ARAM_base = (U32) ipsec_aram_storage;
	sc->ARAM_base = (U32) pfe->iram_baseaddr + IPSEC_IRAM_BASEADDR;
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s iram_baseaddr:%x - aram_base:%x\n", __func__,  (u32)pfe->iram_baseaddr, (u32)sc->ARAM_base);
#endif
#if 0
	/* TODO */
	sc->elp_irqm = IRQM_IPSEC_EPEA;
#endif

	aram_addr=ipsec_init_pe(sc->ARAM_base, &sc->in_pe);
	aram_addr=ipsec_init_pe(aram_addr, &sc->out_pe);
	sc->ARAM_sa_base = (aram_addr+ELP_SA_ALIGN-1) & ~(ELP_SA_ALIGN-1);
	sc->ARAM_sa_base_end = sc->ARAM_sa_base + IPSEC_ARAM_BYTESIZE - 1;
	// Use remaining space to allocate SA's in aram
	m = IPSEC_ARAM_BYTESIZE - (sc->ARAM_sa_base -  sc->ARAM_base);
	for(j=0;j<NUM_ARAM_SADSC;j++) {
		if (m >= 32 * ELP_SA_ALIGN) {
			aram_sa_amask[j] = 0xffffffffUL;
			m -= 32*ELP_SA_ALIGN;
		} else if (m >= ELP_SA_ALIGN) {
			aram_sa_amask[j] = 0;
			i = 0x80000000UL;
			do {
				aram_sa_amask[j] |= i;
				i = i>>1;
				m -= ELP_SA_ALIGN;
			} while (m>0) ; // && (i) not needed because m < 32 * SA_SIZE
		} else {
			aram_sa_amask[j] = 0;
		}
	}
	elp_start_device(sc);

	/* Send the hardware context to utilpe */
	ipsec_send_ctx_utilpe(sc);
}


/** Send the ipsec context to utilpe.
 *  This function updates the ipsec context with allocated ARAM addresses to 
 *  utilpe.
 * @param      sc    	pointer to the global context structure.
 *
 */

int ipsec_send_ctx_utilpe(IPSec_hw_context* sc)
{

	IPSec_hw_context* hw_ctx = &util_g_ipsec_hw_ctx;
	//struct pfe_ctrl *ctrl = &pfe->ctrl;

#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s --- Started \n", __func__);
	printk(KERN_INFO "ELP_REGS : %x\n",  (u32)sc->ELP_REGS);
	printk(KERN_INFO "Aram_base : %x\n",  (u32)sc->ARAM_base);
	printk(KERN_INFO "irq_en_flags : %x\n",  sc->irq_en_flags);
	printk(KERN_INFO "in_pe:wq_avail : %d- ring:%x ringlast:%x free:%x\n",  sc->in_pe.wq_avail, (u32)sc->in_pe.pe_wq_ring, (u32)sc->in_pe.pe_wq_ringlast, (u32)sc->in_pe.pe_wq_free);
	printk(KERN_INFO "out_pe:wq_avail : %d- ring:%x ringlast:%x free:%x\n",  sc->out_pe.wq_avail, (u32)sc->out_pe.pe_wq_ring, (u32)sc->out_pe.pe_wq_ringlast, (u32)sc->out_pe.pe_wq_free);
	printk(KERN_INFO "Aram_SA_base : %x - end:%x\n",  (u32)sc->ARAM_sa_base, (u32)sc->ARAM_sa_base_end);
#endif
	/* Convert the structure elements to big endian */

	hw_ctx->ELP_REGS = (clp30_espah_regs *)cpu_to_be32((u32)virt_to_phys_ipsec_axi((void*)sc->ELP_REGS));
	hw_ctx->ARAM_base = cpu_to_be32(virt_to_phys_iram((void*)sc->ARAM_base));
	hw_ctx->irq_en_flags = cpu_to_be32(sc->irq_en_flags);
	hw_ctx->in_pe.wq_avail = cpu_to_be32(sc->in_pe.wq_avail);
	hw_ctx->in_pe.pe_wq_ring =  (struct elp_wqentry*)cpu_to_be32(virt_to_phys_iram((void*)sc->in_pe.pe_wq_ring));
	hw_ctx->in_pe.pe_wq_ringlast = (struct elp_wqentry*)cpu_to_be32(virt_to_phys_iram((void*)sc->in_pe.pe_wq_ringlast));
	hw_ctx->in_pe.pe_wq_free = (struct elp_wqentry*)cpu_to_be32(virt_to_phys_iram((void*)sc->in_pe.pe_wq_free));
	hw_ctx->out_pe.wq_avail = cpu_to_be32(sc->out_pe.wq_avail);
	hw_ctx->out_pe.pe_wq_ring = (struct elp_wqentry*)cpu_to_be32(virt_to_phys_iram((void*)sc->out_pe.pe_wq_ring));
	hw_ctx->out_pe.pe_wq_ringlast = (struct elp_wqentry*)cpu_to_be32(virt_to_phys_iram((void*)sc->out_pe.pe_wq_ringlast));
	hw_ctx->out_pe.pe_wq_free = (struct elp_wqentry*)cpu_to_be32(virt_to_phys_iram((void*)sc->out_pe.pe_wq_free));
	hw_ctx->ARAM_sa_base = cpu_to_be32(virt_to_phys_iram((void*)sc->ARAM_sa_base));
	hw_ctx->ARAM_sa_base_end = cpu_to_be32(virt_to_phys_iram((void*)sc->ARAM_sa_base_end));


	pe_dmem_memcpy_to32(UTIL_ID, virt_to_util_dmem(&util_g_ipsec_hw_ctx), &util_g_ipsec_hw_ctx, sizeof(IPSec_hw_context));

	return NO_ERR;
}


