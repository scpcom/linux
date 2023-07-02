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
#ifndef _UTIL_DMEM_STORAGE_H_
#define _UTIL_DMEM_STORAGE_H_


#include "module_rtp_relay.h"
#include "voicebuf.h"

#define MEMCPY_TMP_BUF_SIZE		128
struct util_buffers_t {
	U8 transmit_buffer[128] __attribute__((aligned(8)));

	union {
		struct {
			RTPflow rtpflow __attribute__((aligned(32))); /* sized to handle up to one flow entry */
			RTPCall rtpcall __attribute__((aligned(32)));/* sized to handle up to two special packet buffers */
			U8 UTIL_SOCKET_BUF[256] __attribute__((aligned(32))); /* sized to handle up to one IPv6 socket entry */
		} rtp_relay;

		struct {
			U8 UTIL_VOICEBUF[VOICEBUF_PAYLOAD_LEN] __attribute__((aligned(8))); /* sized to handle up to one frame */
			struct voice_buffer_reduced buffer_reduced __attribute__((aligned(8)));
			struct voice_source source __attribute__((aligned(8)));
		} voicebuf;

		struct {
			U8 memcpy_tmp_buf[MEMCPY_TMP_BUF_SIZE] __attribute__((aligned(8)));
			U8 frag_mtd_cache[48] __attribute__((aligned(8))); /* Should be atleast sizeof(struct _tFragIP4) */
		} fragbuf;
	};
};

extern struct util_buffers_t util_buffers;

#define RTPFLOW_BUFFER (&util_buffers.rtp_relay.rtpflow)
#define RTPCALL_BUFFER (&util_buffers.rtp_relay.rtpcall)
#define SOCKET_BUFFER() ((PVOID)util_buffers.rtp_relay.UTIL_SOCKET_BUF)
#define RTPQOS_BUFFER (&util_buffers.rtp_relay.UTIL_SOCKET_BUF)

#define VOICEBUF_PAYLOAD (util_buffers.voicebuf.UTIL_VOICEBUF)
#define VOICEBUF_BUF_ENTRY (&util_buffers.voicebuf.buffer_reduced)
#define VOICEBUF_SOURCE_ENTRY (&util_buffers.voicebuf.source)

#define MEMCPY_TMP_BUF		(&util_buffers.fragbuf.memcpy_tmp_buf[0])
#define FRAG_MTD_CACHE		((void *)&util_buffers.fragbuf.frag_mtd_cache)

#endif /* _UTIL_DMEM_STORAGE_H_ */
