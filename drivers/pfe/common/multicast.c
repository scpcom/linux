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


#include "types.h"
#include "system.h"
#include "fpp.h"
#include "multicast.h"

#ifdef COMCERTO_2000_CLASS
/** Prepend L2 headers, excluding the outermost one in case of QinQ.
 * This behavior is needed on C2k because the hardware will take care of modifying
 * the last header, as long as it only means changing the src MAC address and/or VLAN tag.
 * @param mtd
 * @param pmcl
 */
U32 mc_prepend_upper_header(PMetadata mtd, PMCListener pmcl, U16 **srcMac, real_vlan_hdr_t *vlan_tag, U8 flags)
{
	U16 ethertype = l2_get_tid(pmcl->family);
	U16 vlanid = 0;
	U16 srcMac_orig[3];
	U16 *pkt_src_mac;
	struct itf *itf = pmcl->itf;
	int res = 0;

	pkt_src_mac = (U16 *) (mtd->data + BaseOffset + ETHER_ADDR_LEN);
	COPY_MACADDR(srcMac_orig, pkt_src_mac);

	if (itf->type & IF_TYPE_PPPOE)
	{
		M_pppoe_encapsulate(mtd, (pPPPoE_Info)itf, ethertype, 1);
		ethertype = ETHERTYPE_PPPOE_END;
		itf = itf->phys;
	}

#ifdef CFG_MACVLAN
	if (itf->type & IF_TYPE_MACVLAN)
	{
		*srcMac = ((PMacvlanEntry)itf)->MACaddr;
		itf = itf->phys;
	}
#endif

	/* We can add but not remove VLANs in hardware, so:
	 * . if we must add a VLAN tag, we store its value to program the HW later (so we'll be able to handle listeners
	 * with or without VLAN tags).
	 * . if we must add 2 VLAN tags (QinQ), we add the inner one and store the value of the second to program the HW later
	 * (so we'll be able to handle listeners with one or 2 tags).
	 * => This means we cannot handle regular ethernet and QinQ listeners in hardware for the same packet.
	 */
	if (itf->type & IF_TYPE_VLAN)
	{
		vlanid = ((PVlanEntry) itf)->VlanId;

		/* QinQ */
		if (itf->phys->type & IF_TYPE_VLAN)
		{
			res = 1;
			M_vlan_encapsulate(mtd, (PVlanEntry)itf, ethertype, 1);
			ethertype = ETHERTYPE_VLAN_END;
			itf = itf->phys;
			vlanid = ((PVlanEntry) itf)->VlanId;
		}
		itf = itf->phys;
	}

	if (vlanid)
	{
		fill_real_vlan_header(vlan_tag, ETHERTYPE_VLAN_END, vlanid, mtd->vlan_pbits);
	}

	mtd->length += ETH_HEADER_SIZE;
	mtd->offset -= ETH_HEADER_SIZE;

#ifdef CFG_MACVLAN
	if (itf->type & IF_TYPE_MACVLAN)
	{
		*srcMac = ((PMacvlanEntry)itf)->MACaddr;
		itf = itf->phys;
	}
	else
#endif
	if (!*srcMac)
		*srcMac = (U16 *) ((struct physical_port *)itf)->mac_addr;


	mtd->output_port = ((struct physical_port *)itf)->id;

	if((IS_WIFI_PORT(mtd->output_port)) && ((flags & MC_MODE_MASK) == MC_MODE_ROUTED) )
		COPY_MACHEADER(mtd->data + mtd->offset, pmcl->dstmac, *srcMac, ethertype);
	else
		COPY_MACHEADER(mtd->data + mtd->offset, pmcl->dstmac, srcMac_orig, ethertype);

	return res;
}


/** Clone and send a packet.
 * Device-specific (C2000), should only be called by mc_clone_and_send.
 * @param[in] mtd				Metadata for the packet to be sent
 * @param[in] mcdest			Multicast destination entry to send the packet to
 * @param[in] first_listener	First output destination
 */
void __mc_clone_and_send(PMetadata mtd, PMCxEntry mcdest, PMCListener first_listener)
{
	U32 n = mcdest->num_listeners;
	U8 no_of_mc_uc_lis = mcdest->num_mc_uc_listeners;
#ifdef CFG_WIFI_OFFLOAD
	U8 no_of_wifi_mc_lis = mcdest->num_wifi_mc_listeners;
#else
	U8 no_of_wifi_mc_lis = 0;
#endif
	U32 packet_start = 0;
	U16 *src_mac = NULL;
	int i=0;
	int j=0;
	int first_is_vlan = 0;
	real_vlan_hdr_t vlan_tag;
	struct itf *itf;
	class_tx_hdr_mc_t *tx_hdr;
	u16 hif_hdr_len = 0;
	U32 queue_mod = DSCP_to_Qmod[mtd->priv.mcast.dscp];
	void *ddr_ptr=NULL;
	void *ddr_start=NULL;
	void *dmem_addr;
	U16 orig_mtd_offset;
	U16 orig_mtd_length;
	int mc_uc_or_wifi_mc_lisidx=0;
	U8 send_last_mc_uc_or_wifi_pkt_by_ro=FALSE;
	U8 get_first_mc_lis_idx=FALSE;
	u32 phy;
	U8 orig_hdr[ETH_MAX_HEADER_SIZE + 4];
	U8 copy_back_eth_header=FALSE;

	vlan_tag.tag = 0;

	// TODO We need enough headroom for additional descriptors, otherwise send to exception path.

	/* Assume all listeners will have the same L2 headers after first ETH/VLAN fields.
	 * This assumption is no longer required on the C1k with the use of itf structs, but still is on the C2k.
	 */
	if (mcdest->flags & MC_ACP_LISTENER)
	{
		mtd->priv.mcast.refcnt = n+1; // For C2k, priv.mcast.refcnt will be the total number of clones, i.e. one more than the C1k value.
	} else
	{
		mtd->priv.mcast.refcnt = n; // For C2k, priv.mcast.refcnt will be the total number of clones, i.e. one more than the C1k value.
	}


	/* Packet is sent by RO for
		    1. MC listeners that are not WiFi listeners or
		    2. (MC-UC listener Or WiFi MC Listener when n==1) or
		    3. (Last MC-UC listener or WiFi MC listener when no MC non-WiFi listeners are present) */

	if(no_of_mc_uc_lis + no_of_wifi_mc_lis) {

		/* Condition: IF all listeners are MC-UC listeners or WiFi MC Listeners, send last one by RO if ACP is not programmed
		   Otherwise point to first MC listener that is not WiFi listener to send by RO */
		if((no_of_mc_uc_lis + no_of_wifi_mc_lis) == n) {
			if(!(mcdest->flags & MC_ACP_LISTENER)) {
				if((no_of_mc_uc_lis + no_of_wifi_mc_lis) ==1)
				{
					goto send_by_ro;
				}
				i = no_of_mc_uc_lis + no_of_wifi_mc_lis -1;
				send_last_mc_uc_or_wifi_pkt_by_ro = TRUE;
			}
			else
				i=n;
		}
		else
			get_first_mc_lis_idx = TRUE;

		orig_mtd_offset = mtd->offset;
		orig_mtd_length = mtd->length;

		/**
 		 * For MC-UC we are constructing new ethernet header over original recived packet.
 		 * Because of this, the original ethernet header is overriden. This will cause problem
 		 * when ACP listener is enabled(ACP will receive packet with currupted ethernet header).
 		 *
 		 * So backup the original header, if packet need to send to ACP.
                 */
		if(mcdest->flags & MC_ACP_LISTENER) {
			copy_back_eth_header = TRUE;
			memcpy(orig_hdr, mtd->data + BaseOffset, ETH_MAX_HEADER_SIZE);
		}

		for(j=0; j < n; j++) {
			/* Only process MC-UC Listeners and WiFi MC Listeners*/
			if(!mcdest->listeners[j].uc_bit && !(IS_WIFI_PORT(mcdest->listeners[j].output_port))) {
				if(get_first_mc_lis_idx) {
					i = j;
					get_first_mc_lis_idx = FALSE;
				}
				continue;
			}

			__l2_prepend_header(mtd, mcdest->listeners[j].itf, mcdest->listeners[j].dstmac, l2_get_tid(mcdest->listeners[j].family), 1);

#ifdef CFG_WIFI_OFFLOAD
			if (IS_WIFI_PORT(mtd->output_port))
			{
				mtd->queue = queue_mod;
			}
			else
#endif
				mtd->queue = mcdest->listeners[j].queue_base + queue_mod;

			// Allocate BMU2 buffer sepcific to each listener
			ddr_ptr = pktbuffer_alloc();

			dmem_addr = mtd->data + mtd->offset;

			ddr_start = ddr_ptr + ddr_offset(mtd, dmem_addr);

			class_mtd_copy_to_ddr(mtd, ddr_start, mtd->length, CLASS_GP_DMEM_BUF,
						CLASS_GP_DMEM_BUF_SIZE);

			send_to_tx_direct(mtd->output_port, mtd->queue, dmem_addr, ddr_start, 0, mtd->length, 0, mtd->flags);

			/* Now let the mtd->offset & mtd->length point back to IP */
			mtd->offset = orig_mtd_offset;
			mtd->length = orig_mtd_length;
	
			mtd->priv.mcast.refcnt--;
			mc_uc_or_wifi_mc_lisidx++;

			if(send_last_mc_uc_or_wifi_pkt_by_ro && (mc_uc_or_wifi_mc_lisidx == (no_of_mc_uc_lis + no_of_wifi_mc_lis -1)))
				goto send_by_ro;
		}
	}

send_by_ro:

	/* Restart of processing for RO Block */
	if (mcdest->flags & MC_ACP_LISTENER)
	{
		/* Write ethernet header back */
		if (copy_back_eth_header)
			memcpy(mtd->data + BaseOffset, orig_hdr, ETH_MAX_HEADER_SIZE);

		orig_mtd_offset = mtd->offset;
		orig_mtd_length = mtd->length;
		mtd->length += mtd->offset - BaseOffset;
		mtd->offset = BaseOffset;
		mtd->output_port = get_expt_tx_output_port(mtd->input_port);
		if ((mtd->priv.mcast.refcnt > 1) && is_vlan(mtd) &&
				!(mcdest->listeners[i].itf->type & IF_TYPE_VLAN))
		{
			// Allocate BMU2 buffer sepcific to each listener
			ddr_ptr = pktbuffer_alloc();
			dmem_addr = mtd->data + mtd->offset;
			ddr_start = ddr_ptr + ddr_offset(mtd, dmem_addr);
			class_mtd_copy_to_ddr(mtd, ddr_start, mtd->length, CLASS_GP_DMEM_BUF,
						CLASS_GP_DMEM_BUF_SIZE);

			send_to_tx_direct(mtd->output_port, mtd->queue, dmem_addr, ddr_start, 0, mtd->length, 0, mtd->flags);
			/* Now let the mtd->offset & mtd->length point back to IP */
			mtd->offset = orig_mtd_offset;
			mtd->length = orig_mtd_length;
			mcdest->flags &= ~MC_ACP_LISTENER;
			mtd->priv.mcast.refcnt--;
		}
	}

	if (mcdest->flags & MC_ACP_LISTENER)
	{
		hif_hdr_len = hif_tx_prepare(mtd);
		packet_start = (U32)mtd->data + mtd->offset - hif_hdr_len;

		if (is_vlan(mtd))
			first_is_vlan = 1;
	}
#ifdef CFG_WIFI_OFFLOAD
	else if((i < n) && (IS_WIFI_PORT(mcdest->listeners[i].output_port)))
	{
		mtd->queue = queue_mod;
		first_is_vlan = mc_prepend_upper_header(mtd, &mcdest->listeners[i], &src_mac, &vlan_tag, mcdest->flags);
		hif_hdr_len = hif_tx_prepare(mtd);
		packet_start = (U32)mtd->data + mtd->offset - hif_hdr_len;
	}
#endif	
	else
	{
		/* Assign queue & repl_mask for first_listener */
		mtd->queue = mcdest->listeners[i].queue_base + queue_mod;
		mtd->repl_msk = mcdest->listeners[i].shaper_mask;
		first_is_vlan = mc_prepend_upper_header(mtd, &mcdest->listeners[i], &src_mac, &vlan_tag, mcdest->flags);
		packet_start = (U32)mtd->data + mtd->offset;
	}

	phy = get_tx_phy(mtd->output_port);

	// First clone
	tx_hdr = (class_tx_hdr_mc_t *)(ALIGN64(packet_start) - sizeof(class_tx_hdr_mc_t));

	tx_hdr->start_data_off = packet_start - ((U32) tx_hdr);
	/* FIXME this only works if lmem_hdr_size == ddr_hdr_size, otherwise we need to add (ddr_hdr_size - lmem_hdr_size) */
	/* (src_addr & 0xff) is the offset from the 256 byte aligned packet slot */
	tx_hdr->start_buf_off = ((U32) tx_hdr) & 0xff;
	tx_hdr->queueno = mtd->queue;
	tx_hdr->act_phyno = (0 << 4) | (phy & 0xfU);
	if ((mcdest->flags & MC_ACP_LISTENER)
#ifdef CFG_WIFI_OFFLOAD
		|| ((i < n) && (IS_WIFI_PORT(mcdest->listeners[i].output_port)))
#endif
	)
	{
		tx_hdr->pkt_length = mtd->length + hif_hdr_len;
		/* Now that we created the first clone, correct offset for the following clones that
		 * do not need the ACP metadata.
		 */
		packet_start += hif_hdr_len;
		/* If WiFi Listener is processed as first clone, point to next listener */
		if(!(mcdest->flags & MC_ACP_LISTENER))
			i++;
	}
	else
	{
		tx_hdr->pkt_length = mtd->length;
		if ((mcdest->flags & MC_MODE_MASK) == MC_MODE_ROUTED) {
			tx_hdr->act_phyno |= ACT_SRC_MAC_REPLACE;
			//src_mac already updated by mc_prepend_upper_header
			tx_hdr->src_mac_msb = src_mac[0];
			tx_hdr->src_mac_lsb = (src_mac[1] << 16) + src_mac[2];
		}
		i++;

	}

	tx_hdr->vlanid = vlan_tag.tag;
	if  (vlan_tag.tag)
	{
		tx_hdr->act_phyno |= ACT_VLAN_ADD;
	}

	/*Skip this tx_hdr if physical port tx is not enabled */
	if((mtd->output_port < GEM_PORTS) && (!IS_TX_ENABLED(mtd->output_port)) &&
			(!(mcdest->flags & MC_ACP_LISTENER))) {
		mtd->priv.mcast.refcnt--;
		PESTATUS_INCR_DROP();
		drop_counter[CLASS_DROP_TX_DISABLE]++;
		tx_hdr++;
	}


	for(;i < n;i++)
	{
		if(mcdest->listeners[i].uc_bit || (IS_WIFI_PORT(mcdest->listeners[i].output_port)))
			continue;

		tx_hdr--;
		tx_hdr->start_data_off = packet_start - (U32) tx_hdr;
		tx_hdr->start_buf_off = ((U32) tx_hdr) & 0xff;
		tx_hdr->pkt_length = mtd->length;
		tx_hdr->act_phyno = 0;

		// Shaper mask can be specified per listener
		//mtd->repl_msk = mcdest->listeners[i].shaper_mask; /* FIXME. Where is repl_msk used in PFE */

		// Now get to the bottom of the itf list to retrieve MAC, output port, and VLAN (if present)
		itf = mcdest->listeners[i].itf;
		vlan_tag.tag = 0;
		src_mac = NULL;
		while (itf->phys)
		{
#ifdef CFG_MACVLAN
			if (itf->type & IF_TYPE_MACVLAN)
				src_mac = ((PMacvlanEntry)itf)->MACaddr;
			else
#endif
			if (itf->type == IF_TYPE_VLAN)
			{
				if (vlan_tag.tag) // This means we're in QinQ mode, so only solution is to add the 2nd tag
					tx_hdr->act_phyno |= ACT_VLAN_ADD;
				fill_real_vlan_header(&vlan_tag, ETHERTYPE_VLAN_END, ((VlanEntry *)itf)->VlanId, mtd->vlan_pbits);
			}
			itf = itf->phys;
		}
		tx_hdr->vlanid = vlan_tag.tag;
		if (vlan_tag.tag)
		{
			if ((tx_hdr->act_phyno & ACT_VLAN_ADD) == 0) // We're not in QinQ mode
			{
				if (first_is_vlan)
					tx_hdr->act_phyno |= ACT_VLAN_REPLACE;
				else
					tx_hdr->act_phyno |= ACT_VLAN_ADD;
			}
		}
		phy = (((struct physical_port *)itf)->id & 0xfU);

		tx_hdr->act_phyno |= __get_tx_phy(phy);
		tx_hdr->queueno = mcdest->listeners[i].queue_base + queue_mod; // the queue number can be specified per listener
		if (!src_mac)
			src_mac = (U16 *) ((struct physical_port *)itf)->mac_addr;
		if ((mcdest->flags & MC_MODE_MASK) == MC_MODE_ROUTED) {
			tx_hdr->act_phyno |= ACT_SRC_MAC_REPLACE;
			tx_hdr->src_mac_msb = src_mac[0];
			tx_hdr->src_mac_lsb = (src_mac[1] << 16) + src_mac[2];
		}

		if (first_is_vlan && !vlan_tag.tag) {
					PRINTF("MCAST entry not supported, skipping\n");
					tx_hdr++;
					mtd->priv.mcast.refcnt--;
		}

		/*Skip this tx_hdr if physical port tx is not enabled */
		if((phy < GEM_PORTS) && (!IS_TX_ENABLED(phy)))	{
			tx_hdr++;
			mtd->priv.mcast.refcnt--;
			PESTATUS_INCR_DROP();
			drop_counter[CLASS_DROP_TX_DISABLE]++;

		}

	}

	// mtd->priv.mcast.refcnt now holds number of copies to be made by RO.
	if( mtd->priv.mcast.refcnt ) {
		send_to_tx_mcast(mtd, tx_hdr);
	}else {
		__free_packet(mtd);
	}
}

#endif /* COMCERTO_2000_CLASS */

#if defined(COMCERTO_2000_CLASS)||defined(COMCERTO_100)||defined(COMCERTO_1000)
/** Clone a packet and send copies to the relevant output interfaces.
 * The actual cloning operation is heavily device-dependent, so that function simply calls
 * __mc_clone_and_send after setting up common fields in the metadata structure.
 * @param[in] mtd		Metadata for the packet to be sent
 * @param[in] mcdest	Multicast destination entry to send the packet to
 */
void mc_clone_and_send(PMetadata mtd, PMCxEntry mcdest)
{
	PMCListener first_listener;

	mtd->priv.mcast.flags = 0;
	first_listener = &mcdest->listeners[0];
	if ((mcdest->flags & MC_MODE_BRIDGED) != 0)
		mtd->priv.mcast.flags |= MC_BRIDGED;

	__mc_clone_and_send(mtd, mcdest, first_listener);
}
#endif

#ifdef COMCERTO_1000
extern	unsigned char Image$$aram_mdma0$$Length[];
extern	unsigned char Image$$aram_mdma0$$ZI$$Length[];
extern	unsigned char Image$$aram_mdma0$$Base[];

#define MDMA0_DEV_MEMSIZE ((U32)Image$$aram_mdma0$$Length + (U32)Image$$aram_mdma0$$ZI$$Length)
#define MDMA0_DEV_MEMADDR Image$$aram_mdma0$$Base

int multicast_init(void)
{
	static BOOL mc_init_done = FALSE;
	U32 tmp;
	mdma_lane *pcntx;
	MMRXdesc tmp_dsc,*prxdsc;

	if (mc_init_done)
		return 0;

	if (( MDMA0_TX_NUMDESCR * sizeof(MMTXdesc) + MDMA0_RX_NUMDESCR *sizeof(MMRXdesc)) >  MDMA0_DEV_MEMSIZE )
		return 1; // Not enough memory should probably hang a chip here.

	// Fill control structure in software.
	pcntx= &mdma_lane0_cntx;
	memset(pcntx,0,sizeof(mdma_lane0_cntx));
	tmp = (U32)  MDMA0_DEV_MEMADDR;
	pcntx->tx_cur = pcntx->tx_top =  (void*) tmp;
	pcntx->tx_bot = pcntx->tx_top + (MDMA0_TX_NUMDESCR - 1);
	pcntx->free_tx = MDMA0_TX_NUMDESCR;
	memset( pcntx->tx_top, 0,  MDMA0_TX_NUMDESCR * sizeof(MMTXdesc));

	tmp += MDMA0_TX_NUMDESCR * sizeof(MMTXdesc);

	pcntx->rx_cur = pcntx->rx_top =  (void*) tmp;
	pcntx->rx_bot = pcntx->rx_top + (MDMA0_RX_NUMDESCR - 1);
	pcntx->pending_rx = 0;
	prxdsc=pcntx->rx_cur;

	// Initialise rx rescriptors
	memset(&tmp_dsc,0,sizeof(tmp_dsc));
	tmp_dsc.rxctl = ( MMRX_BUF_ALLOC | ((BaseOffset + ETH_HDR_SIZE)<< MMRX_OFFSET_SHIFT));
	do {
		SFL_memcpy(prxdsc, &tmp_dsc, sizeof(*prxdsc));
		prxdsc +=1;
	} while(prxdsc != pcntx->rx_bot);
	tmp_dsc.rxctl |= 	MMRX_WRAP;
	SFL_memcpy(pcntx->rx_bot, &tmp_dsc,sizeof(*prxdsc));

	// hw bindings
	pcntx->txcnt = (V32*)  MDMA_TX0_PATH_CTR;
	pcntx->rxcnt = (V32*)  MDMA_RX0_PATH_CTR;
	// Enable mdma block and lane0
	*(V32*) 	MDMA_GLB_CTRL_CFG = 1;
	*(V32*)	MDMA_TX0_PATH_HEAD = (U32) pcntx->tx_top;
	*(V32*)	MDMA_RX0_PATH_HEAD = (U32) pcntx->rx_top;
	*(V32*)	MDMA_INT_MASK_REG &= ~0x33UL; // turn off interrupts from lane0
	//TODO mat-20081017 - is burst length needed?
	// default is 0x10
	//      *(V32*)	MDMA_AHB_CFG  = 	(*(V32*)MDMA_AHB_CFG & ~0x1f) | TBD 
	// I am purposly not touching lane1
	// It is safe as long work is not queued to it.

	mc_init_done = TRUE;
	return 0;
}

#endif /* COMCERTO_1000 */
