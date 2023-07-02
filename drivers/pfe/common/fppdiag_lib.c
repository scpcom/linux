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
#include "fpp_globals.h"
#include "system.h"
#include "fppdiag_lib.h"

extern U32 exception_dump;

#if defined(CFG_DIAGS) && !defined(COMCERTO_2000_TMU) && !defined(COMCERTO_2000_SBL)

#if !defined(COMCERTO_2000)
extern int snprintf(char *str, int size, const char *format, ...);
extern int vsnprintf(char *str, int size, const char *format, va_list ap);
#endif


#define FPPDIAG_DUMP_COUNTER(COUNTER_NAME ,COUNTER)  fppdiag_print(FPPDIAG_LOG_DEBUG, FPPDIAG_MODULE_RX | FPPDIAG_MODULE_TX, \
                                                                                   COUNTER_NAME "  => %u", COUNTER_GET(COUNTER));

#if defined(COMCERTO_2000_CONTROL)
int fppdiag_enable(u16 * p, u16 length)
{
	return 0;
}
int fppdiag_disable(void)
{
	return 0;
}
int fppdiag_update(u16* p, u16 length)
{
	return 0;
}
int fppdiag_dump_counters(void)
{
	return 0;
}
#else

#if !defined(COMCERTO_2000)
int fppdiag_enable(U16 * p, U16 length)
{
	struct fppdiag_config Fppdiagcmd;

    SFL_memcpy((void*)&Fppdiagcmd, (void*)p,  length);

	fppdiagconfig.rng_baseaddr	= Fppdiagcmd.rng_baseaddr;
    fppdiagconfig.rng_size		= Fppdiagcmd.rng_size;
    fppdiagconfig.diag_ctl_flag	= Fppdiagcmd.diag_log_flag;
    fppdiagconfig.diag_log_flag	= Fppdiagcmd.diag_log_flag;
    fppdiagconfig.diag_mod_flag	= Fppdiagcmd.diag_mod_flag;

	return 0;
}


int fppdiag_disable(void)
{
	void * pageaddr;       
	int i;
	/* Invalidate each page entry from  the ring array and each induvidual page entry,
	  * to stay safe of any possible packet corruptions due to stale cache.
	  */
	if(fppdiagconfig.rng_size)  {
		for(i = 0; i < fppdiagconfig.rng_size; i++)	{
			pageaddr = (void*)fppdiagconfig.rng_baseaddr[i];
		  	L1_dc_flush(pageaddr, ((char*)pageaddr + FPPDIAG_PAGE_SIZE  - 1));
  		}
		L1_dc_flush((char*)fppdiagconfig.rng_baseaddr,  
					((char*)fppdiagconfig.rng_baseaddr +( sizeof(unsigned int)  * fppdiagconfig.rng_size) - 1 ));
	}
	fppdiagconfig.diag_ctl_flag = 0;
	fppdiagconfig.rng_baseaddr = 0;
    fppdiagconfig.rng_size = 0;

	return 0;
}


int fppdiag_update(U16* p, U16 length)
{
	struct fppdiag_config Fppdiagcmd;

	SFL_memcpy((void*)&Fppdiagcmd, (void*)p,  length);

    fppdiagconfig.diag_ctl_flag	= Fppdiagcmd.diag_ctl_flag;
	fppdiagconfig.diag_log_flag	= Fppdiagcmd.diag_log_flag;
    fppdiagconfig.diag_mod_flag	= Fppdiagcmd.diag_mod_flag;
	
	return 0;
}

void fppdiag_init(void)
{
	fppdiagconfig.fppdiagctl = (fppdiag_ctl_t *)FPPDIAG_CTL_BASE_ADDR;
	memset(fppdiagconfig.fppdiagctl,0, sizeof(fppdiag_ctl_t));
}

static inline void fppdiag_lowlevel_print(unsigned char* buffer, char* fmt, va_list argptr)
{
	int len = 0, len1 = 0;

	//len = snprintf(buffer, FPPDIAG_ENTRY_SIZE,"%d:%d", log_id, mod_id);
	len   = snprintf((char*)buffer, FPPDIAG_ENTRY_SIZE,"%d :: ", ct_timer );
	len1 = vsnprintf((char*)buffer+len , (FPPDIAG_ENTRY_SIZE - len), (const char*) fmt, argptr);

	L1_dc_clean((void*)buffer , (void*)(buffer + len + len1 - 1));
}

static inline void fppdiag_notify_host(unsigned int write_index)
{
	fppdiagconfig.fppdiagctl->write_index = write_index;

	if (IRQ_FPPDIAG < 32)
			HAL_generate_int(IRQM_FPPDIAG);
		else
			HAL_generate_int_1(IRQM_FPPDIAG);
}


#else

#if defined(CFG_STANDALONE)
static U32 rng_base;

void fppdiag_init(void)
{
	U32 pe_num;

	pe_num = esi_get_mpid();
	fppdiagconfig.fppdiagctl = (fppdiag_ctl_t *)(FPPDIAG_CTL_BASE_ADDR + pe_num*sizeof(fppdiag_ctl_t));
	memset(fppdiagconfig.fppdiagctl,0, sizeof(fppdiag_ctl_t));

	/* For now, hard-code FPP diags config values here until a proper control path is available.
	 */
	fppdiagconfig.diag_ctl_flag = FPPDIAG_CTL_ENABLE | FPPDIAG_CTL_FREERUN;
	fppdiagconfig.diag_log_flag = 0xff;
	fppdiagconfig.diag_mod_flag = 0xff;
	rng_base = FPPDIAG_PAGE_BASE_ADDR+pe_num*PAGE_SIZE;
	fppdiagconfig.rng_baseaddr = &rng_base;
	fppdiagconfig.rng_size = 1;
	memset((U32 *)rng_base, 0, PAGE_SIZE);

}
#endif

/** Copies a string and variable arguments to the specified debug buffer.
 * C2k-specific, simply copies input data instead of using vsprintf functions to reduce code complexity and size.
 * String formatting will then be done on the host side.
 * Assumes the arguments of va_list are all of type FPPDIAG_ARG_TYPE and will only copy the first FPPDIAG_MAX_ARGS of the list.
 * To save space in PFE DMEMs, the string is not copied into the message, but instead the address of the string is passed.
 * The strings are stored in a special section (see the PRINTF macro) so that the host code can then retrieve the string based
 * on the address passed inside the message.
 * @param[out] buffer	pointer to the buffer data should be copied to. Must be 32-bit aligned (should be guaranteed by the organization of diag buffers.).
 * @param[in] fmt		pointer to the character string to be printed out. Must be 32-bit aligned.
 * @param[in] argptr	va_list (opaque type) of arguments to be inserted into the string.
 */
static inline void fppdiag_lowlevel_print(unsigned char* buffer, const char* fmt, va_list argptr)
{
	FPPDIAG_ARG_TYPE *arg;

	*(u32 *)buffer = (u32) fmt;

	arg = (FPPDIAG_ARG_TYPE *)(buffer + FPPDIAG_MAX_STRING_LEN);

	*arg = va_arg(argptr, FPPDIAG_ARG_TYPE);
	arg++;
	*arg = va_arg(argptr, FPPDIAG_ARG_TYPE);
	arg++;
	*arg = va_arg(argptr, FPPDIAG_ARG_TYPE);
	arg++;
	*arg = va_arg(argptr, FPPDIAG_ARG_TYPE);

}

static inline void fppdiag_notify_host(unsigned int write_index)
{
	/* On C2k, Host will simply poll the pointers and read entries when read and write pointers don't match. */
	fppdiagconfig.fppdiagctl->write_index = write_index;
}
#endif


int fppdiag_dump_counters(void)
{
#if !defined(COMCERTO_2000)
	FPPDIAG_DUMP_COUNTER("packets_received on port 0	",packets_received[0]);
	FPPDIAG_DUMP_COUNTER("packets_received on port 1	",packets_received[1]);
	FPPDIAG_DUMP_COUNTER("packets_received at expt on port 0",packets_received_expt[0]);
	FPPDIAG_DUMP_COUNTER("packets_received at expt on port 1",packets_received_expt[1]);
	FPPDIAG_DUMP_COUNTER("packets_transmitted on port 0",packets_transmitted[0]);
	FPPDIAG_DUMP_COUNTER("packets_transmitted on port 1",packets_transmitted[1]);
	FPPDIAG_DUMP_COUNTER("packets_transmitted through expt on port 0",packets_transmitted_expt[0]);
	FPPDIAG_DUMP_COUNTER("packets_transmitted through expt on port 0",packets_transmitted_expt[1]);
	FPPDIAG_DUMP_COUNTER("packets_dropped at qm on port 0",packets_dropped_qm[0]);
	FPPDIAG_DUMP_COUNTER("packets_dropped at qm on port 1",packets_dropped_qm[1]);
	FPPDIAG_DUMP_COUNTER("packets_dropped at tx on port 0	",packets_dropped_tx[0]);
	FPPDIAG_DUMP_COUNTER("packets_dropped at tx on port 1	",packets_dropped_tx[1]);
	FPPDIAG_DUMP_COUNTER("packets_dropped on rxerror on port 0",packets_dropped_rxerror[0]);
	FPPDIAG_DUMP_COUNTER("packets_dropped on rxerror on port 1",packets_dropped_rxerror[1]);
	FPPDIAG_DUMP_COUNTER("packets_dropped at ingress congestion control on port 0",packets_dropped_icc[0]);
	FPPDIAG_DUMP_COUNTER("packets_dropped at ingress congestion control on port 1",packets_dropped_icc[1]);
	FPPDIAG_DUMP_COUNTER("packets_dropped at expt on port 0",packets_dropped_expt[0]);
	FPPDIAG_DUMP_COUNTER("packets_dropped ar expt on port 1",packets_dropped_expt[1]);
	FPPDIAG_DUMP_COUNTER("packets buffer freed and moved to poolA",packets_dropped_poolA);
	FPPDIAG_DUMP_COUNTER("packets buffer freed and moved to poolB",packets_dropped_poolB);
	FPPDIAG_DUMP_COUNTER("rx no metadata available ",rx_no_metadata);
	FPPDIAG_DUMP_COUNTER("packets_dropped at expt invalid port	",packets_dropped_expt_invalid_port);
	FPPDIAG_DUMP_COUNTER("packets_dropped invalid ipv4 cksum",packets_dropped_ipv4_cksum);
	FPPDIAG_DUMP_COUNTER("packets_dropped during ipv4 fragmentation/re-assembly",packets_dropped_ipv4_fragmenter);
	FPPDIAG_DUMP_COUNTER("packets_dropped during ipv6 fragmentation/re-assembly",packets_dropped_ipv6_fragmenter);
	FPPDIAG_DUMP_COUNTER("packets_dropped due to channel full",packets_dropped_channel_full);
	FPPDIAG_DUMP_COUNTER("packets_dropped_socket_not_bound1",packets_dropped_socket_not_bound1);
	FPPDIAG_DUMP_COUNTER("packets_dropped_socket_not_bound2",packets_dropped_socket_not_bound2);
	FPPDIAG_DUMP_COUNTER("packets_dropped_socket_not_bound3",packets_dropped_socket_not_bound3);
	FPPDIAG_DUMP_COUNTER("packets_dropped no_route found",packets_dropped_socket_no_route);
	FPPDIAG_DUMP_COUNTER("packets_dropped rtp_relay no flow found",packets_dropped_rtp_relay_no_flow);
	FPPDIAG_DUMP_COUNTER("packets_dropped rtp_relay no socket found",packets_dropped_rtp_relay_no_socket);
	FPPDIAG_DUMP_COUNTER("packets_dropped rtp_relay discard",packets_dropped_rtp_relay_discard);
	FPPDIAG_DUMP_COUNTER("packets_dropped rtp_relay misc	",packets_dropped_rtp_relay_misc);
	FPPDIAG_DUMP_COUNTER("packets_dropped ipsec inbound",packets_dropped_ipsec_inbound);
	FPPDIAG_DUMP_COUNTER("packets_dropped ipsec outbound",packets_dropped_ipsec_outbound);
	FPPDIAG_DUMP_COUNTER("packets_dropped at ipsec rate limiter	",packets_dropped_ipsec_rate_limiter);
	FPPDIAG_DUMP_COUNTER("packets_dropped natt",packets_dropped_natt);
	FPPDIAG_DUMP_COUNTER("packets_dropped natpt",packets_dropped_natpt);
	FPPDIAG_DUMP_COUNTER("packets_dropped mc4",packets_dropped_mc4);
	FPPDIAG_DUMP_COUNTER("packets_dropped mc6",packets_dropped_mc6);
	FPPDIAG_DUMP_COUNTER("fragments dropped",packets_dropped_fragments);
	FPPDIAG_DUMP_COUNTER("packets_dropped at rx expt ipsec",packets_dropped_expt_rx_ipsec);
#endif
	return 0;
}

static inline fppdiag_entry_t * fppdiag_get_entry(unsigned short log_id, unsigned int mod_id, unsigned int *pwrite_index)
{
	U32 pageaddr;
	unsigned int total_size, read_index, write_index;


	if (!fppdiagconfig.diag_ctl_flag)
		return NULL;
	if (!fppdiagconfig.rng_baseaddr)
		return NULL;
	if((fppdiagconfig.diag_log_flag & log_id) == 0)
		return NULL;
	if((fppdiagconfig.diag_mod_flag & mod_id) == 0)
		return NULL;

	write_index = fppdiagconfig.fppdiagctl->write_index;
	read_index = fppdiagconfig.fppdiagctl->read_index;
	write_index++;

	// Ring full condition
	if (!(fppdiagconfig.diag_ctl_flag & FPPDIAG_CTL_FREERUN) && (write_index == read_index))
		return NULL;

	total_size = fppdiagconfig.rng_size * FPPDIAG_ENTRIES_PER_PAGE;
	if (write_index == total_size)
		write_index = 0;
	
	pageaddr = fppdiagconfig.rng_baseaddr[write_index/FPPDIAG_ENTRIES_PER_PAGE];
	if (!pageaddr)
		return NULL;
	
	*pwrite_index = write_index;
	return (fppdiag_entry_t *)(pageaddr + ((write_index & FPPDIAG_ENTRY_MASK) << FPPDIAG_ENTRY_SHIFT));
}

void fppdiag_print(unsigned short log_id, unsigned int mod_id, const char* fmt, ...)
{
	fppdiag_entry_t *entry;
	unsigned int write_index;
	va_list argptr;

	entry = fppdiag_get_entry(log_id,mod_id, &write_index);
	if (!entry)
		return;

	entry->flags = FPPDIAG_PRINT_ENTRY;
	va_start(argptr,fmt);
	fppdiag_lowlevel_print(entry->buffer, fmt, argptr);
	va_end(argptr);

	fppdiag_notify_host(write_index);

	return;
}

void fppdiag_dump(unsigned short log_id, unsigned int mod_id, void* src_addr, unsigned int src_size)
{
	fppdiag_entry_t *entry;
	unsigned int write_index;
	unsigned int size = min(src_size, FPPDIAG_BUFFER_SIZE);

	entry = fppdiag_get_entry(log_id,mod_id, &write_index);
	if (!entry)
		return;

	entry->flags = FPPDIAG_DUMP_ENTRY;
	/* TODO Use efet_memcpy when possible */
	memcpy(entry->buffer, src_addr, size);
	
	fppdiag_notify_host(write_index);
	
	return;
}

/** Dump CPU registers to FPP diags buffers.
 *
 * @param[in] registers	pointer to memory area holding a copy of the register values.
 *
 */
void fppdiag_exception_dump(U32* registers)
{
	fppdiag_entry_t *entry;
	unsigned int write_index;
	unsigned int size = min(REGISTER_COUNT*4, FPPDIAG_BUFFER_SIZE);

	entry = fppdiag_get_entry(0xff, 0xff, &write_index);
	if (!entry)
		return;

	entry->flags = FPPDIAG_EXPT_ENTRY;
	/* TODO Use efet_memcpy when possible */
	memcpy_aligned32(entry->buffer, registers, size);

	fppdiag_notify_host(write_index);

	return;
}

void (*pfppdiag_exception_dump)(U32*) = &fppdiag_exception_dump;
#endif
#endif /* ifdef CFG_DIAGS */

#if defined(COMCERTO_2000) && !defined(COMCERTO_2000_CONTROL)
void __exit handle_exception(U32 pcval)
{
	PESTATUS_SETSTATE('DEAD');
	PESTATUS_SETERROR(pcval);
	// TODO: store PC and SP values in debug vars
#if defined(CFG_DIAGS) && !defined(COMCERTO_2000_TMU) && !defined(COMCERTO_2000_SBL)
	(*pfppdiag_exception_dump)(&exception_dump);
#endif
	esi_pe_stop((U32)'DEAD');
}
#endif
