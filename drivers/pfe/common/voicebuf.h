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
#ifndef _VOICEBUF_H_
#define _VOICEBUF_H_

#include "types.h"
#include "module_timer.h"
#include "fe.h"


#define VOICEBUF_MAX		8
#define VOICEBUF_SCATTER_MAX	48
#define VOICEBUF_PAYLOAD_LEN	160


#if !defined(COMCERTO_2000) || defined(COMCERTO_2000_CONTROL)
struct voice_buffer_load
{
	U16 buffer_id;
	U16 payload_type;
	U16 frame_size;
	U16 entries;
	U32 data_len;
	U8 page_order[VOICEBUF_SCATTER_MAX];
	U32 addr[VOICEBUF_SCATTER_MAX];
};

struct voice_buffer_unload
{
	U16 buffer_id;
};

struct voice_buffer_start
{
	U16 socket_id;
	U16 buffer_id;
	U16 seq_number_base;
	U16 padding;
	U32 ssrc;
	U32 timestamp_base;
};

struct voice_buffer_stop
{
	U16 socket_id;
};

void voice_buffer_init(void);
void voice_buffer_exit(void);
int voice_buffer_command_load(U16 *cmd, U16 len);
int voice_buffer_command_unload(U16 *cmd, U16 len);
int voice_buffer_command_start(U16 *cmd, U16 len);
int voice_buffer_command_stop(U16 *cmd, U16 len);
int voice_buffer_command_reset(U16 *cmd, U16 len);
#endif
void voice_buffer_init(void);


struct scatter_entry
{
	char *data;
	unsigned int len;
}__attribute__((aligned(8))); // So we can use efet_memcpy64

#if defined(COMCERTO_2000)
struct scatter_info {
	int offset;
	int n;
};

#if defined(COMCERTO_2000_CONTROL)
struct voice_buffer {
	U32 dma_addr;
	int data_len;	//FIXME is that field needed at all?
	int frame_size;
	int payload_type;
	U32 source;

	int sc_entries;
	struct scatter_entry sc[VOICEBUF_SCATTER_MAX];
}__attribute__((aligned(8))); // So we can use efet_memcpy64

struct voice_source
{
	struct scatter_info sci __attribute__((aligned(8)));

	U32 next;

	U16 seq __attribute__((aligned(8)));
	U16 socket_id;
	U32 timestamp;
	U32 ssrc;

	U32 dma_addr;
	/* These fields are only used by host software, so keep them at the end of the structure */
	struct dlist_head 	list;
	unsigned long		removal_time;
}__attribute__((aligned(8))); // So we can use efet_memcpy64

extern TIMER_ENTRY voice_source_delayed_remove_timer;
void voice_source_delayed_remove(void);

#else
struct voice_buffer {
	struct voice_buffer *dma_addr;
	int data_len;	//FIXME is that field needed at all?
	int frame_size;
	int payload_type;
	struct voice_source *source;

	int sc_entries;
	struct scatter_entry sc[VOICEBUF_SCATTER_MAX];
}__attribute__((aligned(8)));


struct voice_buffer_reduced {
	struct voice_buffer *dma_addr;
	int data_len;	//FIXME is that field needed at all?
	int frame_size;
	int payload_type;
	struct voice_source *source;

	int sc_entries;
}__attribute__((aligned(8))); // So we can use efet_memcpy64

struct voice_source
{
	struct scatter_info sci __attribute__((aligned(8)));

	struct voice_source *next;

	U16 seq __attribute__((aligned(8)));
	U16 socket_id;
	U32 timestamp;
	U32 ssrc;

	struct voice_source *dma_addr;
}__attribute__((aligned(8))); // So we can use efet_memcpy64

#endif
extern U8 voice_buffers_loaded;
extern struct voice_buffer voicebuf[VOICEBUF_MAX];

#else /* COMCERTO_1000 */
struct voice_source
{
	struct voice_source *next;

	U16 socket_id;

	int sc_offset;
	int sc_n;

	struct voice_buffer *buf;

	U32 ssrc;
	U16 seq;
	U32 timestamp;
};


static U8 voice_buffers_loaded;
static struct voice_buffer {

	int data_len; //FIXME is that field needed at all?
	int frame_size;
	int payload_type;

	struct scatter_entry sc[VOICEBUF_SCATTER_MAX];
	int sc_entries;

	struct voice_source *source;

} voicebuf[VOICEBUF_MAX];

#endif

static inline int voicebuffer_is_loaded(U16 buffer_id)
{
	return voice_buffers_loaded & (1 << buffer_id);
}

#if defined(COMCERTO_2000_UTIL)
#include "util_dmem_storage.h"
extern TIMER_ENTRY voicebuf_timer;

void voicebuf_timer_handler(void);
#endif

#endif /* _VOICEBUF_H_ */
