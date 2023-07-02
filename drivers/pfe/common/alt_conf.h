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


#ifndef _ALT_CONF_H_
#define _ALT_CONF_H_

#include "types.h"
#include "module_timer.h"

int ALTCONF_HandleCONF_SET (U16 *p, U16 Length);
int ALTCONF_HandleCONF_RESET_ALL (U16 *p, U16 Length);

#define ALTCONF_OPTION_MCTTL		1	/*Multicast TTL option */
#define ALTCONF_OPTION_IPSECRL        2  /*IPSEC Rate Limiting option */
#define ALTCONF_OPTION_MAX			ALTCONF_OPTION_IPSECRL
 #define ALTCONF_OPTIN_MAX_PARAMS    3 /* IPSEC Rate Limiting has 3 parameters. To be updated if a new option is add with more 32bits params */


/* Multicast TTL Configuration definitions */
#define ALTCONF_MCTTL_MODE_DEFAULT	0
#define ALTCONF_MCTTL_MODE_IGNORE	1
#define ALTCONF_MCTTL_MODE_MAX 		ALTCONF_MCTTL_MODE_IGNORE
#define ALTCONF_MCTTL_NUM_PARAMS 	1  //maximu number of u32 allowed for this option

#if !defined(COMCERTO_100)
/* IPSEC Rate Limiting Configuration definitions */
#define ALTCONF_IPSECRL_ON  1
#define ALTCONF_IPSECRL_OFF 0
#define ALTCONF_IPSECRL_NUM_PARAMS      3  //maximu number of u32 allowed for this option


#define ALTCONF_IPSEC_INBOUND    0
#define ALTCONF_IPSEC_OUTBOUND 1

#define ALTCONF_FCS_SIZE 			4
#define ALTCONF_RX_MTU 		1518

#define ALTCONF_bps_PER_Kbps		1000
#define ALTCONF_MILLISEC_PER_SEC	1000
#define ALTCONF_BITS_PER_BYTE		8

#if !defined(COMCERTO_2000)
// IPSEC Rate Limit Information Structure  
typedef struct tAltConfIpsec_RlEntry{
   U32 enable; // Specifies whether IPSEC rate limiting is enabled or not
   U32 aggregate_bandwidth; //Configured Aggregate bandwidth in Mbps
   int tokens_per_clock_period; // Number of bytes worth token available every clock period
   int inbound_tokens_available; // Number of bytes worth tokens available to receive inbound traffic
   int outbound_tokens_available; // Number of bytes worth tokens available to transmit outbound traffic 
   int bucket_size; // Configurable bucket Sizes in bytes 
} AltConfIpsec_RlEntry;
#elif defined(COMCERTO_2000_CONTROL)
typedef struct tAltConfIpsec_RlEntry{
   U32 enable; // Specifies whether IPSEC rate limiting is enabled or not
   U32 aggregate_bandwidth; //Configured Aggregate bandwidth in Mbps
   int tokens_per_clock_period; // Number of bytes worth token available every clock period
   int bucket_size; // Configurable bucket Sizes in bytes
} AltConfIpsec_RlEntry;
#else	// defined(COMCERTO_2000)
typedef struct tAltConfIpsec_RlEntry{
	U32 enable; // Specifies whether IPSEC rate limiting is enabled or not
   int tokens_per_clock_period; // Number of bytes worth token available every clock period
   int inbound_tokens_available; // Number of bytes worth tokens available to receive inbound traffic
   int outbound_tokens_available; // Number of bytes worth tokens available to transmit outbound traffic
   int bucket_size; // Configurable bucket Sizes in bytes
} AltConfIpsec_RlEntry;
#endif

extern AltConfIpsec_RlEntry AltConfIpsec_RateLimitEntry;
extern TIMER_ENTRY ipsec_ratelimiter_timer;

void AltConfIpsec_RateLimit_token_generator(void);
U8 AltConfIpsec_Rate_Limiter(int mode, U32 sizeofpacket);

#endif /* #if !defined(COMCERTO_100) */

typedef struct _tAltConfCommandSet {
	U16		option_id;
	U16		num_params;
	U32		params[ALTCONF_OPTIN_MAX_PARAMS];
}AltConfCommandSet , *PAltConfCommandSet;



#endif /* _ALT_CONF_H_ */
