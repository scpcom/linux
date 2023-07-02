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


// Note that Macros are all uppercase

#ifndef _CHECKSUM_H_
#define _CHECKSUM_H_

#include "types.h"
#include "common_hdrs.h"

#ifdef GCC_TOOLCHAIN
U16 ip_fast_checksum(void *iph, unsigned int ihl);

U32 checksum_partial_nofold(void *buf, U32 len, U32 sum);

U16 checksum_partial(void *buf, U32 len, U32 sum);

U32 pseudoheader_checksum(U32 saddr, U32 daddr, U16 len, U8 proto);
U16 compute_checksum(void *dmem_addr, unsigned int dmem_len, void *ddr_addr, unsigned int ddr_len, U32 sum);

static inline U16 checksum_fold(U32 sum)
{
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~((U16)sum);
}

static inline U16 ip_fast_checksum_gen(ipv4_hdr_t *iph)
{
	iph->HeaderChksum = 0;
	return ip_fast_checksum(iph, iph->Version_IHL & 0x0f);
}

#else

/********************************************************************************
*
*      NAME:        IPHeader_Checksum
*
*      USAGE:          parameter(s):
*                                        			- iph : pointer on the begin of IP header.
*								- ihl : header length in WORD (32 bit)
*
*	                      return value:                  
*								- The checksum field is the 16 bit one's complement of
*								   the one's complement sum of all 16 bit words in the
*								   header.
*
*      DESCRIPTION:	IP header must bit 32 bit aligned. ihl is a number of WORD.
*					Checksum is already complemented
*
********************************************************************************/ 
U16 ip_fast_checksum(void* iph,  unsigned int ihl);
U16 ip_fast_checksum_gen(void* iph);


/********************************************************************************
*
*      NAME:        pseudoheader_checksum
*
*      USAGE:          parameter(s):
*                                        			- saddr : IP source address
*								- daddr : IP dest address
*								- len : data length
*								- proto : IP protocol
*
*	                      return value:                  
*								- 16 bit checksum of Pseudo-header, not complemented
*								   to be used as input of checksum_partial
*
********************************************************************************/ 
U16 pseudoheader_checksum(U32 saddr, U32 daddr, U16 len,  U8 proto);
U16 merge_checksum(U32 temp1, U32 Temp2);

/********************************************************************************
*
*      NAME:        checksum_partial
*
*      USAGE:          parameter(s):
*                                        			- buff : pointer on data
*								- len : length in bytes
*								- sum : previous checksum
*
*	                      return value:                  
*								- 16 bit checksum, complemented
*								
********************************************************************************/
U16 checksum_partial(void *buff, U16 len, U32 sum);
U16 Internet_checksum(void *buff, U16 len, U32 sum);

#endif

// The udpheader_checksum and tcpheader_checksum routines return uncomplemented values

#define udpheader_checksum(buff, sum) ((U16)~checksum_partial((buff), sizeof(udp_hdr_t), (sum)))
#define tcpheader_checksum(buff, sum) ((U16)~checksum_partial((buff), sizeof(tcp_hdr_t), (sum)))

#endif /* _CHECKSUM_H_ */
