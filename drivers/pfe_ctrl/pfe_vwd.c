/*
 *
 *  Copyright (C) 2007 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/version.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#include <net/pkt_sched.h>
#include <linux/rcupdate.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter_bridge.h>
#include <linux/irqnr.h>
#include <linux/ppp_defs.h>

#include <linux/rculist.h>
#include <../../../net/bridge/br_private.h>

#ifndef CONFIG_PFE_WIFI_OFFLOAD
#define original_dev_queue_xmit dev_queue_xmit
#endif

#if defined(CONFIG_INET_IPSEC_OFFLOAD) || defined(CONFIG_INET6_IPSEC_OFFLOAD)
#include <net/xfrm.h>
#endif

#include "pfe_mod.h"
#include "pfe_tso.h"
#include "pfe_vwd.h"

#if defined(CONFIG_INET_IPSEC_OFFLOAD) || defined(CONFIG_INET6_IPSEC_OFFLOAD)
#include <net/xfrm.h>
#endif

#ifdef CFG_WIFI_OFFLOAD

//#define VWD_DEBUG
#define PFE_VWD_BHR_MODE 0
#define PFE_VWD_NAS_MODE 1
#define PFE_VWD_BHR_NAS_MODE 2
unsigned int vwd_ofld = PFE_VWD_BHR_MODE;
module_param(vwd_ofld, uint, S_IRUGO);
MODULE_PARM_DESC(vwd_ofld,
                 "0: VWD in BHR mode, 1: VWD in NAS mode, 2: VWD in BHR+NAS mode");

static int pfe_vwd_rx_low_poll(struct napi_struct *napi, int budget);
static int pfe_vwd_rx_high_poll(struct napi_struct *napi, int budget);
static void pfe_vwd_sysfs_exit(void);
static void pfe_vwd_vap_down(struct pfe_vwd_priv_s *priv, struct vap_desc_s *vap);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
static unsigned int pfe_vwd_nf_route_hook_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
	       const struct nf_hook_state *state);
static unsigned int pfe_vwd_nf_bridge_hook_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
	       const struct nf_hook_state *state);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
static unsigned int pfe_vwd_nf_route_hook_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *));
static unsigned int pfe_vwd_nf_bridge_hook_fn( const struct nf_hook_ops *ops, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *));
#else
static unsigned int pfe_vwd_nf_route_hook_fn( unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *));
static unsigned int pfe_vwd_nf_bridge_hook_fn( unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *));
#endif
static int pfe_vwd_handle_vap( struct pfe_vwd_priv_s *vwd, struct vap_cmd_s *cmd );
static int pfe_vwd_event_handler(void *data, int event, int qno);
static void pfe_vwd_vap_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *drvinfo);
#ifdef CONFIG_PFE_WIFI_OFFLOAD
extern int comcerto_wifi_rx_fastpath_register(int (*hdlr)(struct sk_buff *skb));
extern void comcerto_wifi_rx_fastpath_unregister(void);
#endif

extern unsigned int page_mode;
#if defined(CONFIG_INET_IPSEC_OFFLOAD) || defined(CONFIG_INET6_IPSEC_OFFLOAD)
extern struct xfrm_state *xfrm_state_lookup_byhandle(struct net *net, u16 handle);
#endif


struct ethtool_ops pfe_vwd_vap_ethtool_ops = {
	.get_drvinfo = pfe_vwd_vap_get_drvinfo,
};

/* IPV4 route hook , recieve the packet and forward to VWD driver*/
static struct nf_hook_ops vwd_hook = {
	.hook = pfe_vwd_nf_route_hook_fn,
	.pf = PF_INET,
	.hooknum = NF_INET_PRE_ROUTING,
	.priority = NF_IP_PRI_FIRST,
};

/* IPV6 route hook , recieve the packet and forward to VWD driver*/
static struct nf_hook_ops vwd_hook_ipv6 = {
	.hook = pfe_vwd_nf_route_hook_fn,
	.pf = PF_INET6,
	.hooknum = NF_INET_PRE_ROUTING,
	.priority = NF_IP6_PRI_FIRST,
};

/* Bridge hook , recieve the packet and forward to VWD driver*/
static struct nf_hook_ops vwd_hook_bridge = {
	.hook = pfe_vwd_nf_bridge_hook_fn,
	.pf = PF_BRIDGE,
	.hooknum = NF_BR_PRE_ROUTING,
	.priority = NF_BR_PRI_FIRST,
};

struct pfe_vwd_priv_s glbl_pfe_vwd_priv;

#ifdef VWD_DEBUG
static void pfe_vwd_dump_skb( struct sk_buff *skb )
{
	int i;

	for (i = 0; i < skb->len; i++)
	{
		if (!(i % 16))
			printk("\n");

		printk(" %02x", skb->data[i]);
	}
}
#endif

/** get_vap_by_name
 *
 */
static struct vap_desc_s *get_vap_by_name(struct pfe_vwd_priv_s *priv, const char *name)
{
	int ii;
	struct vap_desc_s *vap = NULL;

	for (ii = 0; ii < MAX_VAP_SUPPORT; ii++) {
		if (priv->vaps[ii].state == VAP_ST_CLOSE)
			continue;

		if (!strcmp(priv->vaps[ii].ifname, name)) {
			vap = &priv->vaps[ii];
			break;
		}
	}

	return vap;
}

/**
 * vwd_vap_device_event_notifier
 */
static int vwd_vap_device_event_notifier(struct notifier_block *unused,
                             unsigned long event, void *ptr)
{
	struct vap_cmd_s vap_cmd;
	struct vap_desc_s *vap;
	struct net_device *wifi_dev = ptr;
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int ii, vap_spin_locked = 0;

	if (!spin_is_locked(&priv->vaplock)) {
		vap_spin_locked = 1;
		spin_lock_bh(&priv->vaplock);
	}

	for (ii = 0; ii < MAX_VAP_SUPPORT; ii++) {
		//FIXME : Check if another field is required 
		if(!strcmp(priv->vaps[ii].ifname, wifi_dev->name) && (priv->vaps[ii].state != VAP_ST_CLOSE))
			break;
	}

	if (ii >= MAX_VAP_SUPPORT)
		goto done;


	vap = &priv->vaps[ii]; 

	if (vwd_ofld != PFE_VWD_NAS_MODE)
		goto done;

	switch (event) {
		case NETDEV_UP:
			printk(KERN_INFO "%s (NETDEV_UP): VAP Name : %s\n", __func__, vap->ifname);
			vap_cmd.action = ADD;
			vap_cmd.vapid = ii;
			vap_cmd.ifindex = wifi_dev->ifindex;
			strcpy(vap_cmd.ifname, wifi_dev->name);
			memcpy(vap_cmd.macaddr, wifi_dev->dev_addr, 6);
			vap_cmd.cmd_flags = 0;
			if (vap->direct_rx_path)
				vap_cmd.cmd_flags |= VAP_CMD_ENABLE_DIRECT_PATH_RX;
			if (vap->direct_tx_path)
				vap_cmd.cmd_flags |= VAP_CMD_ENABLE_DIRECT_PATH_TX;
			if (!pfe_vwd_handle_vap(priv, &vap_cmd))
				printk(KERN_INFO"%s : VAP name(%s) is UP Successfully\n", __func__, wifi_dev->name);
			break;

		case NETDEV_DOWN:
#ifdef CONFIG_PFE_WIFI_OFFLOAD
			if (!wifi_dev->wifi_offload_dev)
				goto done;

			if (!(wifi_dev->flags & IFF_UP)){
				vap_cmd.action = REMOVE;
				vap_cmd.vapid = ii;
				vap_cmd.ifindex = wifi_dev->ifindex;
				strcpy(vap_cmd.ifname, wifi_dev->name);
				memcpy(vap_cmd.macaddr, wifi_dev->dev_addr, 6);
				vap_cmd.cmd_flags = 0;
				if (vap->direct_rx_path)
					vap_cmd.cmd_flags |= VAP_CMD_ENABLE_DIRECT_PATH_RX;
				if (vap->direct_tx_path)
					vap_cmd.cmd_flags |= VAP_CMD_ENABLE_DIRECT_PATH_TX;
				if (!pfe_vwd_handle_vap(priv, &vap_cmd))
					printk(KERN_INFO"%s : VAP name(%s) is DOWN Successfully\n", __func__, wifi_dev->name);
			}
#endif
			break;

	}

done:
	if (vap_spin_locked)
		spin_unlock_bh(&priv->vaplock);

	return NOTIFY_DONE;
}


static struct notifier_block vwd_vap_notifier = {
	.notifier_call = vwd_vap_device_event_notifier,
};

/** pfe_vwd_vap_create
 *
 */
static int pfe_vwd_vap_create(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	struct net_device *wifi_dev;
	struct vap_cmd_s vap_cmd;
	struct vap_desc_s *vap = NULL;
	char name[IFNAMSIZ];
	char tmp_name[IFNAMSIZ];
	int ii, len;

	len = IFNAMSIZ - 1;

	if (len > count)
		len = count;

	memcpy(tmp_name, buf, len);
	tmp_name[len] = '\0';
	sscanf(tmp_name, "%s", name);

	rtnl_lock();
	spin_lock_bh(&priv->vaplock);
	if (get_vap_by_name(priv, name) != NULL) {
		printk("%s: VAP with same name already exist\n", __func__);
		goto done;
	}
	
	/* Configure VAP */	
	for (ii = 0; ii < MAX_VAP_SUPPORT; ii++) {
		if (priv->vaps[ii].state == VAP_ST_CLOSE)
			break;
	}

	if (ii < MAX_VAP_SUPPORT) {

		memset(&vap_cmd, 0, sizeof(struct vap_cmd_s));
		vap_cmd.action = CONFIGURE;
		vap_cmd.vapid = ii;
		strcpy(vap_cmd.ifname, name);
		vap_cmd.cmd_flags = 0;
		memcpy(priv->vaps[ii].ifname, name, IFNAMSIZ);


		if (!pfe_vwd_handle_vap(priv, &vap_cmd)) {
			printk(KERN_INFO"VAP Configured successfully\n");
			vap = &priv->vaps[ii];
		}
	}

	if (!vap)
		goto done;

	wifi_dev = dev_get_by_name(&init_net, name);
	if(wifi_dev) {
		vap_cmd.action = ADD;
		vap_cmd.vapid = vap->vapid;
		vap_cmd.ifindex = wifi_dev->ifindex;
		strcpy(vap_cmd.ifname, name);
		memcpy(vap_cmd.macaddr, wifi_dev->dev_addr, 6);
		vap_cmd.cmd_flags = 0;

		if (!pfe_vwd_handle_vap(priv, &vap_cmd)){
			printk(KERN_INFO"VAP added successfully\n");
		}

		dev_put(wifi_dev);
	}
	else {
		printk(KERN_ERR "%s: %s is not UP...\n",__func__, name);
	}

done:
	spin_unlock_bh(&priv->vaplock);
	rtnl_unlock();

	return count;
}

/** pfe_vwd_vap_reset
 *
 */
static int pfe_vwd_vap_reset(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	struct vap_cmd_s vap_cmd;

	rtnl_lock();
	spin_lock_bh(&priv->vaplock);

	memset(&vap_cmd, 0, sizeof(struct vap_cmd_s));
	vap_cmd.action = RESET;

	if (!pfe_vwd_handle_vap(priv, &vap_cmd)){
		printk(KERN_INFO"VAP reset is  successfull\n");
		//vap->ifname[0] = '\0';
	}

	spin_unlock_bh(&priv->vaplock);
	rtnl_unlock();

	return count;
}


#ifdef PFE_VWD_LRO_STATS
/*
 * pfe_vwd_show_lro_nb_stats
 */
static ssize_t pfe_vwd_show_lro_nb_stats(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	ssize_t len = 0;
	int i;

	for (i = 0; i < LRO_NB_COUNT_MAX; i++)
		len += sprintf(buf + len, "%d fragments packets = %d\n", i, priv->lro_nb_counters[i]);

	return len;
}

/*
 * pfe_vwd_set_lro_nb_stats
 */
static ssize_t pfe_vwd_set_lro_nb_stats(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;

	memset(priv->lro_nb_counters, 0, sizeof(priv->lro_nb_counters));

	return count;
}

/*
 * pfe_vwd_show_lro_len_stats
 */
static ssize_t pfe_vwd_show_lro_len_stats(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	ssize_t len = 0;
	int i;

	for (i = 0; i < LRO_LEN_COUNT_MAX; i++)
		len += sprintf(buf + len, "RX packets > %dKBytes = %d\n", i * 2, priv->lro_len_counters[i]);

	return len;
}

/*
 * pfe_vwd_set_lro_len_stats
 */
static ssize_t pfe_vwd_set_lro_len_stats(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;

	memset(priv->lro_len_counters, 0, sizeof(priv->lro_len_counters));

	return count;
}
#endif

#ifdef PFE_VWD_NAPI_STATS
/*
 * pfe_vwd_show_napi_stats
 */
static ssize_t pfe_vwd_show_napi_stats(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	ssize_t len = 0;

	len += sprintf(buf + len, "sched:  %d\n", priv->napi_counters[NAPI_SCHED_COUNT]);
	len += sprintf(buf + len, "poll:   %d\n", priv->napi_counters[NAPI_POLL_COUNT]);
	len += sprintf(buf + len, "packet: %d\n", priv->napi_counters[NAPI_PACKET_COUNT]);
	len += sprintf(buf + len, "budget: %d\n", priv->napi_counters[NAPI_FULL_BUDGET_COUNT]);
	len += sprintf(buf + len, "desc:   %d\n", priv->napi_counters[NAPI_DESC_COUNT]);

	return len;
}

/*
 * pfe_vwd_set_napi_stats
 */
static ssize_t pfe_vwd_set_napi_stats(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;

	memset(priv->napi_counters, 0, sizeof(priv->napi_counters));

	return count;
}
#endif

/** pfe_vwd_show_dump_stats
 *
 */
static ssize_t pfe_vwd_show_dump_stats(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int ii;

#ifdef VWD_DEBUG_STATS
	len += sprintf(buf, "\nTo PFE\n");
	len += sprintf(buf + len, "  WiFi Rx pkts : %d\n", priv->pkts_transmitted);
	len += sprintf(buf + len, "  WiFi Tx pkts : %d\n", priv->pkts_total_local_tx);
	len += sprintf(buf + len, "  WiFi Tx SG pkts : %d\n", priv->pkts_local_tx_sgs);
	len += sprintf(buf + len, "  Drops : %d\n", priv->pkts_tx_dropped);

	len += sprintf(buf + len, "From PFE\n");
	len += sprintf(buf + len, "  WiFi Rx pkts : %d %d %d\n", priv->pkts_slow_forwarded[0],
			priv->pkts_slow_forwarded[1], priv->pkts_slow_forwarded[2]);
	len += sprintf(buf + len, "  WiFi Tx pkts : %d %d %d\n", priv->pkts_rx_fast_forwarded[0],
			priv->pkts_rx_fast_forwarded[1], priv->pkts_rx_fast_forwarded[2]);
	len += sprintf(buf + len, "  Skb Alloc fails : %d\n", priv->rx_skb_alloc_fail);
#endif
	len += sprintf(buf + len, "\nStatus\n");
	len += sprintf(buf + len, "  Fast path - %s\n", priv->fast_path_enable ? "Enable" : "Disable");
	len += sprintf(buf + len, "  Route hook - %s\n", priv->fast_routing_enable ? "Enable" : "Disable");
	len += sprintf(buf + len, "  Bridge hook - %s\n", priv->fast_bridging_enable ? "Enable" : "Disable");


	len += sprintf(buf + len, "VAPs Configuration  : \n");
	for (ii = 0; ii < MAX_VAP_SUPPORT; ii++) {
		struct vap_desc_s *vap;

		vap = &priv->vaps[ii];
		
		if (vap->state == VAP_ST_CLOSE)
			continue;

		len += sprintf(buf + len, "VAP Name : %s \n", vap->ifname);
		len += sprintf(buf + len, "	Id  	       : %d \n", vap->vapid);
		len += sprintf(buf + len, "	Index          : %d \n", vap->ifindex);
		len += sprintf(buf + len, "	State          : %s \n", (vap->state  == VAP_ST_OPEN) ? "OPEN":"CLOSED");
		len += sprintf(buf + len, "	CPU Affinity   : %d \n", vap->cpu_id);
		len += sprintf(buf + len, "	Direct Rx path : %s \n", vap->direct_rx_path ? "ON":"OFF");
		len += sprintf(buf + len, "	Direct Tx path : %s \n", vap->direct_tx_path ? "ON":"OFF");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
		len += sprintf(buf + len, "	Dev features   : VAP: %llx WiFi: %llx \n\n", vap->dev->features, vap->wifi_dev ? vap->wifi_dev->features:0);
#else
		len += sprintf(buf + len, "	Dev features   : VAP: %x WiFi: %x \n\n", vap->dev->features, vap->wifi_dev ? vap->wifi_dev->features:0);
#endif
	}	


	return len;
}


/** pfe_vwd_show_fast_path_enable
 *
 */
static ssize_t pfe_vwd_show_fast_path_enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int idx;

	idx = sprintf(buf, "\n%d\n", priv->fast_path_enable);

	return idx;
}

/** pfe_vwd_set_fast_path_enable
 *
 */
static ssize_t pfe_vwd_set_fast_path_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s  *priv = &pfe->vwd;
        unsigned int fast_path = 0;

	sscanf(buf, "%d", &fast_path);

	printk("%s: Wifi fast path %d\n", __func__, fast_path);

        if (fast_path && !priv->fast_path_enable)
        {
                printk("%s: Wifi fast path enabled \n", __func__);

                priv->fast_path_enable = 1;

        }
        else if (!fast_path && priv->fast_path_enable)
        {
                printk("%s: Wifi fast path disabled \n", __func__);

                priv->fast_path_enable = 0;

        }

	return count;
}

/** pfe_vwd_show_route_hook_enable
 *
 */
static ssize_t pfe_vwd_show_route_hook_enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int idx;

	idx = sprintf(buf, "\n%d\n", priv->fast_routing_enable);

	return idx;
}

/** pfe_vwd_set_route_hook_enable
 *
 */
static ssize_t pfe_vwd_set_route_hook_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
        unsigned int user_val = 0;

        sscanf(buf, "%d", &user_val);

        if (user_val && !priv->fast_routing_enable)
        {
                printk("%s: Wifi fast routing enabled \n", __func__);
                priv->fast_routing_enable = 1;

                if (priv->fast_bridging_enable)
                {
                        nf_unregister_hook(&vwd_hook_bridge);
                        priv->fast_bridging_enable = 0;
                }

                nf_register_hook(&vwd_hook);
                nf_register_hook(&vwd_hook_ipv6);


        }
        else if (!user_val && priv->fast_routing_enable)
        {
                printk("%s: Wifi fast routing disabled \n", __func__);
                priv->fast_routing_enable = 0;

                nf_unregister_hook(&vwd_hook);
                nf_unregister_hook(&vwd_hook_ipv6);

        }

	return count;
}

/** pfe_vwd_show_bridge_hook_enable
 *
 */
static int pfe_vwd_show_bridge_hook_enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int idx;

	idx = sprintf(buf, "%d", priv->fast_bridging_enable);
	return idx;
}

/** pfe_vwd_set_bridge_hook_enable
 *
 */
static int pfe_vwd_set_bridge_hook_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	unsigned int user_val = 0;

        sscanf(buf, "%d", &user_val);

	if ( user_val && !priv->fast_bridging_enable )
        {
                printk("%s: Wifi fast bridging enabled \n", __func__);
                priv->fast_bridging_enable = 1;

                if(priv->fast_routing_enable)
                {
                        nf_unregister_hook(&vwd_hook);
                        nf_unregister_hook(&vwd_hook_ipv6);
                        priv->fast_routing_enable = 0;
                }

                nf_register_hook(&vwd_hook_bridge);
        }
        else if ( !user_val && priv->fast_bridging_enable )
        {
                printk("%s: Wifi fast bridging disabled \n", __func__);
                priv->fast_bridging_enable = 0;

                nf_unregister_hook(&vwd_hook_bridge);
        }

	return count;
}

/** pfe_vwd_show_direct_tx_path
 *
 */
static int pfe_vwd_show_direct_tx_path(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vap_desc_s *vap;
	int rc;

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);
	rc = sprintf(buf, "%d\n", vap->direct_tx_path);
	spin_unlock_bh(&pfe->vwd.vaplock);

	return rc;
}

/** pfe_vwd_set_direct_tx_path
 *
 */
static int pfe_vwd_set_direct_tx_path(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct vap_desc_s *vap;
	int enable;

	sscanf(buf, "%d", &enable);

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);
	printk(KERN_INFO "%s: VWD => WiFi direct path is %s for %s\n",
				__func__, enable ? "enabled":"disabled", vap->ifname);
	vap->direct_tx_path = enable;
	spin_unlock_bh(&pfe->vwd.vaplock);

	return count;
}

/** pfe_vwd_show_direct_rx_path
 *
 */
static int pfe_vwd_show_direct_rx_path(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vap_desc_s *vap;
	int rc;

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);
	rc = sprintf(buf, "%d\n", vap->direct_rx_path);
	spin_unlock_bh(&pfe->vwd.vaplock);

	return rc;
}

/** pfe_vwd_set_direct_rx_path
 *
 */
static int pfe_vwd_set_direct_rx_path(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct vap_desc_s *vap;
	int enable;

	sscanf(buf, "%d", &enable);

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);
	printk(KERN_INFO "%s: WiFi => VWD direct path is %s for %s\n",
				__func__, enable ? "enabled":"disabled", vap->ifname);
	vap->direct_rx_path = enable;

	spin_unlock_bh(&pfe->vwd.vaplock);

	return count;
}

#if defined(CONFIG_SMP) && (NR_CPUS > 1)
/** pfe_vwd_show_rx_cpu_affinity
 *
 */
static int pfe_vwd_show_rx_cpu_affinity(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vap_desc_s *vap;
	int rc;

	spin_lock_bh(&pfe->vwd.vaplock);
	vap =  get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);
	rc = sprintf(buf, "%d\n", vap->cpu_id);
	spin_unlock_bh(&pfe->vwd.vaplock);

	return rc;
}

/** pfe_vwd_set_rx_cpu_affinity
 *
 */
static int pfe_vwd_set_rx_cpu_affinity(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct vap_desc_s *vap;
	unsigned int cpu_id;

	sscanf(buf, "%d", &cpu_id);

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);

	if (cpu_id < NR_CPUS) {
		vap->cpu_id = cpu_id;
		hif_lib_set_rx_cpu_affinity(&vap->client, cpu_id);
	}
	else
		printk(KERN_ERR "%s: Invalid cpu#%d \n", __func__, cpu_id);

	spin_unlock_bh(&pfe->vwd.vaplock);

	return count;
}
#endif

#if defined(CONFIG_COMCERTO_CUSTOM_SKB_LAYOUT)
/** pfe_vwd_show_custom_skb_enable
 *
 */
static int pfe_vwd_show_custom_skb_enable(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vap_desc_s *vap;
	int rc;

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);
	rc = sprintf(buf, "%d\n", vap->custom_skb);
	spin_unlock_bh(&pfe->vwd.vaplock);

	return rc;
}

/** pfe_vwd_set_custom_skb_enable
 *
 */
static int pfe_vwd_set_custom_skb_enable(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct vap_desc_s *vap;
	int enable;

	sscanf(buf, "%d", &enable);

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));
	BUG_ON(!vap);
	printk(KERN_INFO "%s: Custun skb feature is %s for %s\n", __func__, enable ? "enabled":"disabled", vap->ifname);
	vap->custom_skb = enable;
	spin_unlock_bh(&pfe->vwd.vaplock);

	return count;
}
#endif

#ifdef PFE_VWD_TX_STATS
/** pfe_vwd_show_vap_tx_stats
 *
 */
static int pfe_vwd_show_vap_tx_stats(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct vap_desc_s *vap;
	int len = 0, ii;

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));

	BUG_ON(!vap);

	len = sprintf(buf, "TX queues stats:\n");

	for (ii = 0; ii < VWD_TXQ_CNT; ii++) {
		len += sprintf(buf + len, "Queue #%02d\n", ii);
		len += sprintf(buf + len, "            clean_fail            = %10d\n", vap->clean_fail[ii]);
		len += sprintf(buf + len, "            stop_queue            = %10d\n", vap->stop_queue_total[ii]);
		len += sprintf(buf + len, "            stop_queue_hif        = %10d\n", vap->stop_queue_hif[ii]);
		len += sprintf(buf + len, "            stop_queue_hif_client = %10d\n", vap->stop_queue_hif_client[ii]);
	}

	spin_unlock_bh(&pfe->vwd.vaplock);
	return len;
}

/** pfe_vwd_set_vap_tx_stats
 *
 */
static int pfe_vwd_set_vap_tx_stats(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct vap_desc_s *vap;
	int ii;

	spin_lock_bh(&pfe->vwd.vaplock);
	vap = get_vap_by_name(&pfe->vwd, kobject_name(kobj));

	BUG_ON(!vap);

	for (ii = 0; ii < VWD_TXQ_CNT; ii++) {
		spin_lock_bh(&vap->tx_lock[ii]);
		vap->clean_fail[ii] = 0;
		vap->stop_queue_total[ii] = 0;
		vap->stop_queue_hif[ii] = 0;
		vap->stop_queue_hif_client[ii] = 0;
		spin_unlock_bh(&vap->tx_lock[ii]);
	}

	spin_unlock_bh(&pfe->vwd.vaplock);
	return count;
}
#endif

/*
 * pfe_vwd_show_tso_stats
 */
static ssize_t pfe_vwd_show_tso_stats(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	ssize_t len = 0;
	int i;

	for (i = 0; i < 32; i++)
		len += sprintf(buf + len, "TSO packets > %dKBytes = %u\n", i * 2, priv->tso.len_counters[i]);

	return len;
}

/*
 * pfe_vwd_set_tso_stats
 */
static ssize_t pfe_vwd_set_tso_stats(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;

	memset(priv->tso.len_counters, 0, sizeof(priv->tso.len_counters));
	return count;
}

/** pfe_vwd_classify_packet
 *
 */
static int pfe_vwd_classify_packet( struct pfe_vwd_priv_s *priv, struct sk_buff *skb,
							int bridge_hook, int route_hook, int *vapid, int *own_mac)
{
	unsigned short type;
	struct vap_desc_s *vap;
	int rc = 1, ii, length;
	unsigned char *data_ptr;
	struct ethhdr *hdr;
#if defined (CONFIG_COMCERTO_VWD_MULTI_MAC)
	struct net_bridge_fdb_entry *dst = NULL;
	struct net_bridge_port *p = NULL;
	const unsigned char *dest = eth_hdr(skb)->h_dest;
#endif
	*own_mac = 0;

	/* Move to packet network header */
	data_ptr = skb_mac_header(skb);
	length = skb->len + (skb->data - data_ptr);

	spin_lock_bh(&priv->vaplock);
	/* Broadcasts and MC are handled by stack */
	if( (eth_hdr(skb)->h_dest[0] & 0x1) ||
			( length <= ETH_HLEN ) )
	{
		rc = 1;
		goto done;
	}

	/* FIXME: This packet is VWD slow path packet, and already seen by VWD */

	if (*(unsigned long *)skb->head == 0xdead)
	{
		//printk(KERN_INFO "%s:This is dead packet....\n", __func__);
		*(unsigned long *)skb->head = 0x0;
		rc = 1;
		goto done;
	}

#ifdef VWD_DEBUG
	printk(KERN_INFO "%s: skb cur len:%d skb orig len:%d\n", __func__, skb->len, length );
#endif

	/* FIXME: We need to check the route table for the route entry. If route
	 *  entry found for the current packet, send the packet to PFE. Otherwise
	 *  REJECT the packet.
	 */
	for ( ii = 0; ii < MAX_VAP_SUPPORT; ii++ )
	{
		vap = &priv->vaps[ii];
		if (vap->ifindex == skb->skb_iif)
		{
			/* This interface packets need to be processed by direct API */
			if (vap->direct_rx_path || (vap->state != VAP_ST_OPEN)) {
				rc = 1;
				goto done;
			}

			hdr = (struct ethhdr *)data_ptr;
			type = htons(hdr->h_proto);
			data_ptr += ETH_HLEN;
			length -= ETH_HLEN;
			rc = 0;

			*vapid = vap->vapid;

			/* FIXME send only IPV4 and IPV6 packets to PFE */
			//Determain final protocol type
			//FIXME : This multi level parsing is not required for
			//        Bridged packets.
			if( type == ETH_P_8021Q )
			{
				struct vlan_hdr *vhdr = (struct vlan_hdr *)data_ptr;

				data_ptr += VLAN_HLEN;
				length -= VLAN_HLEN;
				type = htons(vhdr->h_vlan_encapsulated_proto);
			}

			if( type == ETH_P_PPP_SES )
			{
				struct pppoe_hdr *phdr = (struct pppoe_hdr *)data_ptr;

				if (htons(*(u16 *)(phdr+1)) == PPP_IP)
					type = ETH_P_IP;
				else if (htons(*(u16 *)(phdr+1)) == PPP_IPV6)
					type = ETH_P_IPV6;
			}


			if (bridge_hook)
			{
#if defined (CONFIG_COMCERTO_VWD_MULTI_MAC)
				/* check if destination MAC matches one of the interfaces attached to the bridge */
				if((p = rcu_dereference(skb->dev->br_port)) != NULL)
					dst = __br_fdb_get(p->br, dest);

				if (skb->pkt_type == PACKET_HOST || (dst && dst->is_local))
#else
					if (skb->pkt_type == PACKET_HOST)
#endif
					{
						*own_mac = 1;

						if ((type != ETH_P_IP) && (type != ETH_P_IPV6))
						{
							rc = 1;
							goto done;
						}
					}
					else if (!memcmp(vap->macaddr, eth_hdr(skb)->h_dest, ETH_ALEN))
					{
						//WiFi management packets received with dst address
						//as bssid
						rc = 1;
						goto done;
					}
			}
			else
				*own_mac = 1;

			break;
		}

	}

done:
	spin_unlock_bh(&priv->vaplock);
	return rc;

}

/** pfe_vwd_flush_txQ
 *
 */
static void pfe_vwd_flush_txQ(struct vap_desc_s *vap, int queuenum, int from_tx, int n_desc)
{
	struct sk_buff *skb;
	int count = max(TX_FREE_MAX_COUNT, n_desc);
	unsigned int flags;

	//printk(KERN_INFO "%s\n", __func__);

	if (!from_tx)
		spin_lock_bh(&vap->tx_lock[queuenum]);

	while (count && (skb = hif_lib_tx_get_next_complete(&vap->client, queuenum, &flags, count))) {

		/* FIXME : Invalid data can be skipped in hif_lib itself */
		if (flags & HIF_DATA_VALID) {
			if (flags & HIF_DONT_DMA_MAP)
				pfe_tx_skb_unmap(skb);
			dev_kfree_skb_any(skb);
		}
		// When called from the timer, flush all descriptors
		if (from_tx)
			count--;
	}

	if (!from_tx)
		spin_unlock_bh(&vap->tx_lock[queuenum]);


}

/** pfe_eth_flush_tx
 */
static void pfe_vwd_flush_tx(struct vap_desc_s *vap, int force)
{
	int ii;

	for (ii = 0; ii < VWD_TXQ_CNT; ii++) {
		if (force || (time_after(jiffies, vap->client.tx_q[ii].jiffies_last_packet + (COMCERTO_TX_RECOVERY_TIMEOUT_MS * HZ)/1000)))
			pfe_vwd_flush_txQ(vap, ii, 0, 0); //We will release everything we can based on from_tx param, so the count param can be set to any value
	}
}


/** pfe_vwd_tx_timeout
 */
void pfe_vwd_tx_timeout(unsigned long data )
{
	struct pfe_vwd_priv_s *priv = (struct pfe_vwd_priv_s *)data;
	int ii;

	spin_lock_bh(&priv->vaplock);
	for (ii = 0; ii < MAX_VAP_SUPPORT; ii++) {
		if (priv->vaps[ii].state != VAP_ST_OPEN)
			continue;

		pfe_vwd_flush_tx(&priv->vaps[ii], 0);
	}

	if (priv->vap_count) {
		priv->tx_timer.expires = jiffies + ( COMCERTO_TX_RECOVERY_TIMEOUT_MS * HZ )/1000;
		add_timer(&priv->tx_timer);
	}

	spin_unlock_bh(&priv->vaplock);
}

/** pfe_vwd_send_packet
 *
 */
static void pfe_vwd_send_packet( struct sk_buff *skb, struct  pfe_vwd_priv_s *priv, int queuenum, struct vap_desc_s *vap, int own_mac, u32 ctrl)
{
	void *data;
	int count;
	unsigned int nr_frags;
	struct skb_shared_info *sh;
	unsigned int nr_desc, nr_segs;

	spin_lock_bh(&vap->tx_lock[queuenum]);

	if (skb_headroom(skb) < (PFE_PKT_HEADER_SZ + sizeof(unsigned long))) {

		//printk(KERN_INFO "%s: copying skb %d\n", __func__, skb_headroom(skb));

		if (pskb_expand_head(skb, (PFE_PKT_HEADER_SZ + sizeof(unsigned long)), 0, GFP_ATOMIC)) {
			kfree_skb(skb);
			skb = NULL;
#ifdef VWD_DEBUG_STATS
			priv->pkts_tx_dropped += 1;
#endif
			goto out;
		}
	}

	pfe_tx_get_req_desc(skb, &nr_desc, &nr_segs);
	hif_tx_lock(&pfe->hif);

	if ((__hif_tx_avail(&pfe->hif) < nr_desc) || (hif_lib_tx_avail(&vap->client, queuenum) < nr_desc)) {

		//printk(KERN_ERR "%s: __hif_lib_xmit_pkt() failed\n", __func__);
		kfree_skb(skb);
		skb = NULL;
#ifdef VWD_DEBUG_STATS
		priv->pkts_tx_dropped++;
#endif
		goto out;
	}

	/* Send vap_id to PFE */
	if (own_mac)
		ctrl |= ((vap->vapid << HIF_CTRL_VAPID_OFST) | HIF_CTRL_TX_OWN_MAC);
	else
		ctrl |= (vap->vapid << HIF_CTRL_VAPID_OFST);


	if ((vap->wifi_dev->features & NETIF_F_RXCSUM) && (skb->ip_summed == CHECKSUM_NONE)) 
		ctrl |= HIF_CTRL_TX_CSUM_VALIDATE;


	sh = skb_shinfo(skb);
	nr_frags = sh->nr_frags;

	/* if nr_desc > 1, then skb is scattered, otherwise linear skb */
	if (nr_frags) {
		skb_frag_t *f;
		int i;

		__hif_lib_xmit_pkt(&vap->client, queuenum, skb->data, skb_headlen(skb), ctrl, HIF_FIRST_BUFFER, skb);

		for (i = 0; i < nr_frags - 1; i++) {
			f = &sh->frags[i];

			__hif_lib_xmit_pkt(&vap->client, queuenum, skb_frag_address(f), skb_frag_size(f), 0x0, 0x0, skb);
		}

		f = &sh->frags[i];

		__hif_lib_xmit_pkt(&vap->client, queuenum, skb_frag_address(f), skb_frag_size(f), 0x0, HIF_LAST_BUFFER|HIF_DATA_VALID, skb);


#ifdef VWD_DEBUG_STATS
		priv->pkts_local_tx_sgs += 1;
#endif
	}
	else
	{
#if defined(CONFIG_COMCERTO_CUSTOM_SKB_LAYOUT)
		if (skb->mspd_data && skb->mspd_len) {
			int len = skb->len -  skb->mspd_len;

			//printk("%s : custom skb\n", __func__);

			data = (skb->mspd_data + skb->mspd_ofst) - len;
			memcpy(data, skb->data, len);
		}
		else
#endif

		data = skb->data;

		__hif_lib_xmit_pkt(&vap->client, queuenum, data, skb->len, ctrl, HIF_FIRST_BUFFER | HIF_LAST_BUFFER | HIF_DATA_VALID, skb);
	}

	hif_tx_dma_start();

#ifdef VWD_DEBUG_STATS
	priv->pkts_transmitted += 1;
#endif
	vap->stats.tx_packets++;
	vap->stats.tx_bytes += skb->len;


out:
	hif_tx_unlock(&pfe->hif);
	// Recycle buffers if a socket's send buffer becomes half full or if the HIF client queue starts filling up
	if (((count = (hif_lib_tx_pending(&vap->client, queuenum) - HIF_CL_TX_FLUSH_MARK)) > 0)
		|| (skb && skb->sk && ((sk_wmem_alloc_get(skb->sk) << 1) > skb->sk->sk_sndbuf)))
		pfe_vwd_flush_txQ(vap, queuenum, 1, count);

	spin_unlock_bh(&vap->tx_lock[queuenum]);

	return;
}

#ifdef CONFIG_PFE_WIFI_OFFLOAD
/*
 * vwd_wifi_if_send_pkt
 */
static int vwd_wifi_if_send_pkt(struct sk_buff *skb)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int ii;
	unsigned int dst_mac[2];

	if (!priv->fast_path_enable)
		goto end;

	/* Copy destination mac into cacheable memory */
	if (!((unsigned long)skb->data & 0x3))
		__memcpy8(dst_mac, skb->data);
	else
		memcpy(dst_mac, skb->data, 6);

	if (dst_mac[0] & 0x1)
		goto end;

	spin_lock_bh(&priv->vaplock);

	for (ii = 0; ii < MAX_VAP_SUPPORT; ii++)
	{
		struct vap_desc_s *vap;

		vap = &priv->vaps[ii];

		if (vap->state != VAP_ST_OPEN)
			continue;

		if ((vap->ifindex == skb->dev->ifindex) && vap->direct_rx_path)
		{

			if (!memcmp(vap->macaddr, dst_mac, ETH_ALEN))
				pfe_vwd_send_packet( skb, priv, 0, vap, 1, 0);
			else
				pfe_vwd_send_packet( skb, priv, 0, vap, 0, 0);

			break;
		}
	}

	spin_unlock_bh(&priv->vaplock);

	if (unlikely(ii == MAX_VAP_SUPPORT))
		goto end;

	return 0;

end:
	return -1;
}
#endif


/** vwd_nf_bridge_hook_fn
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
static unsigned int pfe_vwd_nf_bridge_hook_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
	       const struct nf_hook_state *state)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
static unsigned int pfe_vwd_nf_bridge_hook_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
#else
static unsigned int pfe_vwd_nf_bridge_hook_fn( unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
#endif
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int vapid = -1;
	int own_mac = 0;

#ifdef VWD_DEBUG
	printk("%s: protocol : 0x%04x\n", __func__, htons(skb->protocol));
#endif

	if( !priv->fast_path_enable )
		goto done;

	if( !pfe_vwd_classify_packet(priv, skb, 1, 0, &vapid, &own_mac) )
	{
#ifdef VWD_DEBUG
		printk("%s: Accepted\n", __func__);
	//	pfe_vwd_dump_skb( skb );
#endif
		skb_push(skb, ETH_HLEN);
		pfe_vwd_send_packet( skb, priv, 0, &priv->vaps[vapid], own_mac, 0);
		return NF_STOLEN;
	}

done:

	return NF_ACCEPT;

}

/** vwd_nf_route_hook_fn
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
static unsigned int pfe_vwd_nf_route_hook_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
	       const struct nf_hook_state *state)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
static unsigned int pfe_vwd_nf_route_hook_fn(const struct nf_hook_ops *ops, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
#else
static unsigned int pfe_vwd_nf_route_hook_fn( unsigned int hook, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out,
		int (*okfn)(struct sk_buff *))
#endif
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;
	int vapid = -1;
	int own_mac = 0;

#ifdef VWD_DEBUG
	printk("%s: protocol : 0x%04x\n", __func__, htons(skb->protocol));
#endif

	if (!priv->fast_path_enable)
		goto done;

	if (!pfe_vwd_classify_packet(priv, skb, 0, 1, &vapid, &own_mac))
	{
#ifdef VWD_DEBUG
		printk("%s: Accepted\n", __func__);
//		pfe_vwd_dump_skb( skb );
#endif
		skb_push(skb, ETH_HLEN);
		pfe_vwd_send_packet( skb, priv, 0, &priv->vaps[vapid], own_mac, 0);
		return NF_STOLEN;
	}

done:
	return NF_ACCEPT;

}

static DEVICE_ATTR(vwd_debug_stats, 0444, pfe_vwd_show_dump_stats, NULL);
static DEVICE_ATTR(vwd_fast_path_enable, 0644, pfe_vwd_show_fast_path_enable, pfe_vwd_set_fast_path_enable);
static DEVICE_ATTR(vwd_route_hook_enable, 0644, pfe_vwd_show_route_hook_enable, pfe_vwd_set_route_hook_enable);
static DEVICE_ATTR(vwd_bridge_hook_enable, 0644, pfe_vwd_show_bridge_hook_enable, pfe_vwd_set_bridge_hook_enable);
static DEVICE_ATTR(vwd_tso_stats, 0644, pfe_vwd_show_tso_stats, pfe_vwd_set_tso_stats);

static struct kobj_attribute direct_rx_attr =
		 __ATTR(direct_rx_path, 0644, pfe_vwd_show_direct_rx_path, pfe_vwd_set_direct_rx_path);
static struct kobj_attribute direct_tx_attr =
		 __ATTR(direct_tx_path, 0644, pfe_vwd_show_direct_tx_path, pfe_vwd_set_direct_tx_path);
#if defined(CONFIG_COMCERTO_CUSTOM_SKB_LAYOUT)
static struct kobj_attribute custom_skb_attr =
		 __ATTR(custom_skb_enable, 0644, pfe_vwd_show_custom_skb_enable, pfe_vwd_set_custom_skb_enable);
#endif
#if defined(CONFIG_SMP) && (NR_CPUS > 1)
static struct kobj_attribute rx_cpu_affinity_attr =
		 __ATTR(rx_cpu_affinity, 0644, pfe_vwd_show_rx_cpu_affinity, pfe_vwd_set_rx_cpu_affinity);
#endif
#ifdef PFE_VWD_TX_STATS
static struct kobj_attribute tx_stats_attr =
		 __ATTR(tx_stats, 0644, pfe_vwd_show_vap_tx_stats, pfe_vwd_set_vap_tx_stats);
#endif
static struct attribute *vap_attrs[] = {
	&direct_rx_attr.attr,
	&direct_tx_attr.attr,
#if defined(CONFIG_COMCERTO_CUSTOM_SKB_LAYOUT)
	&custom_skb_attr.attr,
#endif
#if defined(CONFIG_SMP) && (NR_CPUS > 1)
	&rx_cpu_affinity_attr.attr,
#endif
#ifdef PFE_VWD_TX_STATS
	&tx_stats_attr.attr,
#endif
	NULL,
};

static struct attribute_group vap_attr_group = {
	.attrs = vap_attrs,
};

#ifdef PFE_VWD_NAPI_STATS
static DEVICE_ATTR(vwd_napi_stats, 0644, pfe_vwd_show_napi_stats, pfe_vwd_set_napi_stats);
#endif
static DEVICE_ATTR(vwd_vap_create, 0644, NULL, pfe_vwd_vap_create);
static DEVICE_ATTR(vwd_vap_reset, 0644, NULL, pfe_vwd_vap_reset);
#ifdef PFE_VWD_LRO_STATS
static DEVICE_ATTR(vwd_lro_nb_stats, 0644, pfe_vwd_show_lro_nb_stats, pfe_vwd_set_lro_nb_stats);
static DEVICE_ATTR(vwd_lro_len_stats, 0644, pfe_vwd_show_lro_len_stats, pfe_vwd_set_lro_len_stats);
#endif
/** pfe_vwd_sysfs_init
 *
 */
static int pfe_vwd_sysfs_init( struct pfe_vwd_priv_s *priv )
{
	struct pfe *pfe = priv->pfe;

	if (device_create_file(pfe->dev, &dev_attr_vwd_debug_stats))
		goto err_dbg_sts;

	if (device_create_file(pfe->dev, &dev_attr_vwd_fast_path_enable))
		goto err_fp_en;

	if (device_create_file(pfe->dev, &dev_attr_vwd_route_hook_enable))
		goto err_rt;

	if (device_create_file(pfe->dev, &dev_attr_vwd_bridge_hook_enable))
		goto err_br;

	if ((vwd_ofld == PFE_VWD_NAS_MODE ) && device_create_file(pfe->dev, &dev_attr_vwd_vap_create))
		goto err_vap_add;

	if ((vwd_ofld == PFE_VWD_NAS_MODE) && device_create_file(pfe->dev, &dev_attr_vwd_vap_reset))
		goto err_vap_del;

	if (device_create_file(pfe->dev, &dev_attr_vwd_tso_stats))
		goto err_tso_stats;

#ifdef PFE_VWD_NAPI_STATS
	if (device_create_file(pfe->dev, &dev_attr_vwd_napi_stats))
		goto err_napi;
#endif

#ifdef PFE_VWD_LRO_STATS
	if (device_create_file(pfe->dev, &dev_attr_vwd_lro_nb_stats))
		goto err_lro_nb;

	if (device_create_file(pfe->dev, &dev_attr_vwd_lro_len_stats))
		goto err_lro_len;
#endif

	return 0;

#ifdef PFE_VWD_LRO_STATS
err_lro_len:
	device_remove_file(pfe->dev, &dev_attr_vwd_lro_nb_stats);
err_lro_nb:
#endif

#ifdef PFE_VWD_NAPI_STATS
	device_remove_file(pfe->dev, &dev_attr_vwd_napi_stats);
err_napi:
#endif

#if defined(PFE_VWD_LRO_STATS) || defined(PFE_VWD_NAPI_STATS)
	device_remove_file(pfe->dev, &dev_attr_vwd_tso_stats);
#endif

err_tso_stats:
	if (vwd_ofld == PFE_VWD_NAS_MODE)
		device_remove_file(pfe->dev, &dev_attr_vwd_vap_reset);
err_vap_del:
	if (vwd_ofld == PFE_VWD_NAS_MODE)
		device_remove_file(pfe->dev, &dev_attr_vwd_vap_create);
err_vap_add:
	device_remove_file(pfe->dev, &dev_attr_vwd_bridge_hook_enable);
err_br:
	device_remove_file(pfe->dev, &dev_attr_vwd_route_hook_enable);
err_rt:
	device_remove_file(pfe->dev, &dev_attr_vwd_fast_path_enable);
err_fp_en:
	device_remove_file(pfe->dev, &dev_attr_vwd_debug_stats);
err_dbg_sts:
	return -1;
}


/** pfe_vwd_sysfs_exit
 *
 */
static void pfe_vwd_sysfs_exit(void)
{
	device_remove_file(pfe->dev, &dev_attr_vwd_tso_stats);
#ifdef PFE_VWD_LRO_STATS
	device_remove_file(pfe->dev, &dev_attr_vwd_lro_len_stats);
	device_remove_file(pfe->dev, &dev_attr_vwd_lro_nb_stats);
#endif

#ifdef PFE_VWD_NAPI_STATS
	device_remove_file(pfe->dev, &dev_attr_vwd_napi_stats);
#endif
	if (vwd_ofld == PFE_VWD_NAS_MODE) {
		device_remove_file(pfe->dev, &dev_attr_vwd_vap_create);
		device_remove_file(pfe->dev, &dev_attr_vwd_vap_reset);
	}
	device_remove_file(pfe->dev, &dev_attr_vwd_bridge_hook_enable);
	device_remove_file(pfe->dev, &dev_attr_vwd_route_hook_enable);
	device_remove_file(pfe->dev, &dev_attr_vwd_fast_path_enable);
	device_remove_file(pfe->dev, &dev_attr_vwd_debug_stats);
}

/** pfe_vwd_rx_page
 *
 */
static struct sk_buff *pfe_vwd_rx_page(struct vap_desc_s *vap, int qno, unsigned int *ctrl)
{
	struct page *p;
	void *buf_addr;
	unsigned int rx_ctrl;
	unsigned int desc_ctrl = 0;
	struct sk_buff *skb;
	int length, offset, data_offset;
	struct hif_lro_hdr *lro_hdr;
	unsigned long page_offset;
	u32 pe_id;
	struct pfe_vwd_priv_s *priv = vap->priv;


	while (1) {
		buf_addr = hif_lib_receive_pkt(&vap->client, qno, &length, &offset, &rx_ctrl, &desc_ctrl, (void **)&lro_hdr);

		if (!buf_addr)
			goto empty;

		if (qno == 2)
			pe_id = (rx_ctrl >> HIF_CTRL_RX_PE_ID_OFST) & 0xf;
		else
			pe_id = 0;

		if (vap->state != VAP_ST_OPEN) {
			free_page((unsigned long)buf_addr);
			continue;
		}
			

		skb = vap->skb_inflight[qno + pe_id];

#ifdef PFE_VWD_NAPI_STATS
		priv->napi_counters[NAPI_DESC_COUNT]++;
#endif

		*ctrl = rx_ctrl;

		/* Compute page offset */
		page_offset = ((unsigned long)buf_addr & ~(PAGE_MASK)) & HIF_RX_PKT_MIN_SIZE_MASK;

		/* First frag */
		if ((desc_ctrl & CL_DESC_FIRST) && !skb) {
			p = virt_to_page(buf_addr);

			skb = dev_alloc_skb(MAX_HDR_SIZE + PFE_PKT_HEADROOM + 2);
			if (unlikely(!skb)) {
				goto pkt_drop;
			}

			skb_reserve(skb, PFE_PKT_HEADROOM + 2);

			if (lro_hdr) {
				data_offset = lro_hdr->data_offset;
				if (lro_hdr->mss)
					skb_shinfo(skb)->gso_size = lro_hdr->mss;

			//	printk(KERN_INFO "mss: %d, offset: %d, data_offset: %d, len: %d\n", lro_hdr->mss, offset, lro_hdr->data_offset, length);
			} else {
				data_offset = MAX_HDR_SIZE;
			}

			/* We don't need the fragment if the whole packet */
			/* has been copied in the first linear skb        */
			if (length <= data_offset) {
				__memcpy(skb->data, buf_addr + offset, length);
				skb_put(skb, length);
				free_page((unsigned long)buf_addr);
			} else {
				__memcpy(skb->data, buf_addr + offset, data_offset);
				skb_put(skb, data_offset);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
				skb_add_rx_frag(skb, 0, p, page_offset + offset + data_offset, length - data_offset, length - data_offset);
#else
				skb_add_rx_frag(skb, 0, p, page_offset + offset + data_offset, length - data_offset);
#endif
			}

			if ((vap->wifi_dev->features & NETIF_F_RXCSUM) && (rx_ctrl & HIF_CTRL_RX_CHECKSUMMED))
			{
				skb->ip_summed = CHECKSUM_UNNECESSARY;
#ifdef VWD_DEBUG_STATS
				priv->rx_csum_correct++;
#endif
			}

		} else {
			/* Next frags */
			if (unlikely(!skb)) {
				printk(KERN_ERR "%s: NULL skb_inflight\n", __func__);
				goto pkt_drop;
			}

			p = virt_to_page(buf_addr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
			skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags, p, page_offset + offset, length, length);
#else
			skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags, p, page_offset + offset, length);
#endif
		}

		/* Last buffer in a software chain */
		if ((desc_ctrl & CL_DESC_LAST) && !(rx_ctrl & HIF_CTRL_RX_CONTINUED))
			break;

		/* Keep track of skb for this queue/pe */
		vap->skb_inflight[qno + pe_id] = skb;
	}

	vap->skb_inflight[qno + pe_id] = NULL;

	return skb;

pkt_drop:
	vap->skb_inflight[qno + pe_id] = NULL;

	free_page((unsigned long)buf_addr);

	return NULL;

empty:
	return NULL;
}

/** pfe_vwd_rx_skb
 *
 */
static struct sk_buff *pfe_vwd_rx_skb(struct vap_desc_s *vap, int qno, unsigned int *ctrl)
{
	void *buf_addr;
	struct hif_ipsec_hdr *ipsec_hdr;
	unsigned int rx_ctrl;
	unsigned int desc_ctrl = 0;
	struct sk_buff *skb = NULL;
	int length = 0, offset;
	struct pfe_vwd_priv_s *priv = vap->priv;
#if defined(CONFIG_INET_IPSEC_OFFLOAD) || defined(CONFIG_INET6_IPSEC_OFFLOAD)
	struct timespec ktime;
#endif

	buf_addr = hif_lib_receive_pkt(&vap->client, qno, &length, &offset, &rx_ctrl, &desc_ctrl,(void **) &ipsec_hdr);
	if (!buf_addr)
		goto out;
		
	if (vap->state != VAP_ST_OPEN) 
		goto pkt_drop;
	

	*ctrl = rx_ctrl;
#ifdef PFE_VWD_NAPI_STATS
	priv->napi_counters[NAPI_DESC_COUNT]++;
#endif

#if defined(CONFIG_COMCERTO_CUSTOM_SKB_LAYOUT)
	if ((vap->custom_skb) && !(rx_ctrl & HIF_CTRL_RX_WIFI_EXPT)) {
		/* Even we use smaller area allocate bigger buffer, to meet skb helper function's requirements */
		skb = dev_alloc_skb(length + offset + 32);

		if (unlikely(!skb)) {
#ifdef VWD_DEBUG_STATS
			priv->rx_skb_alloc_fail += 1;
#endif
			goto pkt_drop;
		}

		/**
		 *  __memcpy expects src and dst need to be same alignment. So make sure that
		 *  skb->data starts at same alignement as buf_addr + offset.
		 */
		skb_reserve(skb, offset);
		if (length <= MAX_WIFI_HDR_SIZE) {
			__memcpy(skb->data, buf_addr + offset, length);
			skb_put(skb, length);
#if defined(CONFIG_COMCERTO_DMA_COHERENT_SKB)
			dma_free_coherent(NULL, PFE_BUF_SIZE, buf_addr, 0);
#elif
			kfree(buf_addr);
#endif
		}
		else {
			__memcpy(skb->data, buf_addr + offset, MAX_WIFI_HDR_SIZE);
			skb_put(skb, length);
			skb->mspd_data = buf_addr;
			skb->mspd_len = length - MAX_WIFI_HDR_SIZE;
			skb->mspd_ofst = offset + MAX_WIFI_HDR_SIZE;
		}
	}

	else
#endif
	if (rx_ctrl & HIF_CTRL_RX_WIFI_EXPT) {
#if defined(CONFIG_COMCERTO_ZONE_DMA_NCNB) || defined(CONFIG_COMCERTO_DMA_COHERENT_SKB)
		skb = dev_alloc_skb(length + offset + 32);
#else
		skb = build_skb(buf_addr, 0);
#endif

		if (unlikely(!skb)) {
#ifdef VWD_DEBUG_STATS
			priv->rx_skb_alloc_fail += 1;
#endif
			goto pkt_drop;
		}

#if !defined(CONFIG_COMCERTO_ZONE_DMA_NCNB) && !defined(CONFIG_COMCERTO_DMA_COHERENT_SKB)
		skb->data = buf_addr;
#endif
		skb_reserve(skb, offset);
#if defined(CONFIG_COMCERTO_ZONE_DMA_NCNB) || defined(CONFIG_COMCERTO_DMA_COHERENT_SKB)
		/* Since, these packets are going to linux stack, 
                 * to avoid NCNB access overhead copy NCNB to CB buffer.
                 */
		__memcpy(skb->data, buf_addr + offset, length);
#if defined(CONFIG_COMCERTO_ZONE_DMA_NCNB)
		kfree(buf_addr);
#elif defined(CONFIG_COMCERTO_DMA_COHERENT_SKB)
		dma_free_coherent(NULL, PFE_BUF_SIZE, buf_addr, 0);
#endif
#endif
		skb_put(skb, length);

		if ((vap->wifi_dev->features & NETIF_F_RXCSUM) && (rx_ctrl & HIF_CTRL_RX_CHECKSUMMED))
		{
			skb->ip_summed = CHECKSUM_UNNECESSARY;
#ifdef VWD_DEBUG_STATS
			priv->rx_csum_correct++;
#endif
		}
#if defined(CONFIG_INET_IPSEC_OFFLOAD) || defined(CONFIG_INET6_IPSEC_OFFLOAD)
                        if (rx_ctrl & HIF_CTRL_RX_IPSEC_IN) {
                                if (ipsec_hdr) {
                                        struct sec_path *sp;
                                        struct xfrm_state *x;
                                        unsigned short *sah = &ipsec_hdr->sa_handle[0];
                                        int i = 0;

                                        sp = secpath_dup(skb->sp);

                                        if (!sp)
                                        {
                                                goto pkt_drop;
                                        }

                                        skb->sp = sp;

                                        /* at maximum 2 SA are expected */
                                        while (i <= 1)
                                        {
                                                if(!*sah)
                                                        break;

                                                if ((x = xfrm_state_lookup_byhandle(dev_net(vap->dev), ntohs(*sah))) == NULL)
                                                {
                                                        goto pkt_drop;
                                                }

                                                sp->xvec[i] = x;

                                                if (!x->curlft.use_time)
                                                {
                                                        ktime = current_kernel_time();
                                                        x->curlft.use_time = (unsigned long)ktime.tv_sec;
                                                }

                                                i++; sah++;
                                        }

                                        sp->len = i;
                                }
                        }
#endif

	}
	else
	{
#if defined(CONFIG_COMCERTO_DMA_COHERENT_SKB)
		skb = __build_skb(buf_addr, PFE_BUF_SIZE);
		if (skb) {
			skb->dma_coherent = 1;
		}
#else
		skb = build_skb(buf_addr, 0);
#endif
		if (unlikely(!skb)) {
#ifdef VWD_DEBUG_STATS
			priv->rx_skb_alloc_fail += 1;
#endif
			goto pkt_drop;
		}
		skb->data = buf_addr;

		skb_reserve(skb, offset);
		skb_put(skb, length);
	}


	return skb;

pkt_drop:
	if (skb) {
		kfree_skb(skb);
	} else {
#if defined(CONFIG_COMCERTO_DMA_COHERENT_SKB)
		dma_free_coherent(NULL, PFE_BUF_SIZE, buf_addr, 0);
#else
		kfree(buf_addr);
#endif
	}

out:
	return NULL;
}

/** pfe_vwd_send_to_vap
 *
 */

/* The most of the logic inside this function is copied from dev_queue_xmit() in linux/net/core/dev.c.*/

static void pfe_vwd_send_to_vap(struct vap_desc_s *vap, struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_queue *txq;
	int cpu, rc;

	if (!vap->direct_tx_path) {
		dev_queue_xmit(skb);
		//pr_debug("%s: !vap->direct_tx_path: use slowpath\n", __func__);
		return;
	}

	/* Disable soft irqs for various locks below. Also
         * stops preemption for RCU.
         */
        rcu_read_lock_bh();

#if 0
	if (unlikely(dev->real_num_tx_queues != 1)) {
		//pr_debug("%s: number of Tx queues = %d: use slowpath\n", __func__, dev->real_num_tx_queues);
		goto deliver_slow;
	}
#endif

	if (dev->flags & IFF_UP) {
		skb_set_queue_mapping(skb, 0);
		txq = netdev_get_tx_queue(dev, 0);

		cpu = smp_processor_id();

		if (txq->xmit_lock_owner != cpu) {
 			HARD_TX_LOCK(dev, txq, cpu);

			if (unlikely(netif_tx_queue_stopped(txq))) {
				//pr_debug("%s: Tx queue stopped: use slowpath\n", __func__);
 				HARD_TX_UNLOCK(dev, txq);
				goto deliver_slow;
			}

			rc = dev->netdev_ops->ndo_start_xmit(skb, dev);

			if (dev_xmit_complete(rc)) {
 				HARD_TX_UNLOCK(dev, txq);
				goto done;
			}
			HARD_TX_UNLOCK(dev, txq);
		}
	}

	rcu_read_unlock_bh();
	kfree_skb(skb);

	return;

done:
	//pr_debug("%s: delivered packet through fastpath\n", __func__);
	rcu_read_unlock_bh();
	return;

deliver_slow:
	rcu_read_unlock_bh();

	/* deliver packet to vap through stack */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	dev_queue_xmit(skb);
#else
	original_dev_queue_xmit(skb);
#endif
	return;
}

/** pfe_vwd_rx_poll
 *
 */
static int pfe_vwd_rx_poll( struct vap_desc_s *vap, struct napi_struct *napi, int qno, int budget)
{
	struct sk_buff *skb;
	int work_done = 0;
	struct net_device *dev;
	struct pfe_vwd_priv_s *priv = vap->priv;

	//printk(KERN_INFO"%s\n", __func__);
	dev = dev_get_by_index(&init_net, vap->ifindex);

#ifdef PFE_VWD_NAPI_STATS
	priv->napi_counters[NAPI_POLL_COUNT]++;
#endif
	do {
		unsigned int ctrl = 0;

		if (page_mode)
			skb = pfe_vwd_rx_page(vap, qno, &ctrl);
		else
			skb = pfe_vwd_rx_skb(vap, qno, &ctrl);

		if (!skb)
			break;
		if(!dev) {
			/*VAP got disappeared, simply drop the packet */
			kfree_skb(skb);
			work_done++;
			continue;
		}

		skb->dev = dev;
		dev->last_rx = jiffies;

#ifdef PFE_VWD_LRO_STATS
		priv->lro_len_counters[((u32)skb->len >> 11) & (LRO_LEN_COUNT_MAX - 1)]++;
		priv->lro_nb_counters[skb_shinfo(skb)->nr_frags & (LRO_NB_COUNT_MAX - 1)]++;
#endif
		vap->stats.rx_packets++;
		vap->stats.rx_bytes += skb->len;

		/*FIXME: Need to handle WiFi to WiFi fast path */
		if (ctrl & HIF_CTRL_RX_WIFI_EXPT) {
			//pr_debug("%s: HIF_CTRL_RX_WIFI_EXPT: use slowpath\n", __func__);

			*(unsigned long *)skb->head = 0xdead;
			skb->protocol = eth_type_trans(skb, dev);
#ifdef VWD_DEBUG_STATS
                        priv->pkts_slow_forwarded[qno] += 1;
#endif
			netif_receive_skb(skb);
		}
		else {
			struct ethhdr *hdr;

                        hdr = (struct ethhdr *)skb->data;
                        skb->protocol = hdr->h_proto;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))
                        skb->mac.raw = skb->data;
                        skb->nh.raw = skb->data + sizeof(struct ethhdr);
#else
                        skb_reset_mac_header(skb);
                        skb_set_network_header(skb, sizeof(struct ethhdr));
#endif

#ifdef VWD_DEBUG_STATS
			priv->pkts_rx_fast_forwarded[qno] += 1;
#endif
                        skb->priority = 0;


			pfe_vwd_send_to_vap(vap, skb, dev);
		}


		work_done++;
#ifdef PFE_VWD_NAPI_STATS
		priv->napi_counters[NAPI_PACKET_COUNT]++;
#endif
	} while (work_done < budget);

	if(dev)
		dev_put(dev);

	/* If no Rx receive nor cleanup work was done, exit polling mode.
	 * No more netif_running(dev) check is required here , as this is checked in
	 * net/core/dev.c ( 2.6.33.5 kernel specific).
	 */
	if (work_done < budget) {
		napi_complete(napi);

		hif_lib_event_handler_start(&vap->client, EVENT_RX_PKT_IND, qno);
	}
#ifdef PFE_VWD_NAPI_STATS
	else
		priv->napi_counters[NAPI_FULL_BUDGET_COUNT]++;
#endif

	return work_done;
}

/** pfe_vwd_lro_poll
 */
static int pfe_vwd_lro_poll(struct napi_struct *napi, int budget)
{
	struct vap_desc_s *vap = container_of(napi, struct vap_desc_s, lro_napi);


	return pfe_vwd_rx_poll(vap, napi, 2, budget);
}


/** pfe_eth_low_poll
 */
static int pfe_vwd_rx_high_poll(struct napi_struct *napi, int budget)
{
	struct vap_desc_s *vap = container_of(napi, struct vap_desc_s, high_napi);

	return pfe_vwd_rx_poll(vap, napi, 1, budget);
}

/** pfe_eth_high_poll
 */
static int pfe_vwd_rx_low_poll(struct napi_struct *napi, int budget )
{
	struct vap_desc_s *vap = container_of(napi, struct vap_desc_s, low_napi);

	return pfe_vwd_rx_poll(vap, napi, 0, budget);
}

/** pfe_vwd_event_handler
 */
static int pfe_vwd_event_handler(void *data, int event, int qno)
{
	struct vap_desc_s *vap = data;

	//printk(KERN_INFO "%s: %d\n", __func__, __LINE__);

	switch (event) {
		case EVENT_RX_PKT_IND:
			if (qno == 0) {
				if (napi_schedule_prep(&vap->low_napi)) {
					//printk(KERN_INFO "%s: schedule high prio poll\n", __func__);

					__napi_schedule(&vap->low_napi);
				}
			}
			else if (qno == 1) {
				if (napi_schedule_prep(&vap->high_napi)) {
					//printk(KERN_INFO "%s: schedule high prio poll\n", __func__);

					__napi_schedule(&vap->high_napi);
				}
			}
			else if (qno == 2) {
				if (napi_schedule_prep(&vap->lro_napi)) {
					//printk(KERN_INFO "%s: schedule lro poll\n", __func__);

					__napi_schedule(&vap->lro_napi);
				}
			}

#ifdef PFE_VWD_NAPI_STATS
			vap->priv->napi_counters[NAPI_SCHED_COUNT]++;
#endif
			break;

		case EVENT_TXDONE_IND:
		case EVENT_HIGH_RX_WM:
		default:
			break;
	}

	return 0;
}

/** pfe_vwd_fast_tx_timeout
 */
static enum hrtimer_restart pfe_vwd_fast_tx_timeout(struct hrtimer *timer)
{
	struct pfe_eth_fast_timer *fast_tx_timeout = container_of(timer, struct pfe_eth_fast_timer, timer);
	struct vap_desc_s *vap =  container_of(fast_tx_timeout->base, struct vap_desc_s, fast_tx_timeout);

	if(netif_queue_stopped(vap->dev)) {
#ifdef PFE_VWD_TX_STATS
		vap->was_stopped[fast_tx_timeout->queuenum] = 1;
#endif
		netif_wake_queue(vap->dev);
	}

	return HRTIMER_NORESTART;
}

/** pfe_eth_fast_tx_timeout_init
 */
static void pfe_vwd_fast_tx_timeout_init(struct vap_desc_s *vap)
{
	int i;
	for (i = 0; i < VWD_TXQ_CNT; i++) {
		vap->fast_tx_timeout[i].queuenum = i;
		hrtimer_init(&vap->fast_tx_timeout[i].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		vap->fast_tx_timeout[i].timer.function = pfe_vwd_fast_tx_timeout;
		vap->fast_tx_timeout[i].base = vap->fast_tx_timeout;
	}
}

static int pfe_vwd_might_stop_tx(struct vap_desc_s *vap, int queuenum, unsigned int n_desc)
{
	int tried = 0;
	ktime_t kt;

try_again:
	if (unlikely((__hif_tx_avail(&pfe->hif) < n_desc)
	|| (hif_lib_tx_avail(&vap->client, queuenum) < n_desc))) {

		if (!tried) {
			hif_tx_unlock(&pfe->hif);
			pfe_vwd_flush_txQ(vap, queuenum, 1, n_desc);
			tried = 1;
			hif_tx_lock(&pfe->hif);
			goto try_again;
		}
#ifdef PFE_VWD_TX_STATS
		if (__hif_tx_avail(&pfe->hif) < n_desc)
			vap->stop_queue_hif[queuenum]++;
		else if (hif_lib_tx_avail(&vap->client, queuenum) < n_desc) {
			vap->stop_queue_hif_client[queuenum]++;
		}
		vap->stop_queue_total[queuenum]++;
#endif
		netif_stop_queue(vap->dev);

		kt = ktime_set(0, COMCERTO_TX_FAST_RECOVERY_TIMEOUT_MS * NSEC_PER_MSEC);
		hrtimer_start(&vap->fast_tx_timeout[queuenum].timer, kt, HRTIMER_MODE_REL);
		return -1;
	}
	else {
		return 0;
	}
}

#define SA_MAX_OP 2

/**
 * pfe_vwd_xmit_local_packet()
 */
static int pfe_vwd_vap_xmit_local_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct vap_desc_s *vap;
	struct skb_shared_info *sh;
	int queuenum = 0, n_desc = 0, n_segs;
	int count = 0, nr_frags;
	int ii;
	u32 ctrl = HIF_CTRL_TX_WIFI;
#if defined(CONFIG_INET_IPSEC_OFFLOAD) || defined(CONFIG_INET6_IPSEC_OFFLOAD)
	u16 sah[SA_MAX_OP] = {0};
	struct hif_ipsec_hdr *hif_ipsec;
#endif
	u16 vapid = *(u16 *)(netdev_priv(dev));
	
	if (unlikely(vapid >= MAX_VAP_SUPPORT)) {
		printk(KERN_ERR "%s:%d VAPID (%d) is bigger than supported\n", __func__, __LINE__, vapid);
		kfree_skb(skb);
		return 0;
	}
	
	vap = &pfe->vwd.vaps[vapid];
	
	if (unlikely(vap->state != VAP_ST_OPEN)) {
		printk(KERN_ERR "%s:%d VAPID (%d %s) is not in open state\n", __func__, __LINE__, vapid, vap->ifname);
		kfree_skb(skb);
		return 0;
	}
		

	pfe_tx_get_req_desc(skb, &n_desc, &n_segs);

	spin_lock_bh(&pfe->vwd.vaplock);
	spin_lock_bh(&vap->tx_lock[queuenum]);
	hif_tx_lock(&pfe->hif);

	if (pfe_vwd_might_stop_tx(vap, queuenum, n_desc)){
	        hif_tx_unlock(&pfe->hif);
		spin_unlock_bh(&vap->tx_lock[queuenum]);
		spin_unlock_bh(&pfe->vwd.vaplock);
#ifdef PFE_VWD_TX_STATS
		if(vap->was_stopped[queuenum]) {
			vap->clean_fail[queuenum]++;
			vap->was_stopped[queuenum] = 0;
		}
#endif
                return NETDEV_TX_BUSY;
        }


	if ( !(skb_is_gso(skb)) && (skb_headroom(skb) < (PFE_PKT_HEADER_SZ + sizeof(unsigned long)))) {

		//printk(KERN_INFO "%s: copying skb %d\n", __func__, skb_headroom(skb));

		if (pskb_expand_head(skb, (PFE_PKT_HEADER_SZ + sizeof(unsigned long)), 0, GFP_ATOMIC)) {
			kfree_skb(skb);
			skb = NULL;
			goto out;
		}
	}

	/* Send vap_id to PFE */
	ctrl |= vap->vapid << HIF_CTRL_VAPID_OFST;
	sh = skb_shinfo(skb);

	if (skb_is_gso(skb)) {
		int nr_bytes;

		if(likely(nr_bytes = pfe_tso(skb, &vap->client, &pfe->vwd.tso, queuenum, ctrl))) {
			vap->stats.tx_packets += sh->gso_segs;
			vap->stats.tx_bytes += nr_bytes;
		}
		else
			vap->stats.tx_dropped++;

		skb = NULL;
		goto out;
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL) 
		ctrl |= HIF_CTRL_TX_CHECKSUM;

	nr_frags = sh->nr_frags;

#if defined(CONFIG_INET_IPSEC_OFFLOAD) || defined(CONFIG_INET6_IPSEC_OFFLOAD)
	/* check if packet sent from Host to PFE needs IPsec processing */
	if (skb->ipsec_offload)
	{
		if (skb->sp)
		{
			for (ii = skb->sp->len-1; ii >= 0; ii--)
			{
				struct xfrm_state *x = skb->sp->xvec[ii];
				sah[ii] = htons(x->handle);
			}

			ctrl |= HIF_CTRL_TX_IPSEC_OUT;

			/* add SA info to the hif header*/
			hif_ipsec = (struct hif_ipsec_hdr *)(skb->data - sizeof(struct hif_ipsec_hdr));
			hif_ipsec->sa_handle[0] = sah[0];
			hif_ipsec->sa_handle[1] = sah[1];

			skb->data -= sizeof(struct hif_ipsec_hdr);
			skb->len += sizeof(struct hif_ipsec_hdr);
		}
		else
			printk(KERN_ERR "%s: secure path data not found\n", __func__);
	}
#endif

	if (nr_frags) {
		skb_frag_t *f;

		__hif_lib_xmit_pkt(&vap->client, queuenum, skb->data, skb_headlen(skb), ctrl, HIF_FIRST_BUFFER, skb);

		for (ii = 0; ii < nr_frags - 1; ii++) {
			f = &sh->frags[ii];

			__hif_lib_xmit_pkt(&vap->client, queuenum, skb_frag_address(f), skb_frag_size(f), 0x0, 0x0, skb);
		}

		f = &sh->frags[ii];

		__hif_lib_xmit_pkt(&vap->client, queuenum, skb_frag_address(f), skb_frag_size(f), 0x0, HIF_LAST_BUFFER|HIF_DATA_VALID, skb);

#ifdef VWD_DEBUG_STATS
			pfe->vwd.pkts_local_tx_sgs += 1;
#endif
	}
	else
	{
		__hif_lib_xmit_pkt(&vap->client, queuenum, skb->data, skb->len, ctrl, HIF_FIRST_BUFFER | HIF_LAST_BUFFER | HIF_DATA_VALID, skb);
	}

	hif_tx_dma_start();

#ifdef VWD_DEBUG_STATS
	pfe->vwd.pkts_total_local_tx += 1;
#endif
	vap->stats.tx_packets++;
	vap->stats.tx_bytes += skb->len;

	//printk(KERN_INFO "%s: pkt sent successfully skb:%p len:%d\n", __func__, skb, skb->len);

out:
	hif_tx_unlock(&pfe->hif);

	dev->trans_start = jiffies;

	// Recycle buffers if a socket's send buffer becomes half full or if the HIF client queue starts filling up
	if (((count = (hif_lib_tx_pending(&vap->client, queuenum) - HIF_CL_TX_FLUSH_MARK)) > 0)
		|| (skb && skb->sk && ((sk_wmem_alloc_get(skb->sk) << 1) > skb->sk->sk_sndbuf)))
		pfe_vwd_flush_txQ(vap, queuenum, 1, count);

	spin_unlock_bh(&vap->tx_lock[queuenum]);
	spin_unlock_bh(&pfe->vwd.vaplock);

	return 0;
}

/**
 * pfe_vwd_get_stats()
 */

static struct net_device_stats *pfe_vwd_vap_get_stats(struct net_device *dev)
{
	u16 vapid = *(u16 *)netdev_priv(dev);
	struct vap_desc_s *vap;

	if (vapid >= MAX_VAP_SUPPORT)
		return NULL;

	vap = &pfe->vwd.vaps[vapid];

	return &vap->stats;
}

static const struct net_device_ops vwd_netdev_ops = {
	.ndo_start_xmit = pfe_vwd_vap_xmit_local_packet,
	.ndo_get_stats = pfe_vwd_vap_get_stats,
};

/**
 * pfe_vwd_vap_get_drvinfo -  Fills in the drvinfo structure with some basic info 
 *
 */
static void pfe_vwd_vap_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *drvinfo)
{
	strncpy(drvinfo->driver, "VWD", COMCERTO_INFOSTR_LEN);
	strncpy(drvinfo->version, "1.0", COMCERTO_INFOSTR_LEN);
	strncpy(drvinfo->fw_version, "N/A", COMCERTO_INFOSTR_LEN);
	strncpy(drvinfo->bus_info, "N/A", COMCERTO_INFOSTR_LEN);
	drvinfo->testinfo_len = 0;
	drvinfo->regdump_len = 0;
	drvinfo->eedump_len = 0;
}

/** pfe_vwd_vap_configure
 *
 */
static int pfe_vwd_vap_configure(struct pfe_vwd_priv_s *priv, struct vap_desc_s *vap, struct vap_cmd_s *cmd)
{
	struct net_device *dev;
	int ii, rc, vap_spin_locked = 0;
	char name[IFNAMSIZ]; 

	printk("%s:%d\n", __func__, __LINE__);

	BUG_ON(vap->dev);

	vap->state = VAP_ST_CONFIGURING;

	if (spin_is_locked(&priv->vaplock)) {
		vap_spin_locked = 1;
		spin_unlock_bh(&priv->vaplock);
	}

	sprintf(name, "vwd%d", cmd->vapid);
	dev = alloc_etherdev(sizeof(u16));

	if (!dev) {
		printk(KERN_ERR "%s : Unable to allocate device structure for %s\n", __func__, name);
		goto err0;
	}

	*(u16 *)(netdev_priv(dev)) = cmd->vapid;	

	vap->vapid = cmd->vapid;
	vap->ifindex = cmd->ifindex;
	vap->direct_rx_path = (cmd->cmd_flags & VAP_CMD_ENABLE_DIRECT_PATH_RX) ? 1 : 0;
	vap->direct_tx_path = (cmd->cmd_flags & VAP_CMD_ENABLE_DIRECT_PATH_TX) ? 1 : 0;
	memcpy(vap->ifname, cmd->ifname, 12);
	memcpy(vap->macaddr, cmd->macaddr, ETH_ALEN);
	vap->dev = dev;
	vap->priv = priv;
	dev->mtu = 1500;
	dev->flags |= IFF_NOARP;
	dev->netdev_ops = &vwd_netdev_ops;

	if (dev_alloc_name(dev, name) < 0) {
		netdev_err(dev, "%s: dev_alloc_name(%s) failed\n", __func__, name);
		goto err1;
	}
	vap->cpu_id = -1;
	dev->features = dev->hw_features = priv->vap_dev_features; 

	/* We are already in rtnl_lock context */
	rc = register_netdevice(vap->dev);


	if (rc) {
		netdev_err(vap->dev, "register_netdev() failed\n");
		goto err1;
	}

	vap->vap_kobj = kobject_create_and_add(vap->ifname, &pfe->dev->kobj);
	if (!vap->vap_kobj) {
		printk(KERN_ERR "%s : Failed to create kobject\n", __func__);
		goto err2;
	}

	if (sysfs_create_group(vap->vap_kobj, &vap_attr_group)) {
		printk(KERN_ERR "%s : Failed to create sysfs entries \n", __func__);
		goto err3;
	}

	for (ii = 0; ii < VWD_TXQ_CNT; ii++)
		spin_lock_init(&vap->tx_lock[ii]);


	printk(KERN_INFO "%s:ADD: name:%s, vapid:%d, direct_rx_path : %s, ifindex:%d, mac:%x:%x:%x:%x:%x:%x\n",
			__func__, vap->ifname, vap->vapid,
			vap->direct_rx_path ? "ON":"OFF", vap->ifindex,
			vap->macaddr[0], vap->macaddr[1],
			vap->macaddr[2], vap->macaddr[3],
			vap->macaddr[4], vap->macaddr[5] );

	if (vap_spin_locked)
		spin_lock_bh(&priv->vaplock);

	dev_activate(vap->dev);
	vap->state = VAP_ST_CONFIGURED;
	return 0;

err3:
	kobject_put(vap->vap_kobj);

err2:
	unregister_netdevice(vap->dev);
err1:
	free_netdev(dev);
err0:
	if (vap_spin_locked)
		spin_lock_bh(&priv->vaplock);
	

	vap->state = VAP_ST_CLOSE;

	return -1;
	
}

/** pfe_vwd_vap_up
 *
 */

static int pfe_vwd_vap_up(struct pfe_vwd_priv_s *priv, struct vap_desc_s *vap, struct vap_cmd_s *cmd, struct net_device *wifi_dev)
{
	struct hif_client_s *client;

	printk("%s:%d\n", __func__, __LINE__);

	vap->ifindex = cmd->ifindex;
	vap->direct_rx_path = (cmd->cmd_flags & VAP_CMD_ENABLE_DIRECT_PATH_RX) ? 1 : 0;
	vap->direct_tx_path = (cmd->cmd_flags & VAP_CMD_ENABLE_DIRECT_PATH_TX) ? 1 : 0;
	memcpy(vap->macaddr, cmd->macaddr, ETH_ALEN);

	vap->wifi_dev = wifi_dev;
	
	/* Register VWD Client driver with HIF */
	client = &vap->client;
	memset(client, 0, sizeof(*client));
	client->id = PFE_CL_VWD0 + vap->vapid;
	client->tx_qn = VWD_TXQ_CNT;
	client->rx_qn = VWD_RXQ_CNT;
	client->priv    = vap;
	client->pfe     = pfe;
	client->event_handler   = pfe_vwd_event_handler;
	client->user_cpu_id  = vap->cpu_id;

	/* FIXME : For now hif lib sets all tx and rx queues to same size */
	client->tx_qsize = EMAC_TXQ_DEPTH;
	client->rx_qsize = EMAC_RXQ_DEPTH;

	if (hif_lib_client_register(client)) {
		printk(KERN_ERR"%s: hif_lib_client_register(%d) failed\n", __func__, client->id);
		return -1;
	}


	/* Initilize NAPI for Rx processing */
	netif_napi_add(vap->dev, &vap->low_napi, pfe_vwd_rx_low_poll, VWD_RX_POLL_WEIGHT);
	netif_napi_add(vap->dev, &vap->high_napi, pfe_vwd_rx_high_poll, VWD_RX_POLL_WEIGHT);
	netif_napi_add(vap->dev, &vap->lro_napi, pfe_vwd_lro_poll, VWD_RX_POLL_WEIGHT);
	napi_enable(&vap->high_napi);
	napi_enable(&vap->low_napi);
	napi_enable(&vap->lro_napi);
	pfe_vwd_fast_tx_timeout_init(vap);

	if (!priv->vap_count) {
		printk("%s: Tx recover Timer started...\n", __func__);
		if (!timer_pending(&priv->tx_timer))
			add_timer(&priv->tx_timer);
	}
	
	priv->vap_count++;

	if (vwd_ofld) {
		dev_get_by_index(&init_net, vap->dev->ifindex);
#ifdef CONFIG_PFE_WIFI_OFFLOAD
		wifi_dev->wifi_offload_dev = vap->dev;
#endif
		vap->wifi_ethtool_ops = wifi_dev->ethtool_ops;

		if (!wifi_dev->ethtool_ops) {
			wifi_dev->ethtool_ops = &pfe_vwd_vap_ethtool_ops;
		}
		vap->wifi_hw_features = wifi_dev->hw_features;
		vap->wifi_features = wifi_dev->features;

		wifi_dev->hw_features |= priv->vap_dev_hw_features;
		vap->dev->features = vap->dev->hw_features = priv->vap_dev_features; 

		set_bit(__LINK_STATE_START, &vap->dev->state);
		vap->dev->flags |= IFF_UP;
		netif_tx_wake_all_queues(vap->dev);
	
	}
	

	printk(KERN_INFO "%s: UP: name:%s, vapid:%d, direct_rx_path : %s, ifindex:%d, mac:%x:%x:%x:%x:%x:%x\n",
			__func__, vap->ifname, vap->vapid,
			vap->direct_rx_path ? "ON":"OFF", vap->ifindex,
			vap->macaddr[0], vap->macaddr[1],
			vap->macaddr[2], vap->macaddr[3],
			vap->macaddr[4], vap->macaddr[5] );

	vap->state = VAP_ST_OPEN;

	return 0;
}

/** pfe_vwd_vap_down
 *
 */
static void pfe_vwd_vap_down(struct pfe_vwd_priv_s *priv, struct vap_desc_s *vap)
{
	int i;
	int tx_pkts;
	struct net_device *dev = vap->dev, *wifi_dev;
	unsigned long end = jiffies + (TX_POLL_TIMEOUT_MS * HZ) / 1000;

	printk("%s:%d\n", __func__, __LINE__);
	printk(KERN_INFO "%s:DOWN: name:%s, vapid:%d, direct_rx_path : %s, ifindex:%d, mac:%x:%x:%x:%x:%x:%x\n",
			__func__, vap->ifname, vap->vapid,
			vap->direct_rx_path ? "ON":"OFF", vap->ifindex,
			vap->macaddr[0], vap->macaddr[1],
			vap->macaddr[2], vap->macaddr[3],
			vap->macaddr[4], vap->macaddr[5] );

	vap->state = VAP_ST_CONFIGURED;
	pfe->vwd.vap_count--;
	netif_stop_queue(vap->dev);
	napi_disable(&vap->high_napi);
	napi_disable(&vap->low_napi);
	napi_disable(&vap->lro_napi);
	netif_napi_del(&vap->high_napi);
	netif_napi_del(&vap->low_napi);
	netif_napi_del(&vap->lro_napi);

	for (i = 0; i < VWD_TXQ_CNT; i++)
		hrtimer_cancel(&vap->fast_tx_timeout[i].timer);


	do {
		tx_pkts = 0;
		pfe_vwd_flush_tx(vap, 1);

		for (i = 0; i < VWD_TXQ_CNT; i++)
			tx_pkts += hif_lib_tx_pending(&vap->client, i);

		if (tx_pkts) {
			/* This is atomic context, we cann't call schedule */
			udelay(100);

			/*Don't wait forever, break if we cross max timeout */
			if (time_after(jiffies, end)) {
				printk(KERN_ERR "(%s)Tx is not complete after %dmsec\n", dev->name, TX_POLL_TIMEOUT_MS);
				break;
			}

			printk(KERN_INFO "%s : (%s) Waiting for tx packets to free. Pending tx pkts = %d.\n", __func__, dev->name, tx_pkts);
		}
	} while(tx_pkts);

	hif_lib_client_unregister(&vap->client);

	if (!pfe->vwd.vap_count) {
		printk("%s: Tx recover Timer stopped...\n", __func__);
		del_timer(&pfe->vwd.tx_timer);
	}
	
	/* FIXME Assuming that vwd_ofld is NAS mode */
	if (vwd_ofld) {
		clear_bit(__LINK_STATE_START, &vap->dev->state);
		vap->dev->flags &= ~(IFF_UP);
		if ((wifi_dev = dev_get_by_name(&init_net, vap->ifname))) {
#ifdef CONFIG_PFE_WIFI_OFFLOAD
			if (wifi_dev->wifi_offload_dev) {
				wifi_dev->ethtool_ops = vap->wifi_ethtool_ops;
				wifi_dev->wifi_offload_dev = NULL;
				wifi_dev->hw_features = vap->wifi_hw_features;
				wifi_dev->features = vap->wifi_features;

			}
#endif

			dev_put(wifi_dev);
		}

		vap->wifi_dev = NULL;

		dev_put(vap->dev);
	}

}


/** pfe_vwd_handle_vap
 *
 */
static int pfe_vwd_handle_vap( struct pfe_vwd_priv_s *priv, struct vap_cmd_s *cmd )
{
	struct net_device *wifi_dev;
	struct vap_desc_s *vap;
	int rc = 0, ii;

	printk(KERN_INFO "%s function called %d: %s\n", __func__, cmd->action, cmd->ifname);

	if (cmd->vapid >= MAX_VAP_SUPPORT) {
		printk(KERN_ERR "%s : VAPID (%d)  >=  MAX_VAP_SUPPORT(%d)\n", __func__, cmd->vapid, MAX_VAP_SUPPORT);
		return -1;
	}

	vap = &priv->vaps[cmd->vapid];

	//if ((cmd->action != CONFIGURE) && (strcmp(vap->ifname, cmd->ifname))) {
	//	printk(KERN_ERR "%s : VAP (id : %d  name : %s) is not valid\n", __func__, cmd->vapid, cmd->ifname);
	//	return -1;
	//}	

	switch (cmd->action) {

		case CONFIGURE:
			printk(KERN_INFO "%s: CONFIGURE ... %s\n", __func__, cmd->ifname);
			if (vap->state != VAP_ST_CLOSE) {
				printk("%s : VAP (id : %d  name : %s) is not in close state\n",
						__func__, cmd->vapid, cmd->ifname);
				rc = -1;
				break;
			}

			if (!(rc = pfe_vwd_vap_configure(priv, vap, cmd))) 
				printk(KERN_INFO "%s: Configured VAP (id : %d  name : %s)\n", 
								__func__, cmd->vapid, cmd->ifname);
			else
				printk(KERN_ERR "%s: Failed to configure VAP (id : %d  name : %s)\n", 
									__func__, cmd->vapid, cmd->ifname);
			break;

		case ADD:
			printk(KERN_INFO "%s: ADD ... %s\n", __func__, cmd->ifname);
			if (vap->state != VAP_ST_CONFIGURED) {
				printk(KERN_ERR "%s : VAP (id : %d  name : %s) is not configured \n",
								 __func__, cmd->vapid, cmd->ifname);
				rc = -1;
				break;
			}

			if ((wifi_dev = dev_get_by_name(&init_net, vap->ifname))) {
				if (!(wifi_dev->flags & IFF_UP)) {
					printk(KERN_ERR "%s : Failed to open VAP (id : %d  name : %s) WiFi device is not in UP state\n",
								 __func__, cmd->vapid, cmd->ifname);
					rc = -1;
					
				} else if (pfe_vwd_vap_up(priv, vap, cmd, wifi_dev)) {
					printk(KERN_ERR "%s : Failed to open VAP (id : %d  name : %s)\n",
								 __func__, cmd->vapid, cmd->ifname);
					rc = -1;
				}
					
				dev_put(wifi_dev);
			}
			else
				printk(KERN_ERR "%s : Invalid WiFi device %s\n", __func__, vap->ifname);
				
			break;

		case REMOVE:
			printk(KERN_INFO "%s: REMOVE ... %s\n", __func__, cmd->ifname);
			if (vap->state != VAP_ST_OPEN) {
				printk(KERN_ERR "%s : VAP (id : %d  name : %s) is not opened \n",
								 __func__, cmd->vapid, cmd->ifname);
				rc = -1;
				break;
			}
			pfe_vwd_vap_down(priv, vap);
			break;

		case UPDATE:
			printk(KERN_INFO "%s: UPDATE ... %s\n", __func__, cmd->ifname);
			vap->ifindex = cmd->ifindex;
			vap->direct_rx_path = (cmd->cmd_flags & VAP_CMD_ENABLE_DIRECT_PATH_RX) ? 1 : 0;
			vap->direct_tx_path = (cmd->cmd_flags & VAP_CMD_ENABLE_DIRECT_PATH_TX) ? 1 : 0;
			memcpy(vap->macaddr, cmd->macaddr, ETH_ALEN);
			break;

		case RESET:
			printk(KERN_INFO "%s: RESET ...\n", __func__);
			for (ii = 0; ii < MAX_VAP_SUPPORT; ii++) {
				vap = &priv->vaps[ii];
				
				if (vap->state == VAP_ST_CLOSE)
					continue;

				if (vap->state == VAP_ST_OPEN)
					pfe_vwd_vap_down(priv, vap);
		
				if (vap->state == VAP_ST_CONFIGURED) {
					struct net_device *dev;
					sysfs_remove_group(vap->vap_kobj, &vap_attr_group);
					kobject_put(vap->vap_kobj);
					dev = vap->dev;
					vap->dev = NULL;
					vap->state = VAP_ST_CLOSE;
					spin_unlock_bh(&priv->vaplock);
					dev_deactivate(dev);
					/* We are already in rtnl_lock context*/
					unregister_netdevice(dev);
					rtnl_unlock();
					free_netdev(dev);
					rtnl_lock();
					spin_lock_bh(&priv->vaplock);
				}
			}
			break;
	} 


	return rc;
}

#define SIOCVAPUPDATE  ( 0x6401 )

/** pfe_vwd_ioctl
 *
 */
static long
pfe_vwd_ioctl(struct file * file, unsigned int cmd, unsigned long arg)
{
	struct vap_cmd_s vap_cmd;
	void __user *argp = (void __user *)arg;
	int rc = -EOPNOTSUPP;
	struct pfe_vwd_priv_s *priv = (struct pfe_vwd_priv_s *)file->private_data;

	rtnl_lock();
	spin_lock_bh(&priv->vaplock);
	printk("%s: start\n", __func__);
	switch(cmd) {
		case SIOCVAPUPDATE:
			if (copy_from_user(&vap_cmd, argp, sizeof(struct vap_cmd_s))) {
				rc = -EFAULT;
				goto done;
			}

			rc = pfe_vwd_handle_vap(priv, &vap_cmd);
	}
	printk("%s: end\n", __func__);
	spin_unlock_bh(&priv->vaplock);
	rtnl_unlock();

done:

	return rc;
}


/** vwd_open
 *
 */
	static int
pfe_vwd_open(struct inode *inode, struct file *file)
{
	int result = 0;
	unsigned int dev_minor = MINOR(inode->i_rdev);

#if defined (CONFIG_COMCERTO_VWD_MULTI_MAC)
	printk( "%s :  Multi MAC mode enabled\n", __func__);
#endif
	printk( "%s :  minor device -> %d\n", __func__, dev_minor);
	if (dev_minor != 0)
	{
		printk(KERN_ERR ": trying to access unknown minor device -> %d\n", dev_minor);
		result = -ENODEV;
		goto out;
	}

	file->private_data = &pfe->vwd;

out:
	return result;
}

/** vwd_close
 *
 */
	static int
pfe_vwd_close(struct inode * inode, struct file * file)
{
	printk( "%s \n", __func__);

	return 0;
}

struct file_operations vwd_fops = {
unlocked_ioctl:	pfe_vwd_ioctl,
		open:		pfe_vwd_open,
		release:	pfe_vwd_close,
};


/** pfe_vwd_up
 *
 */
static int pfe_vwd_up(struct pfe_vwd_priv_s *priv )
{
	printk("%s: start\n", __func__);

	nf_register_hook(&vwd_hook);
	nf_register_hook(&vwd_hook_ipv6);

	if (pfe_vwd_sysfs_init(priv))
		goto err0;

	priv->fast_path_enable = 0;
	priv->fast_bridging_enable = 0;
	priv->fast_routing_enable = 1;

#ifdef CONFIG_PFE_WIFI_OFFLOAD
	comcerto_wifi_rx_fastpath_register(vwd_wifi_if_send_pkt);
#endif

	if (vwd_ofld == PFE_VWD_NAS_MODE) {
		register_netdevice_notifier(&vwd_vap_notifier);
	}
	
	/* supported features */
	priv->vap_dev_hw_features = 
			NETIF_F_RXCSUM | NETIF_F_IP_CSUM |  NETIF_F_IPV6_CSUM |	
			NETIF_F_SG | NETIF_F_TSO;

	/* enabled by default */
	if (lro_mode) {
		priv->vap_dev_hw_features |= NETIF_F_LRO;
	}

	priv->vap_dev_features = priv->vap_dev_hw_features; 

	printk("%s: End\n", __func__);
	return 0;

err0:
	nf_unregister_hook(&vwd_hook);
	nf_unregister_hook(&vwd_hook_ipv6);

	return -1;
}

/** pfe_vwd_down
 *
 */
static int pfe_vwd_down( struct pfe_vwd_priv_s *priv )
{
	int ii;

	printk(KERN_INFO "%s: %s\n", priv->name, __func__);

#ifdef CONFIG_PFE_WIFI_OFFLOAD
	comcerto_wifi_rx_fastpath_unregister();
#endif

	if( priv->fast_bridging_enable )
	{
		nf_unregister_hook(&vwd_hook_bridge);
	}

	if( priv->fast_routing_enable )
	{
		nf_unregister_hook(&vwd_hook);
		nf_unregister_hook(&vwd_hook_ipv6);
	}

	/*Stop Tx recovery timer and cleanup all vaps*/
	if (priv->vap_count) {
		printk("%s: Tx recover Timer stopped...\n", __func__);
		del_timer_sync(&priv->tx_timer);
	}
	
	for (ii = 0; ii < MAX_VAP_SUPPORT; ii++)
	{
		struct vap_desc_s *vap = &priv->vaps[ii];
		struct net_device *wifi_dev = NULL;

		if (vap->state == VAP_ST_OPEN) {
			pfe_vwd_vap_down(priv, vap);
		}
		
		if (vap->state == VAP_ST_CONFIGURED) {

			wifi_dev = dev_get_by_name(&init_net, vap->ifname);

			if (wifi_dev) {		
#ifdef CONFIG_PFE_WIFI_OFFLOAD
				if (wifi_dev->wifi_offload_dev) {
					wifi_dev->ethtool_ops = vap->wifi_ethtool_ops;
					wifi_dev->wifi_offload_dev = NULL;
					wifi_dev->hw_features = vap->wifi_hw_features;
					wifi_dev->features = vap->wifi_features;
				}
#endif
				dev_put(wifi_dev);
			}

			sysfs_remove_group(vap->vap_kobj, &vap_attr_group);
			kobject_put(vap->vap_kobj);
			dev_deactivate(vap->dev);
			unregister_netdev(vap->dev);
			free_netdev(vap->dev);
			vap->state = VAP_ST_CLOSE;
		}
	}

	if (vwd_ofld == PFE_VWD_NAS_MODE) {
		unregister_netdevice_notifier(&vwd_vap_notifier);
	}


	priv->vap_count = 0;
	pfe_vwd_sysfs_exit();


	return 0;
}

/** pfe_vwd_driver_init
 *
 *	 PFE wifi offload:
 *	 - uses HIF functions to receive/send packets
 */

static int pfe_vwd_driver_init( struct pfe_vwd_priv_s *priv )
{
	int rc;
	printk("%s: start\n", __func__);

	strcpy(priv->name, "vwd");

	spin_lock_init(&priv->vaplock);
	priv->pfe = pfe;

	rc = pfe_vwd_up(priv);
	printk("%s: end\n", __func__);
	return rc;
}

/** vwd_driver_remove
 *
 */
static int pfe_vwd_driver_remove(void)
{
	struct pfe_vwd_priv_s *priv = &pfe->vwd;

	pfe_vwd_down(priv);

	return 0;
}

/** pfe_vwd_init
 *
 */
int pfe_vwd_init(struct pfe *pfe)
{
	struct pfe_vwd_priv_s	*priv ;
	int rc = 0;

	printk(KERN_INFO "%s\n", __func__);
	priv  = &pfe->vwd;
	memset(priv, 0, sizeof(*priv));


	rc = alloc_chrdev_region(&priv->char_devno,VWD_MINOR, VWD_MINOR_COUNT, VWD_DRV_NAME);
	if (rc < 0) {
		printk(KERN_ERR "%s: alloc_chrdev_region() failed\n", __func__);
		goto err0;
	}

	cdev_init(&priv->char_dev, &vwd_fops);
	priv->char_dev.owner = THIS_MODULE;

	rc = cdev_add (&priv->char_dev, priv->char_devno, VWD_DEV_COUNT);
	if (rc < 0) {
		printk(KERN_ERR "%s: cdev_add() failed\n", __func__);
		goto err1;
	}

	printk(KERN_INFO "%s: created vwd device(%d, %d)\n", __func__, MAJOR(priv->char_devno),
			MINOR(priv->char_devno));

	priv->pfe = pfe;

	priv->tx_timer.data = (unsigned long)priv;
	priv->tx_timer.function = pfe_vwd_tx_timeout;
	priv->tx_timer.expires = jiffies + ( COMCERTO_TX_RECOVERY_TIMEOUT_MS * HZ )/1000;
	init_timer(&priv->tx_timer);

	if( pfe_vwd_driver_init( priv ) )
		goto err2;

	return 0;

err2:
	cdev_del(&priv->char_dev);

err1:
	unregister_chrdev_region(priv->char_devno, VWD_MINOR_COUNT);

err0:
	return rc;
}

/** pfe_vwd_exit
 *
 */
void pfe_vwd_exit(struct pfe *pfe)
{
	struct pfe_vwd_priv_s	*priv = &pfe->vwd;

	printk(KERN_INFO "%s\n", __func__);

	pfe_vwd_driver_remove();
	cdev_del(&priv->char_dev);
	unregister_chrdev_region(priv->char_devno, VWD_MINOR_COUNT);
}

#else /* !CFG_WIFI_OFFLOAD */

/** pfe_vwd_init
 *
 */
int pfe_vwd_init(struct pfe *pfe)
{
	printk(KERN_INFO "%s\n", __func__);
	return 0;
}

/** pfe_vwd_exit
 *
 */
void pfe_vwd_exit(struct pfe *pfe)
{
	printk(KERN_INFO "%s\n", __func__);
}

#endif /* !CFG_WIFI_OFFLOAD */
