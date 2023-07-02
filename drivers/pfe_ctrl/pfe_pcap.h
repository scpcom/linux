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

#ifndef _PFE_PCAP_H_
#define _PFE_PCAP_H_



#define  COMCERTO_CAP_RX_DESC_NT       (1024)
#define  COMCERTO_CAP_DFL_RATELIMIT     10000  //pps
#define  COMCERTO_CAP_MIN_RATELIMIT     1000 //pps
#define  COMCERTO_CAP_MAX_RATELIMIT     800000 //pps
#define  COMCERTO_CAP_DFL_BUDGET        32  //packets processed in tasklet
#define  COMCERTO_CAP_MAX_BUDGET        64
#define  COMCERTO_CAP_POLL_MS           100

int pfe_pcap_init(struct pfe *pfe);
void pfe_pcap_exit(struct pfe *pfe);

#define PCAP_RXQ_CNT	1
#define PCAP_TXQ_CNT	1

#define PCAP_RXQ_DEPTH	1024
#define PCAP_TXQ_DEPTH	1

#define PCAP_RX_POLL_WEIGHT (HIF_RX_POLL_WEIGHT - 16)


typedef struct pcap_priv_s {
	struct pfe* 		pfe;
	unsigned char		name[12];

	struct net_device_stats stats[NUM_GEMAC_SUPPORT];
	struct net_device       *dev[NUM_GEMAC_SUPPORT];
	struct hif_client_s 	client;
	u32                     rate_limit;
	u32                     pkts_per_msec;
	struct net_device       dummy_dev;
	struct napi_struct      low_napi;
}pcap_priv_t;


#endif /* _PFE_PCAP_H_ */
