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


#include "fpp.h"
#include "modules.h"
#include "channels.h"
#include "events.h"
#include "system.h"
#include "fpart.h"
#include "fe.h"
#include "icc.h"
#include "module_hidrv.h"

//#define ICC_DEBUG

#ifdef CFG_ICC
#ifdef ICC_DEBUG
#define FUNCTION_ENTRY() do { printk(KERN_INFO "%s: function entered\n", __func__); } while (0)
#define DUMP_ICCTAB() dump_icctab()
#else
#define FUNCTION_ENTRY() do { } while (0)
#define DUMP_ICCTAB() do { } while (0)
#endif

#ifdef ICC_DEBUG
static void dump_icctab(void)
{
	int i;
	int x;
	char buf[256];
	char buf1[128], buf2[128];
	printk(KERN_INFO "ICCTAB:\n");
	printk(KERN_INFO "bmu1_thresh=%d, bmu2_thresh=%d\n", be32_to_cpu(icctab.bmu1_threshold), be32_to_cpu(icctab.bmu2_threshold));
	for (x = 0; x < GEM_PORTS; x++)
	{
		printk(KERN_INFO "**** Interface %d ****\n", x);
		buf[0] = '\0';
		for (i = 0; i < 256 / 8; i++)
			sprintf(buf + strlen(buf), "%02x", icctab.ipproto[x][i]);
		printk(KERN_INFO "ipproto: %s\n", buf);
		buf[0] = '\0';
		for (i = 0; i < 64 / 8; i++)
			sprintf(buf + strlen(buf), "%02x", icctab.dscp[x][i]);
		printk(KERN_INFO "dscp: %s\n", buf);
		buf[0] = '\0';
		for (i = 0; i < icctab.num_ethertype[x]; i++)
			sprintf(buf + strlen(buf), "%04x ", be16_to_cpu(icctab.ethertype[x][i]));
		printk(KERN_INFO "ethertype(%d) %s\n", icctab.num_ethertype[x], buf);
		buf[0] = '\0';
		for (i = 0; i < icctab.num_ipv4addr[x]; i++)
		{
			sprintf(buf1, "%x", be32_to_cpu(icctab.ipv4addr[x][i].addr));
			sprintf(buf2, "%d", icctab.ipv4addr[x][i].masklen);
			sprintf(buf + strlen(buf), "%s/%s/%d ", buf1, buf2, icctab.ipv4addr[x][i].src_dst_flag);
		}
		printk(KERN_INFO "ipv4(%d) %s\n", icctab.num_ipv4addr[x], buf);
		buf[0] = '\0';
		for (i = 0; i < icctab.num_ipv6addr[x]; i++)
		{
			int j;
			buf1[0] = buf2[0] = '\0';
			for (j = 0; j < 4; j++)
			{
				sprintf(buf1 + strlen(buf1), "%x", be32_to_cpu(icctab.ipv6addr[x][i].addr[j]));
				if (j < 3)
					strcat(buf1, ":");
			}
			sprintf(buf2, "%d", icctab.ipv6addr[x][i].masklen);
			sprintf(buf + strlen(buf), "%s/%s/%d ", buf1, buf2, icctab.ipv6addr[x][i].src_dst_flag);
		}
		printk(KERN_INFO "ipv6(%d) %s\n", icctab.num_ipv6addr[x], buf);
		buf[0] = '\0';
		for (i = 0; i < icctab.num_port[x]; i++)
			sprintf(buf + strlen(buf), "%d-%d/%d-%d ",
						be16_to_cpu(icctab.port[x][i].sport_from),
						be16_to_cpu(icctab.port[x][i].sport_to),
						be16_to_cpu(icctab.port[x][i].dport_from),
						be16_to_cpu(icctab.port[x][i].dport_to));
		printk(KERN_INFO "port(%d) %s\n", icctab.num_port[x], buf);
		buf[0] = '\0';
		for (i = 0; i < icctab.num_vlan[x]; i++)
			sprintf(buf + strlen(buf), "%d-%d/%d-%d ",
						be16_to_cpu(icctab.vlan[x][i].vlan_from),
						be16_to_cpu(icctab.vlan[x][i].vlan_to),
						be16_to_cpu(icctab.vlan[x][i].prio_from),
						be16_to_cpu(icctab.vlan[x][i].prio_to));
		printk(KERN_INFO "vlan(%d) %s\n", icctab.num_vlan[x], buf);
	}
}
#endif

static void icc_update_class_pe(void)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	int id;

	FUNCTION_ENTRY();
	DUMP_ICCTAB();

	if (pe_sync_stop(ctrl, CLASS_MASK) < 0)
	{
		printk(KERN_ERR "Error updating ICC tables\n");
		return;
	}

	/* update the DMEM in class-pe */
	for (id = CLASS0_ID; id <= CLASS_MAX_ID; id++)
	{
		pe_dmem_memcpy_to32(id, virt_to_class_dmem(&icctab), &icctab, sizeof(icctab));
	}

	pe_start(ctrl, CLASS_MASK);
}

static void icc_reset(void)
{
	int i;

	FUNCTION_ENTRY();

	memset(&icctab, 0, sizeof(icctab));
	icctab.bmu1_threshold = cpu_to_be32(64);
	icctab.bmu2_threshold = cpu_to_be32(128);
	for (i = 0; i < GEM_PORTS; i++)
	{
		setbit_in_array(icctab.ipproto[i], IPPROTOCOL_ICMP, 1);
		setbit_in_array(icctab.ipproto[i], IPPROTOCOL_IGMP, 1);
		setbit_in_array(icctab.ipproto[i], IPPROTOCOL_ICMPV6, 1);
	}
	icc_update_class_pe();
}

static U16 icc_threshold(icc_threshold_cmd_t *pcmd)
{
	FUNCTION_ENTRY();

	if (pcmd->bmu1_threshold > MAX_BMU1_THRESHOLD || pcmd->bmu2_threshold > MAX_BMU2_THRESHOLD)
		return ERR_ICC_THRESHOLD_OUT_OF_RANGE;
	icctab.bmu1_threshold = cpu_to_be32(pcmd->bmu1_threshold);
	icctab.bmu2_threshold = cpu_to_be32(pcmd->bmu2_threshold);
	icc_update_class_pe();
	return NO_ERR;
}

static U16 icc_add(icc_add_delete_cmd_t *pcmd)
{
	int i, n;
	U32 interface;
	U16 rtncode;

	FUNCTION_ENTRY();

	interface = pcmd->interface;
	if (interface >= GEM_PORTS)
		return ERR_UNKNOWN_INTERFACE;
	switch (pcmd->table_type)
	{
		case ICC_TABLETYPE_ETHERTYPE:
		{
			U16 ethertype;
			n = icctab.num_ethertype[interface];
			if (n == MAX_ICC_ETHERTYPE)
				return ERR_ICC_TOO_MANY_ENTRIES;
			ethertype = cpu_to_be16(pcmd->ethertype.type);
			for (i = 0; i < n; i++)
			{
				if (icctab.ethertype[interface][i] == ethertype)
					return ERR_ICC_ENTRY_ALREADY_EXISTS;
			}
			icctab.ethertype[interface][n] = ethertype;
			icctab.num_ethertype[interface]++;
			break;
		}
		case ICC_TABLETYPE_PROTOCOL:
		{
			for (i = 0; i < 256 / 8; i++)
				icctab.ipproto[interface][i] |= pcmd->protocol.ipproto[i];
			break;
		}
		case ICC_TABLETYPE_DSCP:
		{
			for (i = 0; i < 64 / 8; i++)
				icctab.dscp[interface][i] |= pcmd->dscp.dscp_value[i];
			break;
		}
		case ICC_TABLETYPE_SADDR:
		case ICC_TABLETYPE_DADDR:
		{
			U8 src_dst_flag;
			U32 addr;
			U8 masklen;
			n = icctab.num_ipv4addr[interface];
			if (n == MAX_ICC_IPV4ADDR)
				return ERR_ICC_TOO_MANY_ENTRIES;
			src_dst_flag = pcmd->table_type == ICC_TABLETYPE_SADDR ? 0 : 1;
			addr = pcmd->ipaddr.v4_addr;
			masklen = pcmd->ipaddr.v4_masklen;
			if (masklen == 0 || masklen > 32)
				return ERR_ICC_INVALID_MASKLEN;
			for (i = 0; i < n; i++)
			{
				if (icctab.ipv4addr[interface][i].src_dst_flag == src_dst_flag &&
						icctab.ipv4addr[interface][i].addr == addr &&
						icctab.ipv4addr[interface][i].masklen == masklen)
					return ERR_ICC_ENTRY_ALREADY_EXISTS;
			}
			icctab.ipv4addr[interface][n].src_dst_flag = src_dst_flag;
			icctab.ipv4addr[interface][n].addr = addr;
			icctab.ipv4addr[interface][n].masklen = masklen;
			icctab.num_ipv4addr[interface]++;
			break;
		}
		case ICC_TABLETYPE_SADDR6:
		case ICC_TABLETYPE_DADDR6:
		{
			U8 src_dst_flag;
			U32 *addr;
			U8 masklen;
			n = icctab.num_ipv6addr[interface];
			if (n == MAX_ICC_IPV6ADDR)
				return ERR_ICC_TOO_MANY_ENTRIES;
			src_dst_flag = pcmd->table_type == ICC_TABLETYPE_SADDR6 ? 0 : 1;
			addr = pcmd->ipv6addr.v6_addr;
			masklen = pcmd->ipv6addr.v6_masklen;
			if (masklen == 0 || masklen > 128)
				return ERR_ICC_INVALID_MASKLEN;
			for (i = 0; i < n; i++)
			{
				if (icctab.ipv6addr[interface][i].src_dst_flag == src_dst_flag &&
						icctab.ipv6addr[interface][i].addr[0] == addr[0] &&
						icctab.ipv6addr[interface][i].addr[1] == addr[1] &&
						icctab.ipv6addr[interface][i].addr[2] == addr[2] &&
						icctab.ipv6addr[interface][i].addr[3] == addr[3] &&
						icctab.ipv6addr[interface][i].masklen == masklen)
					return ERR_ICC_ENTRY_ALREADY_EXISTS;
			}
			icctab.ipv6addr[interface][n].src_dst_flag = src_dst_flag;
			icctab.ipv6addr[interface][n].addr[0] = addr[0];
			icctab.ipv6addr[interface][n].addr[1] = addr[1];
			icctab.ipv6addr[interface][n].addr[2] = addr[2];
			icctab.ipv6addr[interface][n].addr[3] = addr[3];
			icctab.ipv6addr[interface][n].masklen = masklen;
			icctab.num_ipv6addr[interface]++;
			break;
		}
		case ICC_TABLETYPE_PORT:
		{
			U16 sport_from, sport_to, dport_from, dport_to;
			n = icctab.num_port[interface];
			if (n == MAX_ICC_PORT)
				return ERR_ICC_TOO_MANY_ENTRIES;
			sport_from = cpu_to_be16(pcmd->port.sport_from);
			sport_to = cpu_to_be16(pcmd->port.sport_to);
			dport_from = cpu_to_be16(pcmd->port.dport_from);
			dport_to = cpu_to_be16(pcmd->port.dport_to);
			for (i = 0; i < n; i++)
			{
				if (icctab.port[interface][i].sport_from == sport_from &&
						icctab.port[interface][i].sport_to == sport_to &&
						icctab.port[interface][i].dport_from == dport_from &&
						icctab.port[interface][i].dport_to == dport_to)
					return ERR_ICC_ENTRY_ALREADY_EXISTS;
			}
			icctab.port[interface][n].sport_from = sport_from;
			icctab.port[interface][n].sport_to = sport_to;
			icctab.port[interface][n].dport_from = dport_from;
			icctab.port[interface][n].dport_to = dport_to;
			icctab.num_port[interface]++;
			break;
		}
		case ICC_TABLETYPE_VLAN:
		{
			U16 vlan_from, vlan_to, prio_from, prio_to;
			n = icctab.num_vlan[interface];
			if (n == MAX_ICC_VLAN)
				return ERR_ICC_TOO_MANY_ENTRIES;
			vlan_from = cpu_to_be16(pcmd->vlan.vlan_from);
			vlan_to = cpu_to_be16(pcmd->vlan.vlan_to);
			prio_from = cpu_to_be16(pcmd->vlan.prio_from);
			prio_to = cpu_to_be16(pcmd->vlan.prio_to);
			for (i = 0; i < n; i++)
			{
				if (icctab.vlan[interface][i].vlan_from == vlan_from &&
						icctab.vlan[interface][i].vlan_to == vlan_to &&
						icctab.vlan[interface][i].prio_from == prio_from &&
						icctab.vlan[interface][i].prio_to == prio_to)
					return ERR_ICC_ENTRY_ALREADY_EXISTS;
			}
			icctab.vlan[interface][n].vlan_from = vlan_from;
			icctab.vlan[interface][n].vlan_to = vlan_to;
			icctab.vlan[interface][n].prio_from = prio_from;
			icctab.vlan[interface][n].prio_to = prio_to;
			icctab.num_vlan[interface]++;
			break;
		}
	}
	icc_update_class_pe();
	rtncode = NO_ERR;
	return rtncode;
}

static U16 icc_delete(icc_add_delete_cmd_t *pcmd)
{
	int i, n;
	U32 interface;
	U16 rtncode;

	FUNCTION_ENTRY();

	interface = pcmd->interface;
	if (interface >= GEM_PORTS)
		return ERR_UNKNOWN_INTERFACE;
	switch (pcmd->table_type)
	{
		case ICC_TABLETYPE_ETHERTYPE:
		{
			U16 ethertype;
			n = icctab.num_ethertype[interface];
			ethertype = cpu_to_be16(pcmd->ethertype.type);
			for (i = 0; i < n; i++)
			{
				if (icctab.ethertype[interface][i] == ethertype)
					break;
			}
			if (i == n)
				return ERR_ICC_ENTRY_NOT_FOUND;
			for (i++; i < n; i++)
			{
				icctab.ethertype[interface][i - 1] = icctab.ethertype[interface][i];
			}
			icctab.num_ethertype[interface]--;
			break;
		}
		case ICC_TABLETYPE_PROTOCOL:
		{
			for (i = 0; i < 256 / 8; i++)
				icctab.ipproto[interface][i] &= ~pcmd->protocol.ipproto[i];
			break;
		}
		case ICC_TABLETYPE_DSCP:
		{
			for (i = 0; i < 64 / 8; i++)
				icctab.dscp[interface][i] &= ~pcmd->dscp.dscp_value[i];
			break;
		}
		case ICC_TABLETYPE_SADDR:
		case ICC_TABLETYPE_DADDR:
		{
			U8 src_dst_flag;
			U32 addr;
			U8 masklen;
			n = icctab.num_ipv4addr[interface];
			src_dst_flag = pcmd->table_type == ICC_TABLETYPE_SADDR ? 0 : 1;
			addr = pcmd->ipaddr.v4_addr;
			masklen = pcmd->ipaddr.v4_masklen;
			for (i = 0; i < n; i++)
			{
				if (icctab.ipv4addr[interface][i].src_dst_flag == src_dst_flag &&
						icctab.ipv4addr[interface][i].addr == addr &&
						icctab.ipv4addr[interface][i].masklen == masklen)
					break;
			}
			if (i == n)
				return ERR_ICC_ENTRY_NOT_FOUND;
			for (i++; i < n; i++)
			{
				icctab.ipv4addr[interface][i - 1] = icctab.ipv4addr[interface][i];
			}
			icctab.num_ipv4addr[interface]--;
			break;
		}
		case ICC_TABLETYPE_SADDR6:
		case ICC_TABLETYPE_DADDR6:
		{
			U8 src_dst_flag;
			U32 *addr;
			U8 masklen;
			n = icctab.num_ipv6addr[interface];
			src_dst_flag = pcmd->table_type == ICC_TABLETYPE_SADDR6 ? 0 : 1;
			addr = pcmd->ipv6addr.v6_addr;
			masklen = pcmd->ipv6addr.v6_masklen;
			for (i = 0; i < n; i++)
			{
				if (icctab.ipv6addr[interface][i].src_dst_flag == src_dst_flag &&
						icctab.ipv6addr[interface][i].addr[0] == addr[0] &&
						icctab.ipv6addr[interface][i].addr[1] == addr[1] &&
						icctab.ipv6addr[interface][i].addr[2] == addr[2] &&
						icctab.ipv6addr[interface][i].addr[3] == addr[3] &&
						icctab.ipv6addr[interface][i].masklen == masklen)
					break;
			}
			if (i == n)
				return ERR_ICC_ENTRY_NOT_FOUND;
			for (i++; i < n; i++)
			{
				icctab.ipv6addr[interface][i - 1] = icctab.ipv6addr[interface][i];
			}
			icctab.num_ipv6addr[interface]--;
			break;
		}
		case ICC_TABLETYPE_PORT:
		{
			U16 sport_from, sport_to, dport_from, dport_to;
			n = icctab.num_port[interface];
			sport_from = cpu_to_be16(pcmd->port.sport_from);
			sport_to = cpu_to_be16(pcmd->port.sport_to);
			dport_from = cpu_to_be16(pcmd->port.dport_from);
			dport_to = cpu_to_be16(pcmd->port.dport_to);
			for (i = 0; i < n; i++)
			{
				if (icctab.port[interface][i].sport_from == sport_from &&
						icctab.port[interface][i].sport_to == sport_to &&
						icctab.port[interface][i].dport_from == dport_from &&
						icctab.port[interface][i].dport_to == dport_to)
					break;
			}
			if (i == n)
				return ERR_ICC_ENTRY_NOT_FOUND;
			for (i++; i < n; i++)
			{
				icctab.port[interface][i - 1] = icctab.port[interface][i];
			}
			icctab.num_port[interface]--;
			break;
		}
		case ICC_TABLETYPE_VLAN:
		{
			U16 vlan_from, vlan_to, prio_from, prio_to;
			n = icctab.num_vlan[interface];
			vlan_from = cpu_to_be16(pcmd->vlan.vlan_from);
			vlan_to = cpu_to_be16(pcmd->vlan.vlan_to);
			prio_from = cpu_to_be16(pcmd->vlan.prio_from);
			prio_to = cpu_to_be16(pcmd->vlan.prio_to);
			for (i = 0; i < n; i++)
			{
				if (icctab.vlan[interface][i].vlan_from == vlan_from &&
						icctab.vlan[interface][i].vlan_to == vlan_to &&
						icctab.vlan[interface][i].prio_from == prio_from &&
						icctab.vlan[interface][i].prio_to == prio_to)
					break;
			}
			if (i == n)
				return ERR_ICC_ENTRY_NOT_FOUND;
			for (i++; i < n; i++)
			{
				icctab.vlan[interface][i - 1] = icctab.vlan[interface][i];
			}
			icctab.num_vlan[interface]--;
			break;
		}
	}
	icc_update_class_pe();
	rtncode = NO_ERR;
	return rtncode;
}

static U16 icc_query(icc_query_cmd_t *pcmd)
{
	int i;
	U16 action;
	U32 interface;
	U16 query_result = 0;
	icc_query_reply_t *prsp;
	U8 flag;
	static U8 table_type = 0xFF, table_index;

	FUNCTION_ENTRY();

	action = pcmd->action;
	interface = pcmd->interface;
	if (interface >= GEM_PORTS)
		return ERR_UNKNOWN_INTERFACE;

	prsp = (icc_query_reply_t *)pcmd;
	memset(prsp, 0, sizeof(icc_query_reply_t));
	prsp->interface = interface;
	if (action == 0 || table_type == 0xFF)
	{
		table_type = 0;
		table_index = 0;
	}
	while (1)
	{
		if (table_type >= ICC_MAX_TABLETYPE)
		{
			query_result = 1;
			table_type = 0xFF;
			goto done;
		}
		prsp->table_type = table_type;
		switch(table_type)
		{
			case ICC_TABLETYPE_ETHERTYPE:
			{
				if (table_index < icctab.num_ethertype[interface])
				{
					prsp->ethertype.type = be16_to_cpu(icctab.ethertype[interface][table_index]);
					goto done;
				}
				break;
			}
			case ICC_TABLETYPE_PROTOCOL:
			{
				if (table_index == 0)
				{
					flag = 0;
					for (i = 0; i < 256 / 8; i++)
					{
						prsp->protocol.ipproto[i] = icctab.ipproto[interface][i];
						flag |= prsp->protocol.ipproto[i];
					}
					if (flag)
						goto done;
				}
				break;
			}
			case ICC_TABLETYPE_DSCP:
			{
				if (table_index == 0)
				{
					flag = 0;
					for (i = 0; i < 64 / 8; i++)
					{
						prsp->dscp.dscp_value[i] = icctab.dscp[interface][i];
						flag |= prsp->dscp.dscp_value[i];
					}
					if (flag)
						goto done;
				}
				break;
			}
			case ICC_TABLETYPE_SADDR:
			{
				while (table_index < icctab.num_ipv4addr[interface])
				{
					if (icctab.ipv4addr[interface][table_index].src_dst_flag == 0)
					{
						prsp->ipaddr.v4_addr = icctab.ipv4addr[interface][table_index].addr;
						prsp->ipaddr.v4_masklen = icctab.ipv4addr[interface][table_index].masklen;
						goto done;
					}
					table_index++;
				}
				break;
			}
			case ICC_TABLETYPE_DADDR:
			{
				while (table_index < icctab.num_ipv4addr[interface])
				{
					if (icctab.ipv4addr[interface][table_index].src_dst_flag != 0)
					{
						prsp->ipaddr.v4_addr = icctab.ipv4addr[interface][table_index].addr;
						prsp->ipaddr.v4_masklen = icctab.ipv4addr[interface][table_index].masklen;
						goto done;
					}
					table_index++;
				}
				break;
			}
			case ICC_TABLETYPE_SADDR6:
			{
				while (table_index < icctab.num_ipv6addr[interface])
				{
					if (icctab.ipv6addr[interface][table_index].src_dst_flag == 0)
					{
						prsp->ipv6addr.v6_addr[0] = icctab.ipv6addr[interface][table_index].addr[0];
						prsp->ipv6addr.v6_addr[1] = icctab.ipv6addr[interface][table_index].addr[1];
						prsp->ipv6addr.v6_addr[2] = icctab.ipv6addr[interface][table_index].addr[2];
						prsp->ipv6addr.v6_addr[3] = icctab.ipv6addr[interface][table_index].addr[3];
						prsp->ipv6addr.v6_masklen = icctab.ipv6addr[interface][table_index].masklen;
						goto done;
					}
					table_index++;
				}
				break;
			}
			case ICC_TABLETYPE_DADDR6:
			{
				while (table_index < icctab.num_ipv6addr[interface])
				{
					if (icctab.ipv6addr[interface][table_index].src_dst_flag != 0)
					{
						prsp->ipv6addr.v6_addr[0] = icctab.ipv6addr[interface][table_index].addr[0];
						prsp->ipv6addr.v6_addr[1] = icctab.ipv6addr[interface][table_index].addr[1];
						prsp->ipv6addr.v6_addr[2] = icctab.ipv6addr[interface][table_index].addr[2];
						prsp->ipv6addr.v6_addr[3] = icctab.ipv6addr[interface][table_index].addr[3];
						prsp->ipv6addr.v6_masklen = icctab.ipv6addr[interface][table_index].masklen;
						goto done;
					}
					table_index++;
				}
				break;
			}
			case ICC_TABLETYPE_PORT:
			{
				if (table_index < icctab.num_port[interface])
				{
					prsp->port.sport_from = be16_to_cpu(icctab.port[interface][table_index].sport_from);
					prsp->port.sport_to = be16_to_cpu(icctab.port[interface][table_index].sport_to);
					prsp->port.dport_from = be16_to_cpu(icctab.port[interface][table_index].dport_from);
					prsp->port.dport_to = be16_to_cpu(icctab.port[interface][table_index].dport_to);
					goto done;
				}
				break;
			}
			case ICC_TABLETYPE_VLAN:
			{
				if (table_index < icctab.num_vlan[interface])
				{
					prsp->vlan.vlan_from = be16_to_cpu(icctab.vlan[interface][table_index].vlan_from);
					prsp->vlan.vlan_to = be16_to_cpu(icctab.vlan[interface][table_index].vlan_to);
					prsp->vlan.prio_from = be16_to_cpu(icctab.vlan[interface][table_index].prio_from);
					prsp->vlan.prio_to = be16_to_cpu(icctab.vlan[interface][table_index].prio_to);
					goto done;
				}
				break;
			}
		}
		table_type++;
		table_index = 0;
	}

done:
	table_index++;
	prsp->query_result = query_result;
	return NO_ERR;
}

static U16 M_icc_cmdproc(U16 cmd_code, U16 cmd_len, U16 *p)
{
	U16 rtncode;
	U16 retlen = 2;

	FUNCTION_ENTRY();

	rtncode = NO_ERR;
	switch (cmd_code)
	{
		case CMD_ICC_RESET:
		{
			icc_reset();
			break;
		}

		case CMD_ICC_THRESHOLD:
		{
			icc_threshold_cmd_t *pcmd = (icc_threshold_cmd_t *)p;
			rtncode = icc_threshold(pcmd);
			break;
		}

		case CMD_ICC_ADD_DELETE:
		{
			icc_add_delete_cmd_t *pcmd = (icc_add_delete_cmd_t *)p;
			if (pcmd->action == ICC_ACTION_ADD)
			{
				rtncode = icc_add(pcmd);
			}
			else if (pcmd->action == ICC_ACTION_DELETE)
			{
				rtncode = icc_delete(pcmd);
			}
			else
			{
				rtncode = ERR_UNKNOWN_ACTION;
			}
			break;
		}

		case CMD_ICC_QUERY:
		{
			icc_query_cmd_t *pcmd = (icc_query_cmd_t *)p;
			rtncode = icc_query(pcmd);
			retlen = sizeof(icc_query_reply_t);
			break;
		}

		// unknown command code
		default:
		{
			rtncode = ERR_UNKNOWN_COMMAND;
			break;
		}
	}

	*p = rtncode;
	return retlen;
}

/** ICC init function.
 * This function initializes the ICC control context with default configuration
 * and sends the same configuration to each class PE.
 *
 */
int icc_control_init(void)
{
	int rtn_code = NO_ERR;

	FUNCTION_ENTRY();

	set_cmd_handler(EVENT_ICC, M_icc_cmdproc);

	icc_reset();

	return rtn_code;
}

/** ICC exit function.
*/
void icc_control_exit(void)
{

	FUNCTION_ENTRY();

}
#endif
