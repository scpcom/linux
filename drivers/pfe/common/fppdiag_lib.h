/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
*/
#ifndef _FPPDIAG_LIB_H_
#define _FPPDIAG_LIB_H_

#ifdef CFG_DIAGS

#ifdef COMCERTO_2000

#define FPPDIAG_MAX_ARGS 4 /* 4 ought to be enough for anybody... */
typedef u32 FPPDIAG_ARG_TYPE; /* Type size must be a multiple of 32-bit */

#if !defined(COMCERTO_2000_CONTROL)
#define PAGE_SIZE					(1 << PAGE_SIZE_SHIFT)
#endif

#define PAGE_SIZE_SHIFT				12						/**< This must be set to match the Linux PAGE_SIZE (4kB) */
#define FPPDIAG_ENTRY_SHIFT			6
#define	FPPDIAG_ENTRY_SIZE			(1 << FPPDIAG_ENTRY_SHIFT)
#define FPPDIAG_ENTRIES_PER_PAGE 	(PAGE_SIZE / FPPDIAG_ENTRY_SIZE)
#define FPPDIAG_ENTRY_HEADER_SIZE	4
#define FPPDIAG_BUFFER_SIZE			(FPPDIAG_ENTRY_SIZE - 4) /* Account for the 32 bits of metadata at start of buffer */

#define	FPPDIAG_ENTRY_MASK_BITS		(PAGE_SIZE_SHIFT - FPPDIAG_ENTRY_SHIFT)
#define FPPDIAG_ENTRY_MASK			(( 1 << FPPDIAG_ENTRY_MASK_BITS ) - 1)

#define FPPDIAG_MAX_STRING_LEN		((FPPDIAG_BUFFER_SIZE-FPPDIAG_MAX_ARGS*sizeof(FPPDIAG_ARG_TYPE))& ~0x3) /* aligned on 32-bits to avoid unaligned accesses. */

#else
#define 	FPPDIAG_CTL_BASE_ADDR		0x0A000460

#define PAGE_SIZE_SHIFT				12						/**< This must be set to match the Linux PAGE_SIZE (4kB) */
#define PAGE_SIZE					(1 << PAGE_SIZE_SHIFT)
#define FPPDIAG_ENTRY_SHIFT			8
#define	FPPDIAG_ENTRY_SIZE			(1 << FPPDIAG_ENTRY_SHIFT)
#define FPPDIAG_ENTRIES_PER_PAGE 	(PAGE_SIZE / FPPDIAG_ENTRY_SIZE)

#define	FPPDIAG_ENTRY_MASK_BITS		(PAGE_SIZE_SHIFT - FPPDIAG_ENTRY_SHIFT)
#define FPPDIAG_ENTRY_MASK			(( 1 << FPPDIAG_ENTRY_MASK_BITS ) - 1)
#endif /* #ifdef COMCERTO_2000 */

typedef struct
{
        unsigned int read_index;        /* Read index into Ring buffer array */
        unsigned int write_index;       /* Write index into Ring buffer array */
} fppdiag_ctl_t;


struct fppdiag_config
{
        unsigned int *rng_baseaddr;      /**< Pointer to an array of page buffer addresses */
        unsigned int rng_size;          /**< Size of the page buffer array */
        unsigned short diag_ctl_flag;       /**< Global flag to maintain enable/disable diag*/
        unsigned short diag_log_flag;
        unsigned int diag_mod_flag;    /**< Module level flags to maintain diags */
        fppdiag_ctl_t *fppdiagctl;
}__attribute__((aligned(8)));

/* Flags for DIAG_SET_CTL_FLAGS */
#define FPPDIAG_CTL_ENABLE 	(1<<0)
#define FPPDIAG_CTL_FREERUN	(1<<1)

/* Flags for DIAG_SET_LOG_FLAGS */
#define FPPDIAG_LOG_INFO        (1<<0)
#define FPPDIAG_LOG_ERROR       (1<<1)
#define FPPDIAG_LOG_CRITICAL    (1<<2)
#define FPPDIAG_LOG_DEBUG       (1<<3)

/* Flags for DIAG_SET_MODULE_FLAGS */
#define FPPDIAG_MODULE_IPV4     (1<<0)
#define FPPDIAG_MODULE_IPV6     (1<<1)
#define FPPDIAG_MODULE_PPPOE    (1<<2)
#define FPPDIAG_MODULE_SEC      (1<<3)
#define FPPDIAG_MODULE_IGMP     (1<<4)
#define FPPDIAG_MODULE_VLAN     (1<<5)
#define FPPDIAG_MODULE_RX		(1<<6)
#define FPPDIAG_MODULE_TX		(1<<7)


typedef struct {
	u8 flags;
	u8 dummy1;
	u8 dummy2;
	u8 dummy3;
	u8 buffer[FPPDIAG_BUFFER_SIZE];
} fppdiag_entry_t;

/* Flags to be written at beginning of each entry */
#define FPPDIAG_PRINT_ENTRY	0
#define FPPDIAG_DUMP_ENTRY 	1
#define FPPDIAG_EXPT_ENTRY 	2

#define REGISTER_COUNT 20

#if !defined (COMCERTO_2000_CONTROL) && !defined (COMCERTO_2000_TMU)
extern struct fppdiag_config fppdiagconfig;

void fppdiag_init(void);

void fppdiag_print(unsigned short log_id, unsigned int mod_id,	const char* fmt, ...);

void fppdiag_dump(unsigned short log_id, unsigned int mod_id, void* src_addr, unsigned int src_size);

void fppdiag_exception_dump(U32* registers);

#define PRINTF(str, ...) do { static const char string[] __attribute__ ((section(".pfe_diags_str"))) __attribute__ ((aligned (4))) = str; fppdiag_print(1, 1, string, ##__VA_ARGS__); } while (0)
#define DUMP(src_addr, src_size) do { fppdiag_dump(1, 1, src_addr, src_size); } while (0)
#endif

#if defined(COMCERTO_2000_CONTROL)
int fppdiag_enable(u16 * p, u16 length);
int fppdiag_disable(void);
int fppdiag_update(u16* p, u16 length);
int fppdiag_dump_counters(void);
#endif

#else
#define PRINTF(str, ...) do { } while (0)
#define DUMP(src_addr, src_size) do { } while (0)

#endif /* ifdef CFG_DIAGS */
#endif /* ifndef _FPPDIAG_LIB_H_ */
