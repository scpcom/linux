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

#ifndef _MODULE_IPSEC_H_
#define _MODULE_IPSEC_H_

#include "fe.h"
#include "common_hdrs.h"        /* For ipv4 header structure */

/*
 *  * IPSEC_DEBUG - if not defined or defined to 0 disables debug instrumentation such as drop counters
 *   * */
#if     !defined(IPSEC_DEBUG)
#define IPSEC_DEBUG     0
#endif


/* Codes for proto_family:
It copies from linux and equal to AF_INET/AF_INET6
*/
#define	PROTO_FAMILY_IPV4 2
#define PROTO_FAMILY_IPV6 10

#define IPSEC_MAX_KEY_SIZE (256 /8)
#define IPSEC_MAX_NUM_KEYS 2

/******************************
 * * IPSec API Command strutures
 * *
 * ******************************/



/* Authentication algorithms */
#define SADB_AALG_NONE                  0
#define SADB_AALG_MD5HMAC               2
#define SADB_AALG_SHA1HMAC              3
#define SADB_X_AALG_SHA2_256HMAC        5
#define SADB_X_AALG_SHA2_384HMAC        6
#define SADB_X_AALG_SHA2_512HMAC        7
#define SADB_X_AALG_RIPEMD160HMAC       8
#define SADB_X_AALG_AES_XCBC_MAC        9
#define SADB_X_AALG_NULL                251     /* kame */
#define SADB_AALG_MAX                   251

/* Encryption algorithms */
#define SADB_EALG_NONE                  0
#define SADB_EALG_DESCBC                2
#define SADB_EALG_3DESCBC               3
#define SADB_X_EALG_CASTCBC             6
#define SADB_X_EALG_BLOWFISHCBC         7
#define SADB_EALG_NULL                  11
#define SADB_X_EALG_AESCBC              12
#define SADB_X_EALG_AESCTR              13
#define SADB_X_EALG_AES_CCM_ICV8        14
#define SADB_X_EALG_AES_CCM_ICV12       15
#define SADB_X_EALG_AES_CCM_ICV16       16
#define SADB_X_EALG_AES_GCM_ICV8        18
#define SADB_X_EALG_AES_GCM_ICV12       19
#define SADB_X_EALG_AES_GCM_ICV16       20

/* AESGCM - 18/19/20 */
#define SADB_X_EALG_CAMELLIACBC         22
#define SADB_EALG_MAX                   253 /* last EALG */
/* private allocations should use 249-255 (RFC2407) */
#define SADB_X_EALG_SERPENTCBC  252     /* draft-ietf-ipsec-ciph-aes-cbc-00 */
#define SADB_X_EALG_TWOFISHCBC  253     /* draft-ietf-ipsec-ciph-aes-cbc-00 */

typedef struct _tIPSec_said {
	unsigned int spi;
	unsigned char sa_type;
	unsigned char proto_family;
	unsigned char replay_window;
#define NLKEY_SAFLAGS_ESN       0x1
	unsigned char flags;
	unsigned int dst_ip[4];
	unsigned int src_ip[4];		// added for NAT-T transport mode
	unsigned short mtu;
	unsigned short dev_mtu;
}IPSec_said, *PIPSec_said;

typedef struct _tIPSec_key_desc {
	unsigned short key_bits;
	unsigned char key_alg;
	unsigned char  key_type;
	unsigned char key[IPSEC_MAX_KEY_SIZE];
}IPSec_key_desc, *PIPSec_key_desc;

typedef struct _tIPSec_lifetime {
	unsigned int allocations;
	unsigned int bytes[2];
}IPSec_lifetime, *PIPSec_lifetime;


typedef struct _tCommandIPSecCreateSA {
	unsigned short sagd;
	unsigned short rsvd;
	IPSec_said said;
}CommandIPSecCreateSA, *PCommandIPSecCreateSA;

typedef struct _tCommandIPSecDeleteSA {
	unsigned short sagd;
	unsigned short rsvd;
}CommandIPSecDeleteSA, *PCommandIPSecDeleteSA;

typedef struct _tCommandIPSecSetKey {
	unsigned short sagd;
	unsigned short rsvd;
	unsigned short num_keys;
	unsigned short rsvd2;
	IPSec_key_desc keys[IPSEC_MAX_NUM_KEYS];
}CommandIPSecSetKey, *PCommandIPSecSetKey;

typedef struct _tCommandIPSecSetNatt {
	unsigned short sagd;
	unsigned short sport;
	unsigned short dport;
	unsigned short rsvd;
}CommandIPSecSetNatt, *PCommandIPSecSetNatt;

typedef struct _tCommandIPSecSetState {
	unsigned short sagd;
	unsigned short rsvd;
	unsigned short state;
	unsigned short rsvd2;
}CommandIPSecSetState, *PCommandIPSecSetState;

typedef struct _tCommandIPSecSetTunnel {
	unsigned short sagd;
	unsigned char rsvd;
	unsigned char proto_family;
	union {
		ipv4_hdr_t   ipv4h;
		ipv6_hdr_t   ipv6h;
	} h;
}CommandIPSecSetTunnel, *PCommandIPSecSetTunnel;

typedef struct _tCommandIPSecSetTunnelRoute {
        unsigned short sagd;
        unsigned short route_id;
}CommandIPSecSetTunnelRoute, *PCommandIPSecSetTunnelRoute;


typedef struct _tCommandIPSecSetLifetime{
	unsigned short sagd;
	unsigned short rsvd;
	IPSec_lifetime  hard_time;
	IPSec_lifetime  soft_time;
	IPSec_lifetime  current_time;
}CommandIPSecSetLifetime, *PCommandIPSecSetLifetime;


typedef struct _tCommandIPSecExpireNotify{
	unsigned short sagd;
	unsigned short rsvd;
	unsigned int  action;
}CommandIPSecExpireNotify, *PCommandIPSecExpireNotify;

typedef struct _tCommandIPSecSetPreFrag {
		unsigned short pre_frag_en;		
		unsigned short reserved;
} CommandIPSecSetPreFrag, *PCommandIPSecSetPreFrag;

typedef struct _tSAQueryCommand {
  unsigned short action;
  unsigned short handle; /* handle */ 
  /*SPI information */
  unsigned short mtu;    /* mtu configured */ 
  unsigned short rsvd1;
  unsigned int spi;      /* spi */ 
  unsigned char sa_type; /* SA TYPE Prtocol ESP/AH */
  unsigned char family; /* Protocol Family */
  unsigned char mode; /* Tunnel/Transport mode */
  unsigned char replay_window; /* Replay Window */
  unsigned int dst_ip[4];
  unsigned int src_ip[4];
  
  /* Key information */
  unsigned char key_alg;
  unsigned char state; /* SA VALID /EXPIRED / DEAD/ DYING */
  unsigned short flags; /* ESP AH enabled /disabled */
  
  unsigned char cipher_key[32];
  unsigned char auth_key[20];
  unsigned char ext_auth_key[12];
     
  /* Tunnel Information */
  unsigned char tunnel_proto_family;
  unsigned char rsvd[3];
  union  {
	ipv4_hdr_t ipv4;
	ipv6_hdr_t ipv6;
  } __attribute__((packed)) tnl;

  U64	soft_byte_limit;
  U64	hard_byte_limit;
  U64	soft_packet_limit;
  U64	hard_packet_limit;
  
} __attribute__((packed)) SAQueryCommand, *PSAQueryCommand;



#if !defined(COMCERTO_2000)
U16 M_ipsec_cmdproc(U16 cmd_code, U16 cmd_len, U16 *pcmd);
static int IPsec_handle_CREATE_SA(U16 *p, U16 Length);
static int IPsec_handle_DELETE_SA(U16 *p, U16 Length);
static int IPsec_handle_FLUSH_SA(U16 *p, U16 Length);
static int IPsec_handle_SA_SET_KEYS(U16 *p, U16 Length);
static int IPsec_handle_SA_SET_TUNNEL(U16 *p, U16 Length);
static int IPsec_handle_SA_SET_NATT(U16 *p, U16 Length);
static int IPsec_handle_SA_SET_STATE(U16 *p, U16 Length);
static int IPsec_handle_SA_SET_LIFETIME(U16 *p, U16 Length);
static int IPsec_handle_FRAG_CFG(U16 *p, U16 Length);
#endif


/* Debugging */
/*
Display memory command
*/
typedef struct _tDMCommand {
  unsigned short pad_in_rc_out; /* Padding - retcode */
  unsigned short length;        /* Lenght of memory to display < 224 bytes 
				** returns length being displayed in response */
  unsigned int address;         /* msp address of memory to display 
			** returns address being displayed in response */
} DMCommand, *PDMCommand;


U16 M_ipsec_debug(U16 cmd_code, U16 cmd_len, U16 *pcmd);
__inline  void read_random(unsigned char *p_result, unsigned int rlen);

#if     defined(IPSEC_DEBUG) && (IPSEC_DEBUG)
#       define IPSEC_DBG(x) do { x} while(0)
#else
#       define IPSEC_DBG(x)
#endif



/****** IPSEC related common structures *****/
static __inline U16 HASH_SA(U32 *Daddr, U32 spi, U16 Proto, U8 family)
{
        U16 sum;
        U32 tmp32;

        tmp32 = ntohl(Daddr[0]) ^ ntohl(Daddr[1]) ^ ntohl(spi);
        sum = (tmp32 >> 16) + (tmp32 & 0xffff) + Proto;
        return ((sum ^ (sum >> 8)) & (NUM_SA_ENTRIES - 1));
}


#define SA_MAX_OP		2	// maximum of stackable SA (ESP+AH)

#define FLOW_DIR_IN     0  //  Input flow  for all local traffic
#define FLOW_DIR_OUT    1  // Output flow to be sent out with ipsec policy applied
#define FLOW_DIR_FWD    2  // Forwarded flow for all traffic getting forwarded

#define FLOW_DIR_IN_BITVAL (1 << FLOW_DIR_IN)
#define FLOW_DIR_OUT_BITVAL (1 << FLOW_DIR_OUT)
#define FLOW_DIR_FWD_BITVAL (1 << FLOW_DIR_FWD)


typedef  struct  AH_HDR_STRUCT
{
        U8  nexthdr;
        U8  hdrlen;             /* This one is measured in 32 bit units! */
        U16 reserved;
        U32 spi;
        U32 seq_no;             /* Sequence number */
        U8  auth_data[4];       /* Variable len but >=4. Mind the 64 bit alignment! */
} ip_ah_hdr ;


typedef  struct  ESP_HDR_STRUCT
{
        U32 spi;
        U32 seq_no;
        U32 enc_data[1];
} ip_esp_hdr;


/* from Linux XFRM stack... (should we use PF_KEY value instead ?) */
enum {
	XFRM_STATE_VOID,
	XFRM_STATE_ACQ,
	XFRM_STATE_VALID,
	XFRM_STATE_ERROR,
	XFRM_STATE_EXPIRED,
	XFRM_STATE_DEAD
};

#define SA_MODE_TUNNEL 0x1
#define SA_MODE_TRANSPORT 0x0

typedef struct _tSA_lft_conf {
	U64	soft_byte_limit;
	U64	hard_byte_limit;
	U64	soft_packet_limit;
	U64	hard_packet_limit;
} SA_lft_conf, *PSA_lft_conf;

/*
* Scatter/Gather data descriptor table.
*/
struct elp_ddtdesc {
	/* volatile  */U32	frag_addr;		
	/* volatile  */U32	frag_size;		
};


#if defined(COMCERTO_100)
#include "module_ipsec_c100.h"
#elif defined(COMCERTO_1000)
#include "module_ipsec_c1000.h"
#elif defined(COMCERTO_2000)
#include "module_ipsec_c2000.h"
#endif

#if defined(COMCERTO_2000_CONTROL)
void* M_ipsec_sa_cache_lookup_by_spi(U32 *daddr, U32 spi, U8 proto, U8 family);
void* M_ipsec_sa_cache_lookup_by_h(U16 handle);
extern struct slist_head sa_cache_by_spi[];
extern struct slist_head sa_cache_by_h[];
#elif defined(COMCERTO_2000_CLASS)
PSAEntry M_ipsec_sa_cache_lookup_by_spi(U32 *daddr, U32 spi, U8 proto, U8 family);
PSAEntry M_ipsec_sa_cache_lookup_by_h(U16 handle, PSAEntry);
extern PSAEntry sa_cache_by_spi[];
extern PSAEntry sa_cache_by_h[];
#elif defined(COMCERTO_2000_UTIL)
PSAEntry M_ipsec_sa_cache_lookup_by_h(U16 handle, PSAEntry);
extern PSAEntry sa_cache_by_h[];
#else
void* M_ipsec_sa_cache_lookup_by_spi(U32 *daddr, U32 spi, U8 proto, U8 family);
void* M_ipsec_sa_cache_lookup_by_h(U16 handle);
extern struct slist_head sa_cache_by_spi[];
extern struct slist_head sa_cache_by_h[];
#endif


/* NAT-T implementation */

extern PNatt_Socket_v6 gNatt_Sock_v6_cache[];
extern PNatt_Socket_v4 gNatt_Sock_v4_cache[];



/****** IPSEC related common structures *****/

/*
 *  * Codes
 *  */
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



/* Fixed mapping between source and destination ddts */
#define DST_DDTE(src_ddte) (&(src_ddte[ELP_MAX_DDT_PER_PKT]))



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


 
/****** IPSEC HW related common structures *****/

// Dummy ip headers are presently non-cachable hence flushing them is not needed
// // If they become cachable change flush definition below.
#define IPSEC_flush_ipv4h(start,end) 
//#define IPSEC_flush_if_cachable(start,end) L1_dc_flush(start,end)


/* SA notifications */
#define IPSEC_SOFT_EXPIRE 0
#define IPSEC_HARD_EXPIRE 1

static __inline void consistent_elpctl(PElliptic_SA elp_sa_ptr, int sa_cache_flush)
{
#if !defined (COMCERTO_2000)
	U8 *end_ptr = ((U8*) (elp_sa_ptr + 1)) - 1;
	/* Make sure memory is settled*/
	L1_dc_clean(elp_sa_ptr, end_ptr);
#elif defined(COMCERTO_2000_CONTROL)
	wmb();
#endif
	/*  DSB(); - done as a part of cleaning */
	/* sa_cache_flush should be always 0 for C100 */
	hw_sa_cache_flush(elp_sa_ptr , sa_cache_flush);

}


int M_ipsec_ttl_check_time(void);

extern struct tIPSec_hw_context gIpSecHWCtx;
extern int gIpsec_available;
BOOL M_ipsec_pre_inbound_init(PModuleDesc pModule);
BOOL M_ipsec_post_inbound_init(PModuleDesc pModule);
BOOL M_ipsec_pre_outbound_init(PModuleDesc pModule);
BOOL M_ipsec_post_outbound_init(PModuleDesc pModule);
void M_ipsec_outbound_entry(void);
void M_ipsec_outbound_callback(void);
void M_ipsec_inbound_entry(void);
void M_ipsec_inbound_callback(void);

PNatt_Socket_v6 IPsec_Find_Natt_socket_v6(U32 /* __attribute__((packed)) */ *src_ip, U32 /* __attribute__((packed)) */ *dst_ip, unsigned short  sport, unsigned short dport);

PNatt_Socket_v4 IPsec_Find_Natt_socket_v4(U32 src_ip, U32 dst_ip, unsigned short  sport, unsigned short dport);
#if !defined(COMCERTO_2000_UTIL)
int M_ipsec_is_fragmentation_required(PMetadata mtd,  void *l3_hdr, u32 encap_len) __attribute__ ((noinline));
int M_ipsec_process_sa_ids(PMetadata mtd, void *Entry, u8 proto, int *ipsec_rc) __attribute__ ((noinline));
void M_ipsec_prefragment_outbound(PMetadata mtd, U32 mtu, U32 L2_hlen, U32 if_type) __attribute__ ((noinline));
#endif
void ipsec_common_hard_init(IPSec_hw_context *sc);

#if !defined (COMCERTO_2000)
static int M_ipsec_lft_check_data(PSAEntry sa);
PNatt_Socket_v6 IPsec_create_Natt_socket_v6(PSAEntry sa);
static int IPsec_Free_Natt_socket_v6(PSAEntry sa);
PNatt_Socket_v4 IPsec_create_Natt_socket_v4(PSAEntry sa);
static int IPsec_Free_Natt_socket_v4(PSAEntry sa);


int hw_sa_set_lifetime(CommandIPSecSetLifetime *cmd, PSAEntry sa);
int hw_sa_set_cipher_key(PSAEntry sa, U8* key);
int hw_sa_set_cipher_ALG_AESCTR(PSAEntry sa, U16 key_bits, U8* key , U8* algo);
int hw_sa_set_digest_key(PSAEntry sa, U16 key_bits, U8* key);
#endif

//#define CONTROL_IPSEC_DEBUG
#if defined (COMCERTO_2000_CONTROL)
BOOL ipsec_init(void);
void ipsec_exit(void);
void ipsec_standalone_init(void);
#endif
#endif
