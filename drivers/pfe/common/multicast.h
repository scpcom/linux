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


/* $id$ */
#ifndef _MULTICAST_H_
#define	_MULTICAST_H_

#include "system.h"
#include "fpp.h"
#include "module_expt.h"
#include "module_qm.h"
#ifdef COMCERTO_1000
#include "xdma.h"
#endif
#include "channels.h"
#if defined(COMCERTO_1000)||defined(COMCERTO_2000_CLASS)
#include "layer2.h"
#endif
#include "module_ipv4.h"
#include "module_ipv6.h"
#ifdef COMCERTO_2000_CLASS
#include "module_tx.h"
#include "pfe/class.h"
#endif
#define MCx_MAX_LISTENERS_PER_GROUP		10 
#define MCx_MAX_MC_LISTENERS_PER_GROUP		4
							
#define MCx_MAX_LISTENERS_IN_QUERY		5 
#define MC_MODE_BRIDGED 0x0001
#define MC_MODE_ROUTED 0x0000
#define MC_MODE_MASK 0x0001
#define MC_ACP_LISTENER 0x2

#define MC4_MAC_CHECKER 0x0001005E // in big-endian format

#define MC4_MAC_DEST_MARKER1 ntohs(0x0100)
#define MC4_MAC_DEST_MARKER2 ntohs(0x5E00)

#define MC6_MAC_DEST_MARKER 0x3333

#if defined(COMCERTO_2000)
#define HASH_ENTRY_VALID		(1 << 0)
#define HASH_ENTRY_USED		(1 << 1)
#define HASH_ENTRY_UPDATING	(1 << 2)
#endif

#ifdef COMCERTO_100
typedef struct _tMCListener {
	//Make L2 header aligned the same way as BaseOffset
#if ((BaseOffset & 0x3) == 0)
	U8 L2_header[ETH_HDR_SIZE + VLAN_HDR_SIZE];
	U8 L2_header_size;
	U8 output_index;
#else
	U8 L2_header_size;
	U8 output_index;
	U8 L2_header[ETH_HDR_SIZE + VLAN_HDR_SIZE];
#endif
	int timer;
	U8 output_port;
  	U8 shaper_mask;
	} __attribute__((aligned(4))) MCListener, *PMCListener, *PMC4Listener, *PMC6Listener;

#else
typedef struct _tMCListener {
	struct itf *itf;
	int timer;
	U8 output_index;
	U8 output_port;
  	U8 shaper_mask;
  	U8 family;
	U8 uc_bit;
#ifdef ENDIAN_LITTLE
	U8 queue_base:5,
	   q_bit:1,
	   rsvd:2;
#else
	U8 rsvd:2,
	   q_bit:1,
	   queue_base:5;
#endif
	U8 dstmac[6];
} __attribute__((aligned(4))) MCListener, *PMCListener, *PMC4Listener, *PMC6Listener;
#endif

typedef struct _tMCxEntry {
	U8 src_mask_len;
	U8 num_listeners;
	U8 num_mc_uc_listeners; /* Number of MC UC Listeners */
#ifdef CFG_WIFI_OFFLOAD
	U8 num_wifi_mc_listeners; /* Number of WiFi MC Listeners (Doesn't include ACP) */
#endif
	U8 queue_base;
	U8 flags;
#ifdef CFG_WIFI_OFFLOAD
	U8 padding[6];
#else
	U8 padding[7];
#endif
	int wifi_listener_timer;
	MCListener listeners[MCx_MAX_LISTENERS_PER_GROUP];
}MCxEntry, *PMCxEntry;

#ifdef COMCERTO_1000
int multicast_init(void);
#endif

extern void class_mtd_copy_to_ddr(PMetadata mtd, void *ddr_start, u32 len, void *dmem_buf, unsigned int dmem_len);

#if !defined(COMCERTO_2000_CONTROL)
static __inline void mc_to_expt(PMetadata mtd)
{
#if defined(COMCERTO_2000)
	if ((mtd->flags & MTD_MC6) && IS_ICMPv6(mtd->data + mtd->offset, (U8 *)mtd->rx_dmem_end))
#else
	if ((mtd->flags & MTD_MC6) && IS_ICMPv6(mtd->data + mtd->offset, mtd->data + BaseOffset + mtd->length))
#endif
		mtd->queue = EXPT_ARP_QUEUE;
	else
		mtd->queue = DSCP_to_Q[mtd->priv.mcast.dscp];
	SEND_TO(EXPT, mtd);
}
#endif

/** Computes a multicast MAC address based on the IP (v4 or v6) header.
 * Only assumption is that variables are 16-bit aligned.
 * C2k PFE class version differs in the handling of endianness in the IPv4 case,
 * and is also a bit optimized for the PE CPU (this is the reason it doesn't use
 * READ_UNALIGNED_INT/WRITE_UNALIGNED_INT macros).
 * @param[out]	dstmac 	Pointer to destination buffer
 * @param[in]	iphdr 	IP (v4 or v6) header to be used to generate the MAC address.
 * @param[in]	family	Either PROTO_IPV4 or PROTO_IPV6, depending on IP version.
 */
#if !defined(COMCERTO_2000_CLASS)
static __inline void mc_mac_from_ip(U8 *dstmac, void *iphdr, U8 family)
{
	if (family == PROTO_IPV4)
	{
		U32 da = READ_UNALIGNED_INT(((ipv4_hdr_t *) iphdr)->DestinationAddress);
		*(U16 *)dstmac = MC4_MAC_DEST_MARKER1;
		__WRITE_UNALIGNED_INT(dstmac + 2, MC4_MAC_DEST_MARKER2 | (da & 0xFFFF7F00));
	} else
	{
		U32 da_lo = READ_UNALIGNED_INT(((ipv6_hdr_t *) iphdr)->DestinationAddress[IP6_LO_ADDR]);
		*(U16 *)dstmac = MC6_MAC_DEST_MARKER;
		__WRITE_UNALIGNED_INT(dstmac + 2, da_lo);
	}
}
#endif

#ifdef COMCERTO_100
void mc_prepend_header(PMetadata mtd, PMCListener pmcl)
{
	int l2len;
	U8 *p;

	l2len = pmcl->L2_header_size;
	mtd->offset -= l2len;
	mtd->length += l2len;
	p = mtd->data + mtd->offset;

	mtd->output_port = pmcl->output_port;

#if ((BaseOffset & 0x3) == 0)
	if (mtd->priv.mcast.flags & MC_BRIDGED)
	{
		// need to modify L2_header to reflect unmodified src mac
		*(U16*)(pmcl->L2_header + 6) = *(U16*)(mtd->data + BaseOffset + 6);
		*(U32*)(pmcl->L2_header + 8) = *(U32*)(mtd->data + BaseOffset + 8);
	}
	*(U32*)p = *(U32*)&pmcl->L2_header[0];
	*(U32*)(p + 4) = *(U32*)&pmcl->L2_header[4];
	*(U32*)(p + 8) = *(U32*)&pmcl->L2_header[8];
	if (l2len ==  ETH_HDR_SIZE) {
		*(U16*)(p + 12) = *(U16*)&pmcl->L2_header[12];
	} else {
		*(U32*)(p + 12) = *(U32*)&pmcl->L2_header[12];
		*(U16*)(p + 16) = *(U16*)&pmcl->L2_header[16];
	}
#else
	if (mtd->priv.mcast.flags & MC_BRIDGED)
	{
		// need to modify L2_header to reflect unmodified src mac
		*(U32*)(pmcl->L2_header + 6) = *(U32*)(mtd->data + BaseOffset + 6);
		*(U16*)(pmcl->L2_header + 10) = *(U16*)(mtd->data + BaseOffset + 10);
	}
	if (l2len ==  ETH_HDR_SIZE) {
		copy16(p,&pmcl->L2_header[0]);
	} else {
		copy20(p,&pmcl->L2_header[0]);
	}
#endif
}

/** Clone and send a packet.
 * Device-specific (C100), should only be called by mc_clone_and_send.
 * @param[in] mtd				Metadata for the packet to be sent
 * @param[in] mcdest			Multicast destination entry to send the packet to
 * @param[in] first_listener	First output destination
 */
void __mc_clone_and_send(PMetadata mtd, PMCxEntry mcdest, PMCListener first_listener)
{
	PMCListener this_listener;
	PMetadata mtd_new;
	U32 num_copies;
	U32 n = mcdest->num_listeners;
	int i;

	if (n == 1 && (mcdest->flags & MC_ACP_LISTENER == 0))
	{
		// here if just one listener (and no wifi)
		mc_prepend_header(mtd, (PMCListener) first_listener);
		send_to_tx(mtd);
		return;
	}

	num_copies = 1;
	// have to replicate
	// prepare mtd properties to copy to clones.
	mtd->flags |= MTD_MULTICAST_TX;

	if (mcdest->flags & MC_ACP_LISTENER)
	{
		if ((mtd_new = mtd_alloc()) != NULL) {
			*mtd_new = *mtd;
			num_copies++;
			mc_to_expt(mtd_new);
		}
	}

	for (i = 0; i < n - 1; i++)
	{
		this_listener = &mcdest->listeners[i];
		if ((mtd_new = mtd_alloc()) != NULL) {
			*mtd_new = *mtd;
			if (mcdest->flags & MC_MODE_BRIDGED) {
				// need to modify L2_header to reflect unmodified src mac
				*(U16*)(this_listener->L2_header + 6) = *(U16*)(mtd->data + BaseOffset + 6);
				*(U32*)(this_listener->L2_header + 8) = *(U32*)(mtd->data + BaseOffset + 8);
				L1_dc_clean(this_listener->L2_header,this_listener->L2_header+sizeof(this_listener->L2_header)-1);
			}
			mtd_new->priv.dword =(U64) this_listener->L2_header | (( (U64) this_listener->L2_header_size)<< 32);
			mtd_new->output_port = this_listener->output_port;
			mtd_new->repl_msk = this_listener->shaper_mask;
			send_to_tx(mtd_new);
			num_copies++;
		}
		else {
			break;
		}
	}
	// last packet left - use original mtd.
	this_listener = &mcdest->listeners[i];
	if (mcdest->flags & MC_MODE_BRIDGED) {
		// need to modify L2_header to reflect unmodified src mac
		*(U16*)(this_listener->L2_header + 6) = *(U16*)(mtd->data + BaseOffset + 6);
		*(U32*)(this_listener->L2_header + 8) = *(U32*)(mtd->data + BaseOffset + 8);
		L1_dc_clean(this_listener->L2_header,this_listener->L2_header+sizeof(this_listener->L2_header)-1);
	}
	mtd->priv.dword =(U64) this_listener->L2_header |  (( (U64) this_listener->L2_header_size)<< 32);
	mtd->output_port = this_listener->output_port;
	mtd->repl_msk = this_listener->shaper_mask;
	send_to_tx(mtd);

	// save reference count in the data buffer
	*(U32*)(mtd->data+MC_REFCOUNT_OFFSET) = num_copies;
}
#endif




#ifdef COMCERTO_1000
void mc_prepend_header(PMetadata mtd, PMCListener pmcl)
{
		RouteEntry RtEntry;

		mc_mac_from_ip(RtEntry.dstmac, mtd->data+mtd->offset, pmcl->family);
		RtEntry.itf = pmcl->itf;
		l2_prepend_header(mtd, &RtEntry, (U16) pmcl->family);
}
//
// MDMA lane descriptor

typedef struct _mlane {
	V32	*txcnt;
	V32	*rxcnt;
	MMTXdesc *tx_top;
	MMTXdesc *tx_bot;
	MMTXdesc *tx_cur;
	U32	free_tx;
	MMRXdesc *rx_top;
	MMRXdesc *rx_bot;
	MMRXdesc *rx_cur;
	U32	pending_rx;
} mdma_lane;

#if defined(SIM) 
#define MDMA0_TX_NUMDESCR 8
#else
#define MDMA0_TX_NUMDESCR 48
#endif
#define MDMA0_RX_NUMDESCR ((MCx_MAX_LISTENERS_PER_GROUP-1) *MDMA0_TX_NUMDESCR) 

extern mdma_lane mdma_lane0_cntx;

static __inline int mdma_request_copy(PMetadata mtd, int num_copies, PMCxEntry listeners)
{
	// returns number of copies requested by mdma
	int copies_requested = 0;
#if !defined(COMCERTO_2000)

	U64 tmpU64;
	if (num_copies == 0)
		return 0;
	mdma_lane *pcntx = &mdma_lane0_cntx;
	if (pcntx->free_tx)
	{
		copies_requested = MDMA0_RX_NUMDESCR - pcntx->pending_rx;
		if (copies_requested > num_copies)
			copies_requested = num_copies;
		if (copies_requested > 0)
		{
			// Some copies will be made. Need to flush d-cache in the "copy from" buffer.
			// Only ttl is modified - can do a small flush. 
			// 	7 is an offset to last byte of ipv6.HopLimit 
			// 	11 is offset into ipv4.checksum
			L1_dc_clean(mtd->data+mtd->offset,mtd->data+mtd->offset+11);
			// Cache will be invalidated in module_tx or free_packet

			tmpU64 = (((U64) mtd) << 32) | ((U32) listeners);
			Write64(tmpU64, ((U64*) pcntx->tx_cur)+1);
			tmpU64 =(U64) (mtd->data+mtd->offset);
			tmpU64 += (((U64)mtd->length<<MMTX_LENGTH_SHIFT) | ((U64)copies_requested<< MMTX_NUMCPY_SHIFT) | ((U64)mtd->offset <<  MMTX_OFFSET_SHIFT ))<< 32;
			if (pcntx->tx_cur == pcntx->tx_bot)
			{
				tmpU64 += ((U64) MMTX_WRAP ) << 32;
				Write64(tmpU64, (U64*) pcntx->tx_cur);
				pcntx->tx_cur = pcntx->tx_top;
			}
			else
			{
				Write64(tmpU64, (U64*) pcntx->tx_cur);
				pcntx->tx_cur += 1;
			}
			*pcntx->txcnt = 1;
			*pcntx->rxcnt =  copies_requested;
			pcntx->pending_rx += copies_requested;
			pcntx->free_tx -= 1;
			set_event(gEventStatusReg, (1 << EVENT_MC6));
		}
	}
#endif
	return copies_requested;
}

static __inline int multicast_poola_hwm() {
    // report max number of poolA buffers held in mdma_rx
    return MDMA0_RX_NUMDESCR;
}
/** Clone and send a packet.
 * Device-specific (C1000), should only be called by mc_clone_and_send.
 * @param[in] mtd				Metadata for the packet to be sent
 * @param[in] mcdest			Multicast destination entry to send the packet to
 * @param[in] first_listener	First output destination
 */
static __inline void __mc_clone_and_send(PMetadata mtd, PMCxEntry mcdest, PMCListener first_listener)
{
	U32 num_copies;
	U32 n = mcdest->num_listeners;

	// clone using mdma
	if (mcdest->flags & MC_ACP_LISTENER) {
		// At the end mtd will be redirected to exception path
		mtd->priv.mcast.flags |= MC_LASTMTD_WIFI;
	} else {
		// At the end mtd will be used for last lan transmit
		n -= 1;
	}
	if ((num_copies = mdma_request_copy(mtd, n, mcdest)) > 0) {
		mtd->priv.mcast.refcnt = num_copies;
		mtd->priv.mcast.rxndx = 0;
	}
	else
	{
		// here if just one listener, or mdma request failed
		mc_prepend_header(mtd, (PMCListener)first_listener);
	#ifdef CFG_WIFI_OFFLOAD
		if (IS_WIFI_PORT(first_listener->output_port))
			  // DSCP_to_Qmod could also be considered here, but since we are no longer doing full Wifi offload,
			  // the packet will go to the exception path and so DSCP_to_Q makes more sense.
			  mtd->queue = DSCP_to_Q[mtd->priv.mcast.dscp];

	#endif
		send_to_tx(mtd);
	}
}
#endif	// COMCERTO_1000


#ifdef COMCERTO_2000_CLASS
/** Fill in a VLAN header structure based on given field values.
 * @param[out] pvlanHdr		pointer to VLAN header structure to fill in.
 * @param[in] ethertype		Ethertype to use, in network endianness.
 * @param[in] VlanId		VLAN id, in network endianness.
 * @param[in] vlan_pbits	VLAN priority bits (in bits [2:0]).
 * We can't use the vlan_hdr_t structure as it (purposefully) doesn't match the standard 802.1Q format.
 */
static inline void fill_real_vlan_header(real_vlan_hdr_t *pvlanHdr, U16 ethertype, U16 VlanId, U8 vlan_pbits)
{
	pvlanHdr->TPID = ethertype;
	pvlanHdr->TCI = VlanId | htons(vlan_pbits << 13);
}

U32 mc_prepend_upper_header(PMetadata mtd, PMCListener pmcl, U16 **srcMac, real_vlan_hdr_t *vlan_tag, U8 flags);



void __mc_clone_and_send(PMetadata mtd, PMCxEntry mcdest, PMCListener first_listener);

/* TODO this is the length of the metadata added to packets towards the exception path
 * so that the HIF driver on the host can determine how to handle the packet
 * (wifi offload, MSP, IPsec, etc).
 */
#define ACP_MTD_LEN 8


#endif


#if defined(COMCERTO_2000_CLASS)||defined(COMCERTO_100)||defined(COMCERTO_1000)

void mc_clone_and_send(PMetadata mtd, PMCxEntry mcdest);

#endif




#endif	/* _MULTICAST_H_ */
