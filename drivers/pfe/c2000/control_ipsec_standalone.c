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

#include "module_ipsec.h"
#include "module_hidrv.h"
#include "system.h"

#define SAGD_4_OUT      0x9020
#define SAGD_4_IN       0x9021
int IPsec_handle_CREATE_SA(U16 *p, U16 Length);
int IPsec_handle_SA_SET_KEYS(U16 *p, U16 Length);
int IPsec_handle_SA_SET_TUNNEL(U16 *p, U16 Length);
int IPsec_handle_SA_SET_STATE(U16 *p, U16 Length);
int IPsec_handle_SA_SET_LIFETIME(U16 *p, U16 Length);
int IPv4_HandleIP_CONNTRACK(U16 *p, U16 Length);
int IPv6_handle_CONNTRACK(U16 *p, U16 Length);

int IP_HandleIP_ROUTE_RESOLVE (U16 *p, U16 Length);
CommandIPSecCreateSA create_cmd =
{
        SAGD_4_OUT,
        0x0,
        { 0x42020000, /* spi */
                IPPROTOCOL_ESP,
                PROTO_FAMILY_IPV4,
                0x0, /* replay_window */
                0x0, /* pad */
                {0x2a1eA8C0,0,0,0},
                {0,0,0,0},
                1500, /*mtu*/
                1500
        }
};

CommandIPSecCreateSA create_cmd_v6 =
{
        SAGD_4_OUT,
        0x0,
        { 0x42060000, /* spi */
                IPPROTOCOL_ESP,
                PROTO_FAMILY_IPV6,
                0x0, /* replay_window */
                0x0, /* pad */
                {0x00000120,0,0,0x02000000},
                {0,0,0,0},
                1500, /*mtu*/
                1500
        }
};


CommandIPSecSetKey setkey_cmd =
{
        SAGD_4_OUT,
        0x0,
        2 , /* num keys */
        0x0,
        {
                {
                        160,
                        SADB_AALG_SHA1HMAC,
                        0,
                        {0x59,0x4d,0x1d,0x65,0x98,0x84,0x15,0x0a,0xda,0x9a,0x74,0x19,0x5e,0xac,0xa7,0xcd,0x63,0x58,0xc7,0x20}
                },
                {
                        192,
                        SADB_EALG_3DESCBC,
                        1,
                        {0x7a,0xea,0xca,0x3f,0x87,0xd0,0x60,0xa1,0x2f,0x4a,0x44,0x87,0xd5,0xa5,0xc3,0x35,0x59,0x20,0xfa,0xe6,0x9a,0x96,0xc8,0x32}
                }
        }
};


CommandIPSecCreateSA create_cmd_1 =
{
        SAGD_4_IN,
        0x0,
        { 0x42030000, /* spi */
                IPPROTOCOL_ESP,
                PROTO_FAMILY_IPV4,
                0x0, /* replay_window */
                0x0, /* pad */
                {0x341eA8C0,0,0,0},
                {0,0,0,0},
                1500, /*mtu*/
                1500
        }
};

CommandIPSecCreateSA create_cmd_1_v6 =
{
        SAGD_4_IN,
        0x0,
        { 0x42070000, /* spi */
                IPPROTOCOL_ESP,
                PROTO_FAMILY_IPV6,
                0x0, /* replay_window */
                0x0, /* pad */
                {0x00000120,0,0,0x01000000},
                {0,0,0,0},
                1500, /*mtu*/
                1500
        }
};


CommandIPSecSetKey setkey_cmd_1 =
{
        SAGD_4_IN,
        0x0,
        2 , /* num keys */
        0x0,
        {
                {
                        160,
                        SADB_AALG_SHA1HMAC,
                        0,
                        {0xb9,0xd6,0xeb,0x8d,0xf8,0xad,0xe9,0xc0,0xff,0x2f,0xe8,0xba,0x40,0xb0,0x2c,0xa1,0x61,0x25,0x0d,0x23}
                },
                {
                        192,
                        SADB_EALG_3DESCBC,
                        1,
                        { 0xf6,0xdd,0xb5,0x55,0xac,0xfd,0x9d,0x77,0xb0,0x3e,0xa3,0x84,0x3f,0x26,0x53,0x25,0x5a,0xfe,0x8e,0xb5,0x57,0x39,0x65,0xd2}
                }
        }
};

u8 wan_if[12] = "eth0";
u8 lan_if[12] = "eth2";

//#ifdef CONFIG_IPV4_TUNNEL
#if 1 
void ipsec_standalone_init(void)
{
	int rc;
        union {
                CommandIPSecSetLifetime setlft_cmd;
                CommandIPSecSetTunnel settunnel_cmd;
                CommandIPSecSetState setstate_cmd;
        } ipscmd;
	RtCommand rt_cmd;

        //int i;

        U16 CtExCmd[32] = {ACTION_REGISTER, //register new Conntrack entry
                0x0001, // Conntrack is secure
                0xA8C0, //src IP address of querier        (192.168.1.111)
                0x6F01, //src IP address of querier
                0xA8C0, //dst IP address of querier (192.168.2.222)
                0xDE02, //dst IP address of querier
                0x1122, //src port number of querier ()
                0x3344, //dst port number of querier ()
                0xA8C0, //src IP address of replier (192.168.32.222)
                0xDE02, //src IP address of replier
                0xA8C0, //dst IP address of replier (192.168.31.111)
                0x6F01, //dst IP address of replier
                0x3344, //src port number of replier ()
                0x1122, //dst port number of replier ()
                0x0011,  //protocol number (UDP=17)
                0x0000, // pad
                0x0000, // fwmark
                0x0000, // fwmark
                0x0011, // routeid
                0x0000, // routeid
                0x0012, // reply routeid
                0x0000, // reply routeid
                1, // Sa_nr
                SAGD_4_IN, // sa_handle
                0x0,0x0,0x0, //sa_handle
                1, //sa_reply_nr
                SAGD_4_OUT, // sa_handle
                0x0,0x0,0x0, //sa_handle
        };
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s called..\n", __func__);
#endif

        /* First SA */
        IPsec_handle_CREATE_SA((U16*) &create_cmd, sizeof(create_cmd));
        IPsec_handle_SA_SET_KEYS((U16*) &setkey_cmd, sizeof(setkey_cmd));
        memset(&ipscmd.settunnel_cmd,0,sizeof(ipscmd.settunnel_cmd));
        ipscmd.settunnel_cmd.sagd=SAGD_4_OUT;
        ipscmd.settunnel_cmd.proto_family=PROTO_FAMILY_IPV4;
        ipscmd.settunnel_cmd.h.ipv4h.Version_IHL = 0x45;
        ipscmd.settunnel_cmd.h.ipv4h.TTL = 0x20;
        ipscmd.settunnel_cmd.h.ipv4h.SourceAddress = htonl(0xc0a81e34);
        ipscmd.settunnel_cmd.h.ipv4h.DestinationAddress = htonl(0xc0a81e2a);
        IPsec_handle_SA_SET_TUNNEL((U16*) &ipscmd.settunnel_cmd, sizeof(ipscmd.settunnel_cmd));
        memset(&ipscmd.setlft_cmd,0,sizeof(ipscmd.setlft_cmd));
        ipscmd.setlft_cmd.sagd = SAGD_4_OUT;
        IPsec_handle_SA_SET_LIFETIME((U16*) &ipscmd.setlft_cmd, sizeof(ipscmd.setlft_cmd));
        memset(&ipscmd.setstate_cmd,0,sizeof(ipscmd.setstate_cmd));
        ipscmd.setstate_cmd.sagd = SAGD_4_OUT;
        ipscmd.setstate_cmd.state = XFRM_STATE_VALID;

        IPsec_handle_SA_SET_STATE((U16*) &ipscmd.setstate_cmd, sizeof(ipscmd.setstate_cmd));
        /* Second SA */
        IPsec_handle_CREATE_SA((U16*) &create_cmd_1, sizeof(create_cmd_1));
        IPsec_handle_SA_SET_KEYS((U16*) &setkey_cmd_1, sizeof(setkey_cmd_1));
        memset(&ipscmd.settunnel_cmd,0,sizeof(ipscmd.settunnel_cmd));
        ipscmd.settunnel_cmd.sagd=SAGD_4_IN;
        ipscmd.settunnel_cmd.proto_family=PROTO_FAMILY_IPV4;
        ipscmd.settunnel_cmd.h.ipv4h.Version_IHL = 0x45;
        ipscmd.settunnel_cmd.h.ipv4h.TTL = 0x20;
        ipscmd.settunnel_cmd.h.ipv4h.SourceAddress = htonl(0xc0a81e2a);
        ipscmd.settunnel_cmd.h.ipv4h.DestinationAddress = htonl(0xc0a81e34);
        IPsec_handle_SA_SET_TUNNEL((U16*) &ipscmd.settunnel_cmd, sizeof(ipscmd.settunnel_cmd));
        memset(&ipscmd.setlft_cmd,0,sizeof(ipscmd.setlft_cmd));
        ipscmd.setlft_cmd.sagd = SAGD_4_IN;
        IPsec_handle_SA_SET_LIFETIME((U16*) &ipscmd.setlft_cmd, sizeof(ipscmd.setlft_cmd));
        memset(&ipscmd.setstate_cmd,0,sizeof(ipscmd.setstate_cmd));
        ipscmd.setstate_cmd.sagd = SAGD_4_IN;
        ipscmd.setstate_cmd.state = XFRM_STATE_VALID;
        IPsec_handle_SA_SET_STATE((U16*) &ipscmd.setstate_cmd, sizeof(ipscmd.setstate_cmd));

	rt_cmd.action = ACTION_REGISTER;
	rt_cmd.mtu = 1500;
	rt_cmd.macAddr[0] = 0x00;
	rt_cmd.macAddr[1] = 0xAA;
	rt_cmd.macAddr[2] = 0xBB;
	rt_cmd.macAddr[3] = 0xCC;
	rt_cmd.macAddr[4] = 0xDD;
	rt_cmd.macAddr[5] = 0xEE;
	memcpy(rt_cmd.outputDevice , wan_if, 12);
	rt_cmd.id = 0x11;
	rt_cmd.flags = 0;
	rt_cmd.daddr[3] = 0xc0a802de;
	rt_cmd.daddr[0] =0;
	rt_cmd.daddr[1] = 0;
	rt_cmd.daddr[2] = 0;

	rc = IP_HandleIP_ROUTE_RESOLVE((U16*)&rt_cmd, sizeof(rt_cmd));

	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding orig route entry %d\n", rc);
		return;
	}

	printk(KERN_INFO "Originator Route entry added successfully :%d\n",rc);

	rt_cmd.action = ACTION_REGISTER;
	rt_cmd.mtu = 1500;
	rt_cmd.macAddr[0] = 0x00;
	rt_cmd.macAddr[1] = 0x02;
	rt_cmd.macAddr[2] = 0x03;
	rt_cmd.macAddr[3] = 0x04;
	rt_cmd.macAddr[4] = 0x05;
	rt_cmd.macAddr[5] = 0x06;
	memcpy(rt_cmd.outputDevice , lan_if, 12);
	rt_cmd.id = 0x12;
	rt_cmd.flags = 0;
	rt_cmd.daddr[0] = 0xc0a8016f;
	rt_cmd.daddr[1] =0;
	rt_cmd.daddr[2] = 0;
	rt_cmd.daddr[3] = 0;

	rc = IP_HandleIP_ROUTE_RESOLVE((U16*)&rt_cmd, sizeof(rt_cmd));
	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding replier route entry %d\n", rc);
		return;
	}
	printk(KERN_INFO "Replier Route entry added successfully :%d\n",rc);

	rc = IPv4_HandleIP_CONNTRACK(CtExCmd, 32*2);

	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding conntrack entry %d\n", rc);
		return;
	}
#if 0
        for(i=0;i<4;i++) {
                IPv4_HandleIP_CONNTRACK(CtExCmd, 28*2);
                CtExCmd[6] = htons(ntohs(CtExCmd[6]) + 1);
                CtExCmd[13] = htons(ntohs(CtExCmd[13]) + 1);
        }
#endif

}
#else


void ipsec_standalone_init(void)
{
	int rc;
        union {
                CommandIPSecSetLifetime setlft_cmd;
                CommandIPSecSetTunnel settunnel_cmd;
                CommandIPSecSetState setstate_cmd;
        } ipscmd;
	RtCommand rt_cmd;

        //int i;

        U16 CtExCmd[32] = {ACTION_REGISTER, //register new Conntrack entry
                0x0001, // Conntrack is secure
                0xA8C0, //src IP address of querier        (192.168.1.111)
                0x6F01, //src IP address of querier
                0xA8C0, //dst IP address of querier (192.168.2.222)
                0xDE02, //dst IP address of querier
                0x1122, //src port number of querier ()
                0x3344, //dst port number of querier ()
                0xA8C0, //src IP address of replier (192.168.32.222)
                0xDE02, //src IP address of replier
                0xA8C0, //dst IP address of replier (192.168.31.111)
                0x6F01, //dst IP address of replier
                0x3344, //src port number of replier ()
                0x1122, //dst port number of replier ()
                0x0011,  //protocol number (UDP=17)
                0x0000, // pad
                0x0000, // fwmark
                0x0000, // fwmark
                0x0011, // routeid
                0x0000, // routeid
                0x0012, // reply routeid
                0x0000, // reply routeid
                1, // Sa_nr
                SAGD_4_IN, // sa_handle
                0x0,0x0,0x0, //sa_handle
                1, //sa_reply_nr
                SAGD_4_OUT, // sa_handle
                0x0,0x0,0x0, //sa_handle
        };
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s called..\n", __func__);
#endif

        /* First SA */
        IPsec_handle_CREATE_SA((U16*) &create_cmd_v6, sizeof(create_cmd));
        IPsec_handle_SA_SET_KEYS((U16*) &setkey_cmd, sizeof(setkey_cmd));
        memset(&ipscmd.settunnel_cmd,0,sizeof(ipscmd.settunnel_cmd));
        ipscmd.settunnel_cmd.sagd=SAGD_4_OUT;
        ipscmd.settunnel_cmd.proto_family=PROTO_FAMILY_IPV6;
        ipscmd.settunnel_cmd.h.ipv6h.Version_TC_FLHi = htons(0x6000);
        ipscmd.settunnel_cmd.h.ipv6h.FlowLabelLo = 0;
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[1] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[2] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[3] = htonl(0x1);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[1] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[2] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[3] = htonl(0x2);

        IPsec_handle_SA_SET_TUNNEL((U16*) &ipscmd.settunnel_cmd, sizeof(ipscmd.settunnel_cmd));
        memset(&ipscmd.setlft_cmd,0,sizeof(ipscmd.setlft_cmd));
        ipscmd.setlft_cmd.sagd = SAGD_4_OUT;
        IPsec_handle_SA_SET_LIFETIME((U16*) &ipscmd.setlft_cmd, sizeof(ipscmd.setlft_cmd));
        memset(&ipscmd.setstate_cmd,0,sizeof(ipscmd.setstate_cmd));
        ipscmd.setstate_cmd.sagd = SAGD_4_OUT;
        ipscmd.setstate_cmd.state = XFRM_STATE_VALID;

        IPsec_handle_SA_SET_STATE((U16*) &ipscmd.setstate_cmd, sizeof(ipscmd.setstate_cmd));
        /* Second SA */
        IPsec_handle_CREATE_SA((U16*) &create_cmd_1_v6, sizeof(create_cmd_1));
        IPsec_handle_SA_SET_KEYS((U16*) &setkey_cmd_1, sizeof(setkey_cmd_1));
        memset(&ipscmd.settunnel_cmd,0,sizeof(ipscmd.settunnel_cmd));
        ipscmd.settunnel_cmd.sagd=SAGD_4_IN;
        ipscmd.settunnel_cmd.proto_family=PROTO_FAMILY_IPV6;
        ipscmd.settunnel_cmd.h.ipv6h.Version_TC_FLHi = htons(0x6000);
        ipscmd.settunnel_cmd.h.ipv6h.FlowLabelLo = 0;
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[1] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[2] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[3] = htonl(0x2);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[1] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[2] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[3] = htonl(0x1);
        IPsec_handle_SA_SET_TUNNEL((U16*) &ipscmd.settunnel_cmd, sizeof(ipscmd.settunnel_cmd));
        memset(&ipscmd.setlft_cmd,0,sizeof(ipscmd.setlft_cmd));
        ipscmd.setlft_cmd.sagd = SAGD_4_IN;
        IPsec_handle_SA_SET_LIFETIME((U16*) &ipscmd.setlft_cmd, sizeof(ipscmd.setlft_cmd));
        memset(&ipscmd.setstate_cmd,0,sizeof(ipscmd.setstate_cmd));
        ipscmd.setstate_cmd.sagd = SAGD_4_IN;
        ipscmd.setstate_cmd.state = XFRM_STATE_VALID;
        IPsec_handle_SA_SET_STATE((U16*) &ipscmd.setstate_cmd, sizeof(ipscmd.setstate_cmd));

	rt_cmd.action = ACTION_REGISTER;
	rt_cmd.mtu = 1500;
	rt_cmd.macAddr[0] = 0x00;
	rt_cmd.macAddr[1] = 0x1A;
	rt_cmd.macAddr[2] = 0xBB;
	rt_cmd.macAddr[3] = 0x1F;
	rt_cmd.macAddr[4] = 0xEE;
	rt_cmd.macAddr[5] = 0xCD;
	memcpy(rt_cmd.outputDevice , wan_if, 12);
	rt_cmd.id = 0x11;
	rt_cmd.flags = 0;
	rt_cmd.daddr[3] = 0xc0a802de;
	rt_cmd.daddr[0] =0;
	rt_cmd.daddr[1] = 0;
	rt_cmd.daddr[2] = 0;

	rc = IP_HandleIP_ROUTE_RESOLVE((U16*)&rt_cmd, sizeof(rt_cmd));

	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding orig route entry %d\n", rc);
		return;
	}

	printk(KERN_INFO "Originator Route entry added successfully :%d\n",rc);

	rt_cmd.action = ACTION_REGISTER;
	rt_cmd.mtu = 1500;
	rt_cmd.macAddr[0] = 0x00;
	rt_cmd.macAddr[1] = 0x02;
	rt_cmd.macAddr[2] = 0x03;
	rt_cmd.macAddr[3] = 0x04;
	rt_cmd.macAddr[4] = 0x05;
	rt_cmd.macAddr[5] = 0x06;
	memcpy(rt_cmd.outputDevice , lan_if, 12);
	rt_cmd.id = 0x12;
	rt_cmd.flags = 0;
	rt_cmd.daddr[0] = 0xc0a8016f;
	rt_cmd.daddr[1] =0;
	rt_cmd.daddr[2] = 0;
	rt_cmd.daddr[3] = 0;

	rc = IP_HandleIP_ROUTE_RESOLVE((U16*)&rt_cmd, sizeof(rt_cmd));
	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding replier route entry %d\n", rc);
		return;
	}
	printk(KERN_INFO "Replier Route entry added successfully :%d\n",rc);

	rc = IPv4_HandleIP_CONNTRACK(CtExCmd, 32*2);

	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding conntrack entry %d\n", rc);
		return;
	}
#if 0
        for(i=0;i<4;i++) {
                IPv4_HandleIP_CONNTRACK(CtExCmd, 28*2);
                CtExCmd[6] = htons(ntohs(CtExCmd[6]) + 1);
                CtExCmd[13] = htons(ntohs(CtExCmd[13]) + 1);
        }
#endif

}
#endif

#if 0
void ipsec_standalone_init(void)
{
	int rc;
        union {
                CommandIPSecSetLifetime setlft_cmd;
                CommandIPSecSetTunnel settunnel_cmd;
                CommandIPSecSetState setstate_cmd;
        } ipscmd;
	RtCommand rt_cmd;

        //int i;

        U16 CtExCmd[56] = {ACTION_REGISTER, //register new Conntrack entry
                0x0001, // Conntrack is secure
                0xA8C0, //src IP address of querier        (192.168.1.111)
                0x6F01, //src IP address of querier
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
                0xA8C0, //dst IP address of querier (192.168.2.222)
                0xDE02, //dst IP address of querier
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
                0x1122, //src port number of querier ()
                0x3344, //dst port number of querier ()
                0xA8C0, //src IP address of replier (192.168.32.222)
                0xDE02, //src IP address of replier
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
                0xA8C0, //dst IP address of replier (192.168.31.111)
                0x6F01, //dst IP address of replier
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
                0x3344, //src port number of replier ()
                0x1122, //dst port number of replier ()
                0x0011,  //protocol number (UDP=17)
                0x0000, // pad
                0x0000, // fwmark
                0x0000, // fwmark
                0x0011, // routeid
                0x0000, // routeid
                0x0012, // reply routeid
                0x0000, // reply routeid
                1, // Sa_nr
                SAGD_4_IN, // sa_handle
                0x0,0x0,0x0, //sa_handle
                1, //sa_reply_nr
                SAGD_4_OUT, // sa_handle
                0x0,0x0,0x0, //sa_handle
        };
#ifdef CONTROL_IPSEC_DEBUG
	printk(KERN_INFO "%s called..\n", __func__);
#endif

        /* First SA */
        IPsec_handle_CREATE_SA((U16*) &create_cmd_v6, sizeof(create_cmd));
        IPsec_handle_SA_SET_KEYS((U16*) &setkey_cmd, sizeof(setkey_cmd));
        memset(&ipscmd.settunnel_cmd,0,sizeof(ipscmd.settunnel_cmd));
        ipscmd.settunnel_cmd.sagd=SAGD_4_OUT;
        ipscmd.settunnel_cmd.proto_family=PROTO_FAMILY_IPV6;
        ipscmd.settunnel_cmd.h.ipv6h.Version_TC_FLHi = htons(0x6000);
        ipscmd.settunnel_cmd.h.ipv6h.FlowLabelLo = 0;
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[1] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[2] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[3] = htonl(0x1);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[1] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[2] = htonl(0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[3] = htonl(0x2);

        IPsec_handle_SA_SET_TUNNEL((U16*) &ipscmd.settunnel_cmd, sizeof(ipscmd.settunnel_cmd));
        memset(&ipscmd.setlft_cmd,0,sizeof(ipscmd.setlft_cmd));
        ipscmd.setlft_cmd.sagd = SAGD_4_OUT;
        IPsec_handle_SA_SET_LIFETIME((U16*) &ipscmd.setlft_cmd, sizeof(ipscmd.setlft_cmd));
        memset(&ipscmd.setstate_cmd,0,sizeof(ipscmd.setstate_cmd));
        ipscmd.setstate_cmd.sagd = SAGD_4_OUT;
        ipscmd.setstate_cmd.state = XFRM_STATE_VALID;

        IPsec_handle_SA_SET_STATE((U16*) &ipscmd.setstate_cmd, sizeof(ipscmd.setstate_cmd));
        /* Second SA */
        IPsec_handle_CREATE_SA((U16*) &create_cmd_1_v6, sizeof(create_cmd_1));
        IPsec_handle_SA_SET_KEYS((U16*) &setkey_cmd_1, sizeof(setkey_cmd_1));
        memset(&ipscmd.settunnel_cmd,0,sizeof(ipscmd.settunnel_cmd));
        ipscmd.settunnel_cmd.sagd=SAGD_4_IN;
        ipscmd.settunnel_cmd.proto_family=PROTO_FAMILY_IPV6;
        ipscmd.settunnel_cmd.h.ipv6h.Version_TC_FLHi = htons(0x6000);
        ipscmd.settunnel_cmd.h.ipv6h.FlowLabelLo = 0;
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[1] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[2] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.SourceAddress[3] = htonl(0x2);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[0] = htonl(0x20010000);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[1] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[2] = htonl(0x0);
        ipscmd.settunnel_cmd.h.ipv6h.DestinationAddress[3] = htonl(0x1);
        IPsec_handle_SA_SET_TUNNEL((U16*) &ipscmd.settunnel_cmd, sizeof(ipscmd.settunnel_cmd));
        memset(&ipscmd.setlft_cmd,0,sizeof(ipscmd.setlft_cmd));
        ipscmd.setlft_cmd.sagd = SAGD_4_IN;
        IPsec_handle_SA_SET_LIFETIME((U16*) &ipscmd.setlft_cmd, sizeof(ipscmd.setlft_cmd));
        memset(&ipscmd.setstate_cmd,0,sizeof(ipscmd.setstate_cmd));
        ipscmd.setstate_cmd.sagd = SAGD_4_IN;
        ipscmd.setstate_cmd.state = XFRM_STATE_VALID;
        IPsec_handle_SA_SET_STATE((U16*) &ipscmd.setstate_cmd, sizeof(ipscmd.setstate_cmd));

	rt_cmd.action = ACTION_REGISTER;
	rt_cmd.mtu = 1500;
	rt_cmd.macAddr[0] = 0x00;
	rt_cmd.macAddr[1] = 0x1A;
	rt_cmd.macAddr[2] = 0xBB;
	rt_cmd.macAddr[3] = 0x1F;
	rt_cmd.macAddr[4] = 0xEE;
	rt_cmd.macAddr[5] = 0xCD;
	memcpy(rt_cmd.outputDevice , wan_if, 12);
	rt_cmd.id = 0x11;
	rt_cmd.flags = 0;
	rt_cmd.daddr[3] = 0x33330000;
	rt_cmd.daddr[0] =0;
	rt_cmd.daddr[1] = 0;
	rt_cmd.daddr[2] = 2;

	rc = IP_HandleIP_ROUTE_RESOLVE((U16*)&rt_cmd, sizeof(rt_cmd));

	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding orig route entry %d\n", rc);
		return;
	}

	printk(KERN_INFO "Originator Route entry added successfully :%d\n",rc);

	rt_cmd.action = ACTION_REGISTER;
	rt_cmd.mtu = 1500;
	rt_cmd.macAddr[0] = 0x00;
	rt_cmd.macAddr[1] = 0x02;
	rt_cmd.macAddr[2] = 0x03;
	rt_cmd.macAddr[3] = 0x04;
	rt_cmd.macAddr[4] = 0x05;
	rt_cmd.macAddr[5] = 0x06;
	memcpy(rt_cmd.outputDevice , lan_if, 12);
	rt_cmd.id = 0x12;
	rt_cmd.flags = 0;
	rt_cmd.daddr[0] = 0x11110000;
	rt_cmd.daddr[1] = 0;
	rt_cmd.daddr[2] = 0;
	rt_cmd.daddr[3] = 2;

	rc = IP_HandleIP_ROUTE_RESOLVE((U16*)&rt_cmd, sizeof(rt_cmd));
	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding replier route entry %d\n", rc);
		return;
	}
	printk(KERN_INFO "Replier Route entry added successfully :%d\n",rc);

	rc = IPv6_handle_CONNTRACK(CtExCmd, 56*2);

	if (rc != NO_ERR)
	{
		printk (KERN_ERR "Error in adding conntrack entry %d\n", rc);
		return;
	}
#if 0
        for(i=0;i<4;i++) {
                IPv4_HandleIP_CONNTRACK(CtExCmd, 28*2);
                CtExCmd[6] = htons(ntohs(CtExCmd[6]) + 1);
                CtExCmd[13] = htons(ntohs(CtExCmd[13]) + 1);
        }
#endif

}
#endif


