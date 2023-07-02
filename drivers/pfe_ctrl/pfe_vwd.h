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

#ifndef _PFE_VWD_H_
#define _PFE_VWD_H_

#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include "pfe_tso.h"


#define PFE_VWD_TX_STATS
#define PFE_VWD_LRO_STATS
#define PFE_VWD_NAPI_STATS
#define VWD_DEBUG_STATS
#define VWD_TXQ_CNT	16
#define VWD_RXQ_CNT	3

#define PFE_WIFI_PKT_HEADROOM   96 /*PFE inserts this headroom for WiFi tx packets only in lro mode */

#define VWD_MINOR               0
#define VWD_MINOR_COUNT         1
#define VWD_DRV_NAME            "vwd"
#define VWD_DEV_COUNT           1
#define VWD_RX_POLL_WEIGHT	HIF_RX_POLL_WEIGHT - 16
#define	WIFI_TOE_PE_ID	5

struct vap_desc_s {
	struct	kobject *vap_kobj;
	struct net_device *dev;
	struct net_device *wifi_dev;
	const struct ethtool_ops *wifi_ethtool_ops;
	struct pfe_vwd_priv_s *priv;
	unsigned int	ifindex;
	unsigned int	wifi_hw_features;
	unsigned int	wifi_features;
	unsigned int	state;
#define VAP_ST_CLOSE		0
#define VAP_ST_OPEN		1
#define VAP_ST_CONFIGURED	2
#define VAP_ST_CONFIGURING	3
	struct hif_client_s client;
	struct napi_struct	low_napi;
	struct napi_struct	high_napi;
	struct napi_struct	lro_napi;
	spinlock_t tx_lock[VWD_TXQ_CNT];
	struct sk_buff *skb_inflight[VWD_RXQ_CNT + 6];
	struct pfe_eth_fast_timer fast_tx_timeout[VWD_TXQ_CNT];
	struct net_device_stats	stats;
	int cpu_id;
	char  ifname[IFNAMSIZ];
	unsigned char  macaddr[ETH_ALEN];
	unsigned short vapid;
        unsigned short programmed;
        unsigned short bridged;
	unsigned short  direct_rx_path;          /* Direct path support from offload device=>VWD */
	unsigned short  direct_tx_path;          /* Direct path support from offload VWD=>device */
	unsigned short  custom_skb;   	      /* Customized skb model from VWD=>offload_device */
#ifdef PFE_VWD_TX_STATS
	unsigned int stop_queue_total[VWD_TXQ_CNT];
	unsigned int stop_queue_hif[VWD_TXQ_CNT];
	unsigned int stop_queue_hif_client[VWD_TXQ_CNT];
	unsigned int clean_fail[VWD_TXQ_CNT];
	unsigned int was_stopped[VWD_TXQ_CNT];
#endif
};



struct vap_cmd_s {
	int 		action;
#define 	ADD 		0
#define 	REMOVE		1
#define 	UPDATE		2
#define 	RESET		3
#define 	CONFIGURE	4
	int     	ifindex;
	unsigned short	vapid;
	unsigned short  cmd_flags;
#define VAP_CMD_ENABLE_DIRECT_PATH_RX (1 << 0)
#define VAP_CMD_ENABLE_DIRECT_PATH_TX (1 << 1)
	unsigned char	ifname[IFNAMSIZ];
	unsigned char	macaddr[ETH_ALEN];
};

struct pfe_vwd_priv_s {

	struct pfe 		*pfe;
	unsigned char name[IFNAMSIZ];

	struct cdev char_dev;
	int char_devno;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	netdev_features_t vap_dev_hw_features;
	netdev_features_t vap_dev_features;
#else
	unsigned int vap_dev_hw_features;
	unsigned int vap_dev_features;
#endif

	struct vap_desc_s vaps[MAX_VAP_SUPPORT];
	int vap_count;
	spinlock_t vaplock;
	struct timer_list tx_timer;
	struct tso_cb_s 	tso;

#ifdef PFE_VWD_LRO_STATS
	unsigned int lro_len_counters[LRO_LEN_COUNT_MAX];
	unsigned int lro_nb_counters[LRO_NB_COUNT_MAX]; //TODO change to exact max number when RX scatter done
#endif
#ifdef PFE_VWD_NAPI_STATS
	unsigned int napi_counters[NAPI_MAX_COUNT];
#endif
	int fast_path_enable;
	int fast_bridging_enable;
	int fast_routing_enable;

#ifdef VWD_DEBUG_STATS
	u32 pkts_local_tx_sgs;
	u32 pkts_total_local_tx;
	u32 pkts_local_tx_csum;
	u32 pkts_transmitted;
	u32 pkts_slow_forwarded[VWD_RXQ_CNT];
	u32 pkts_tx_dropped;
	u32 pkts_rx_fast_forwarded[VWD_RXQ_CNT];
	u32 rx_skb_alloc_fail;
	u32 rx_csum_correct;
#endif

	u32 msg_enable;

};


int pfe_vwd_init(struct pfe *pfe);
void pfe_vwd_exit(struct pfe *pfe);

#endif /* _PFE_VWD_H_ */
