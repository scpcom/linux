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

#include <linux/platform_device.h>
#include <linux/kthread.h>
#include "pfe_mod.h"
#include "pfe_pcap.h"
#include "pfe_hif_lib.h"

#ifdef CFG_PCAP

/* PFE packet capture:
   - uses HIF functions to receive packets
   - uses ctrl function to control packet capture
 */

static ssize_t pcap_stats_get(struct device *, struct device_attribute *, char *);
static ssize_t pcap_stats_clear(struct device *, struct device_attribute *, const char *, size_t );

static int pcap_open(struct net_device *dev)
{
	printk(KERN_INFO "%s: %s\n", dev->name, __func__);
	netif_stop_queue(dev);
	return 0;
}
static int pcap_close(struct net_device *dev)
{
	printk(KERN_INFO "%s: %s\n", dev->name, __func__);
	return 0;
}

static int pcap_hard_start_xmit(struct sk_buff *skb,  struct net_device *dev )
{
	netif_stop_queue(dev);
	printk(KERN_INFO "%s() Dropping pkt!!!\n",__func__);
	kfree_skb(skb);
	return NETDEV_TX_OK;
}


static DEVICE_ATTR(pcap_stats, 0644, pcap_stats_get, pcap_stats_clear);



static int pcap_sysfs_init(struct device *dev)
{
	if (device_create_file(dev, &dev_attr_pcap_stats))
	{
		printk(KERN_ERR "Failed to create attr capture stats\n");
		goto err_stats;
	}

	return 0;

err_stats:
	return -1;
}

/** pfe_pcap_sysfs_exit
 *
 */
static void pcap_sysfs_exit(struct pfe* pfe)
{
	device_remove_file(pfe->dev, &dev_attr_pcap_stats);
}

static const struct net_device_ops pcap_netdev_ops = {
	.ndo_open = pcap_open,
	.ndo_stop = pcap_close,
	.ndo_start_xmit = pcap_hard_start_xmit,
};

static struct net_device *pcap_register_capdev(char *dev_name)
{
	struct net_device *dev=NULL;

	printk("%s:\n", __func__);

	/* Create an ethernet device instance */
	dev = (struct net_device *)alloc_etherdev(sizeof (int));
	if (!dev) {
		printk(KERN_ERR "%s: cap device allocation failed\n", __func__);
		goto err0;
	}

	strcpy(dev->name, dev_name);
	//dev->irq = priv->irq;

	/* Fill in the dev structure */
	dev->mtu = 1500;
	dev->features = 0;
	dev->netdev_ops = &pcap_netdev_ops;
	/*TODO */
	dev->dev_addr[0] = 0x0;
	dev->dev_addr[1] = 0x21;
	dev->dev_addr[2] = 0x32;
	dev->dev_addr[3] = 0x43;
	dev->dev_addr[4] = 0x54;
	dev->dev_addr[5] = 0x65;

	if(register_netdev(dev)) {
		printk(KERN_ERR "%s: cannot register net device, aborting.\n", dev->name);
		free_netdev(dev);
		dev = NULL;
	}

err0:
	return dev;
}
static void pcap_unregister_capdev(struct net_device *dev)
{
	unregister_netdev(dev);
	free_netdev(dev);
}

static ssize_t pcap_stats_get(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pfe* pfe =  platform_get_drvdata(pdev);
	struct pcap_priv_s *priv = &pfe->pcap;
	int ii, len = 0;

	//printk("rtc %d rtf %d\n", priv->rxQ.rxToCleanIndex,priv->rxQ.rxToFillIndex);
	for (ii = 0; ii < NUM_GEMAC_SUPPORT; ii++)
		len += sprintf(&buf[len], "GEMAC%d Rx pkts : %lu\n", ii, priv->stats[ii].rx_packets);

	return len;
}

static ssize_t pcap_stats_clear(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pfe* pfe =  platform_get_drvdata(pdev);
	struct pcap_priv_s *priv = &pfe->pcap;
	int ii;

	for (ii = 0; ii < NUM_GEMAC_SUPPORT; ii++)
		priv->stats[ii].rx_packets = 0;
	return count;
}



/** pfe_pcap_rx_poll
 *
 */
static int pfe_pcap_rx_poll (struct napi_struct *napi, int budget)
{
	struct sk_buff *skb;
	void *data_addr;
	int length,offset;
	unsigned int  idx = 0, work_done=0, qno=0;
	unsigned int desc_ctrl = 0,rx_ctrl;
	struct hif_pcap_hdr *pcap_hdr;
	u32 tstamp;
	struct pcap_priv_s *priv = container_of(napi, struct pcap_priv_s, low_napi);


	do{

		data_addr = hif_lib_receive_pkt(&priv->client, qno, &length, &offset, &rx_ctrl, &desc_ctrl, (void **)&pcap_hdr);
		if (!data_addr)
			break;

		idx = pcap_hdr->ifindex;

		//printk("%s: data:%x len:%d idx:%d \n", __func__, (u32)data_addr, length, idx);

		if(idx < NUM_GEMAC_SUPPORT && priv->dev[idx]) {
			struct net_device *dev = priv->dev[idx];
			if(length > 1514) {
				printk(KERN_ERR "Dropping big packet\n");
				goto pkt_drop;
			}
			skb = build_skb(data_addr, 0);

			if (unlikely(!skb)) {
				printk(KERN_ERR "Failed to allocate skb header\n");
				goto pkt_drop;
			}

			skb_reserve(skb, offset);
			skb_put(skb, length);
			skb->protocol = eth_type_trans(skb, dev);
			tstamp = ntohl(pcap_hdr->timestamp);
			skb->tstamp = ktime_set( tstamp/USEC_PER_SEC, (tstamp%USEC_PER_SEC)*1000 );

			/* Send packet to PF_PACKET socket queue */
			capture_receive_skb(skb);

			priv->stats[idx].rx_packets++;
			priv->stats[idx].rx_bytes += length;
			//dev->last_rx = jiffies;
			work_done++;
		}
		else
		{
pkt_drop:
			printk(KERN_ERR "Received with wrong dev\n");
			pfe_kfree(data_addr);
		}

	}while(work_done < budget);

	if (work_done < budget) {
		napi_complete(napi);

		hif_lib_event_handler_start(&priv->client, EVENT_RX_PKT_IND, qno);
	}
	return (work_done);

}





static int pfe_pcap_event_handler(void *data, int event, int qno)
{
	struct pcap_priv_s *priv = data;


	switch (event) {
		case EVENT_RX_PKT_IND:
			/* qno is always 0 */
			if (qno == 0) {
				if (napi_schedule_prep(&priv->low_napi)) {
					//printk(KERN_INFO "%s: schedule high prio poll\n", __func__);

					__napi_schedule(&priv->low_napi);
				}
			}

			break;

		case EVENT_TXDONE_IND:
		case EVENT_HIGH_RX_WM:
		default:
			break;
	}

	return 0;
}


int pfe_pcap_up( struct pcap_priv_s *priv)
{
	struct hif_client_s *client;
	int err = 0,rc, ii;

	char ifname[IFNAMSIZ];

	printk(KERN_INFO "%s\n", __func__);


	priv->pfe = pfe;

	for (ii = 0; ii < NUM_GEMAC_SUPPORT; ii++)
	{
		snprintf(ifname, IFNAMSIZ, "cap_gemac%d", ii);
		priv->dev[ii] = pcap_register_capdev(ifname);

		if(priv->dev[ii]==NULL) {
			printk(KERN_ERR "%s: Failed to register capture device\n",__func__);
			err = -EAGAIN;
			goto err0;
		}
	}

	if (pcap_sysfs_init(pfe->dev) < 0 )
	{
		printk(KERN_ERR "%s: Failed to register to sysfs\n",__func__);
		err = -EAGAIN;
		goto err0;
	}

	/* Register pktcap client driver to HIF */
	client = &priv->client;
	memset(client, 0, sizeof(*client));
	client->id = PFE_CL_PCAP0;
	client->tx_qn = PCAP_TXQ_CNT;
	client->rx_qn = PCAP_RXQ_CNT;
	client->priv = priv;
	client->pfe = priv->pfe;
	client->event_handler = pfe_pcap_event_handler;

	/* FIXME : For now hif lib sets all tx and rx queues to same size */
	client->tx_qsize = PCAP_TXQ_DEPTH;
	client->rx_qsize = PCAP_RXQ_DEPTH;

	printk(KERN_INFO "%s Registering client \n", __func__);
	if ((rc = hif_lib_client_register(client))) {
		printk(KERN_ERR"%s: hif_lib_client_register(%d) failed\n", __func__, client->id);
		goto err1;
	}

	printk(KERN_INFO "%s Enable PCAP in pfe \n", __func__);
	/* Enable Packet capture in PFE */
	if ((rc = pfe_ctrl_set_pcap(1))  != 0)
	{
		printk("%s: Failed to send command(enable) to pfe\n",__func__);
		err = -EAGAIN;
		goto err2;
	}

	printk(KERN_INFO "%s Enable PCAP ratelimit in pfe \n", __func__);
	/* Set the default values for the configurable parameters*/
	priv->rate_limit = COMCERTO_CAP_DFL_RATELIMIT;
	priv->pkts_per_msec = priv->rate_limit/1000;

	if ((rc = pfe_ctrl_set_pcap_ratelimit(priv->pkts_per_msec))  != 0)
	{
		printk("%s: Failed to send ratelimit command to pfe\n",__func__);
		err = -EAGAIN;
		goto err2;
	}



	return 0;

err2:
	hif_lib_client_unregister(client);
err1:
	pcap_sysfs_exit(pfe);
err0:
	for (ii = 0; ii < NUM_GEMAC_SUPPORT; ii++)
		if(priv->dev[ii])
			pcap_unregister_capdev(priv->dev[ii]);

	return err;

}

static int pfe_pcap_down(struct pcap_priv_s* priv)
{
	struct hif_client_s *client = &priv->client;
	int ii;

	printk("%s()\n", __func__);

	/* Disable Packet capture module in PFE */
	if(pfe_ctrl_set_pcap(0)!= 0)
		printk(KERN_ERR "%s: Failed while sending command CMD_PKTCAP_ENABLE\n", __func__ );
	hif_lib_client_unregister(client);
	pcap_sysfs_exit(pfe);

	for (ii = 0; ii < NUM_GEMAC_SUPPORT; ii++)
		if(priv->dev[ii])
			pcap_unregister_capdev(priv->dev[ii]);

	return 0;

}

static int pcap_driver_init(struct pfe* pfe)
{
	struct pcap_priv_s *priv = &pfe->pcap;
	int err;

	/* Initilize NAPI for Rx processing */
	init_dummy_netdev(&priv->dummy_dev);
	netif_napi_add(&priv->dummy_dev, &priv->low_napi, pfe_pcap_rx_poll, PCAP_RX_POLL_WEIGHT);
	napi_enable(&priv->low_napi);

	priv->pfe = pfe;

	err = pfe_pcap_up(priv);

	if (err < 0)
		napi_disable(&priv->low_napi);

	return err;
}

int pfe_pcap_init(struct pfe *pfe)
{
	int rc ;
	printk(KERN_INFO "%s\n",__func__);

	rc = pcap_driver_init(pfe);
	if(rc) goto err0;
	return 0;
err0:
	return rc;
}

void pfe_pcap_exit(struct pfe *pfe)
{
	struct pcap_priv_s *priv = &pfe->pcap;

	printk(KERN_INFO "%s\n", __func__);
	pfe_pcap_down(priv);

}

#else /* !CFG_PCAP */

int pfe_pcap_init(struct pfe *pfe)
{
	printk(KERN_INFO "%s\n", __func__);

	return 0;
}

void pfe_pcap_exit(struct pfe *pfe)
{
	printk(KERN_INFO "%s\n", __func__);
}

#endif /* !CFG_PCAP */

