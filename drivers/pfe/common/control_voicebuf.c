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
#include "voicebuf.h"
#include "module_ipv4.h"
#include "module_ipv6.h"
#include "module_rtp_relay.h"
#include "checksum.h"
#include "module_socket.h"


static int voice_buffer_load_check(struct voice_buffer_load *load)
{
	if (load->buffer_id >= VOICEBUF_MAX)
		return ERR_VOICE_BUFFER_UNKNOWN;

	if ((load->payload_type != RTP_PT_G711A) && (load->payload_type != RTP_PT_G711U))
		return ERR_VOICE_BUFFER_PT;

	if (load->frame_size != VOICEBUF_PAYLOAD_LEN)
		return ERR_VOICE_BUFFER_FRAME_SIZE;

	if ((load->data_len / load->frame_size) * load->frame_size != load->data_len)
		return ERR_VOICE_BUFFER_FRAME_SIZE;

	if (load->entries > VOICEBUF_SCATTER_MAX)
		return ERR_VOICE_BUFFER_ENTRIES;

	return 0;
}


#if defined(COMCERTO_2000)
#include "control_common.h"
struct dlist_head voice_source_removal_list;
struct dlist_head voice_source_list[VOICEBUF_MAX];

struct voice_buffer *virt_voicebuf;

static inline struct voice_buffer *get_voicebuf(U16 buffer_id)
{
	return virt_voicebuf+buffer_id;
}

static inline void voice_buffer_load(U16 buffer_id)
{
	voice_buffers_loaded |= (1 << buffer_id);
	pe_dmem_write(UTIL_ID, voice_buffers_loaded, virt_to_util_dmem(&voice_buffers_loaded), 1);
}

static inline void voice_buffer_unload(U16 buffer_id)
{
	voice_buffers_loaded &= ~(1 << buffer_id);
	pe_dmem_write(UTIL_ID, voice_buffers_loaded, virt_to_util_dmem(&voice_buffers_loaded), 1);
}

static inline void voice_buffer_update(struct voice_buffer *buf)
{
	int i;

	buf->dma_addr = cpu_to_be32(DDR_VIRT_TO_PHYS((void *)buf));
	buf->data_len = cpu_to_be32(buf->data_len);
	buf->frame_size = cpu_to_be32(buf->frame_size);
	buf->payload_type = cpu_to_be32(buf->payload_type);

	for (i = 0; i < buf->sc_entries; i++)
	{
		buf->sc[i].len = cpu_to_be32(buf->sc[i].len);
		buf->sc[i].data = (void *)cpu_to_be32((U32)buf->sc[i].data);
	}
	buf->sc_entries = cpu_to_be32(buf->sc_entries);
}

static void voice_source_schedule_remove(struct voice_source *source)
{
	source->removal_time = jiffies + 2;
	dlist_add(&voice_source_removal_list, &source->list);
}

/** Processes voice source delayed removal list.
* Free all voice sources in the removal list that have reached their removal time.
*
*/
void voice_source_delayed_remove(void)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct dlist_head *entry;
	struct voice_source *source;

	dlist_for_each_safe(source, entry, &voice_source_removal_list, list)
	{
		if (!time_after(jiffies, source->removal_time))
			continue;

		dlist_remove(&source->list);

		dma_pool_free(ctrl->dma_pool, source, be32_to_cpu(source->dma_addr));
	}
}

#else
static inline struct voice_buffer *get_voicebuf(U16 buffer_id)
{
	return &voicebuf[buffer_id];
}

static inline void voice_buffer_load(U16 buffer_id)
{
	voice_buffers_loaded |= (1 << buffer_id);
}

static inline void voice_buffer_unload(U16 buffer_id)
{
	voice_buffers_loaded &= ~(1 << buffer_id);
}

static inline void voice_buffer_update(struct voice_buffer *buf)
{
}
#endif


int voice_buffer_command_load(U16 *cmd, U16 len)
{
	struct voice_buffer_load *load = (struct voice_buffer_load *) cmd;
	struct voice_buffer *buf;
	int i, rc;

	rc = voice_buffer_load_check(load);
	if (rc)
		return rc;

	buf = get_voicebuf(load->buffer_id);

	if (voicebuffer_is_loaded(load->buffer_id))
		return ERR_VOICE_BUFFER_USED;

	memset(buf, 0, sizeof(struct voice_buffer));

	for (i = 0; i < load->entries; i++)
	{
		buf->sc[i].data = (void *)load->addr[i];
		buf->sc[i].len = (4 * 1024) * (1 << load->page_order[i]);

		L1_dc_invalidate(buf->sc[i].data, buf->sc[i].data + buf->sc[i].len - 1);

		buf->sc_entries++;

		if (load->data_len > buf->sc[i].len) {
			load->data_len -= buf->sc[i].len;
		}
		else
		{
			buf->sc[i].len = load->data_len;
			load->data_len = 0;
			break;
		}
	}

	if (load->data_len > 0)
		return ERR_VOICE_BUFFER_SIZE;

	buf->frame_size = load->frame_size;
	buf->payload_type = load->payload_type;
	buf->data_len = load->data_len;

	voice_buffer_update(buf);
	voice_buffer_load(load->buffer_id);

	return NO_ERR;
}


int voice_buffer_command_unload(U16 *cmd, U16 len)
{
	struct voice_buffer_unload *unload = (struct voice_buffer_unload *) cmd;
	struct voice_buffer *buf;

	if (unload->buffer_id >= VOICEBUF_MAX)
		return ERR_VOICE_BUFFER_UNKNOWN;

	buf = get_voicebuf(unload->buffer_id);

	if (!(voicebuffer_is_loaded(unload->buffer_id)))
		return ERR_VOICE_BUFFER_UNKNOWN;

	if (buf->source)
		return ERR_VOICE_BUFFER_STARTED;

	voice_buffer_unload(unload->buffer_id);

	return NO_ERR;
}


#if defined(COMCERTO_2000)
int voice_buffer_command_start(U16 *cmd, U16 len)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct voice_buffer_start *start = (struct voice_buffer_start *) cmd;
	PSockEntry socket;
	struct voice_source *source;
	struct dlist_head *entry;
	struct voice_buffer *buf;
	dma_addr_t dma_addr;

	if (start->buffer_id >= VOICEBUF_MAX)
		return ERR_VOICE_BUFFER_UNKNOWN;

	if (!(voicebuffer_is_loaded(start->buffer_id)))
		return ERR_VOICE_BUFFER_UNKNOWN;

	socket = SOCKET_find_entry_by_id(start->socket_id);
	if (!socket)
		return ERR_SOCKID_UNKNOWN;

	dlist_for_each(source, entry, &voice_source_list[start->buffer_id], list)
	{
		if (source->socket_id == cpu_to_be16(start->socket_id))
			return ERR_SOCK_ALREADY_IN_USE;
	}

	if (socket->SocketType != SOCKET_TYPE_ACP)
		return ERR_WRONG_SOCK_TYPE;

	if (socket->proto != IPPROTOCOL_UDP)
		return ERR_WRONG_SOCK_PROTO;

	/* Allocate hardware entry */
	source = dma_pool_alloc(ctrl->dma_pool, GFP_KERNEL, &dma_addr);
	if (!source)
		return ERR_NOT_ENOUGH_MEMORY;

	buf = get_voicebuf(start->buffer_id);

	memset(source, 0, sizeof(struct voice_source));

	source->dma_addr = cpu_to_be32(dma_addr);

	//hw_entry_set_field(&source->buf, cpu_to_be32(DDR_VIRT_TO_PHYS((void *)buf)));
	hw_entry_set_field(&source->next, hw_entry_get_field(&buf->source));

	dlist_add(&voice_source_list[start->buffer_id], &source->list);

	source->socket_id = cpu_to_be16(start->socket_id);
	source->seq = cpu_to_be16(start->seq_number_base);
	source->ssrc = cpu_to_be32(start->ssrc);
	source->timestamp = cpu_to_be32(start->timestamp_base);

	hw_entry_set_field(&buf->source, source->dma_addr);

	return NO_ERR;
}

void voice_source_remove(struct voice_source *source, U16 buffer_id)
{
	struct voice_buffer *buf;
	struct voice_source *prev;

	buf = get_voicebuf(buffer_id);

	if (&source->list == dlist_first(&voice_source_list[buffer_id]))
		hw_entry_set_field(&buf->source, hw_entry_get_field(&source->next));
	else
	{
		prev = container_of(source->list.prev, typeof(struct voice_source), list);
		hw_entry_set_field(&prev->next, hw_entry_get_field(&source->next));
	}

	dlist_remove(&source->list);
	voice_source_schedule_remove(source);
}

int voice_buffer_command_stop(U16 *cmd, U16 len)
{
	struct voice_buffer_stop *stop = (struct voice_buffer_stop *) cmd;
	struct voice_source *source;
	struct dlist_head *entry;
	int i;

	for (i = 0; i < VOICEBUF_MAX; i++)
	{
		if (!(voicebuffer_is_loaded(i)))
			continue;

		dlist_for_each_safe(source, entry, &voice_source_list[i], list)
		{
			if (source->socket_id == cpu_to_be16(stop->socket_id))
				goto found;
		}
	}

	return ERR_SOCKID_UNKNOWN;

found:
	voice_source_remove(source, i);

	return NO_ERR;
}

int voice_buffer_command_reset(U16 *cmd, U16 len)
{
	struct voice_source *source;
	struct dlist_head *entry;
	int i;

	for (i = 0; i < VOICEBUF_MAX; i++)
	{
		if (!(voicebuffer_is_loaded(i)))
			continue;

		dlist_for_each_safe(source, entry, &voice_source_list[i], list)
		{
			voice_source_remove(source, i);
		}

		voice_buffer_unload(i);
	}

	return NO_ERR;
}

void voice_buffer_init(void)
{
	int i;
	virt_voicebuf = (struct voice_buffer *) virt_to_util_virt(voicebuf);

	voice_buffers_loaded = 0;
	pe_dmem_write(UTIL_ID, voice_buffers_loaded, virt_to_util_dmem(&voice_buffers_loaded), 1);

	dlist_head_init(&voice_source_removal_list);
	for (i = 0; i < VOICEBUF_MAX; i++)
		dlist_head_init(&voice_source_list[i]);

	timer_init(&voice_source_delayed_remove_timer, voice_source_delayed_remove);
	timer_add(&voice_source_delayed_remove_timer, CT_TIMER_INTERVAL);
}

void voice_buffer_exit(void)
{
	struct pfe_ctrl *ctrl = &pfe->ctrl;
	struct dlist_head *entry;
	struct voice_source *source;

	timer_del(&voice_source_delayed_remove_timer);

	voice_buffer_command_reset(NULL, 0);

	dlist_for_each_safe(source, entry, &voice_source_removal_list, list)
		{
			dlist_remove(&source->list);
			dma_pool_free(ctrl->dma_pool, source, be32_to_cpu(source->dma_addr));
		}
}

#else
int voice_buffer_command_start(U16 *cmd, U16 len)
{
	struct voice_buffer_start *start = (struct voice_buffer_start *) cmd;
	PSockEntry socket;
	struct voice_source *source;
	struct voice_buffer *buf;

	if (start->buffer_id >= VOICEBUF_MAX)
		return ERR_VOICE_BUFFER_UNKNOWN;

	buf = get_voicebuf(start->buffer_id);

	if (!voicebuffer_is_loaded(start->buffer_id))
		return ERR_VOICE_BUFFER_UNKNOWN;

	socket = SOCKET_find_entry_by_id(start->socket_id);
	if (!socket)
		return ERR_SOCKID_UNKNOWN;

	source = buf->source;
	while (source)
	{
		if (source->socket_id == start->socket_id)
			return ERR_SOCK_ALREADY_IN_USE;

		source = source->next;
	}

	if (socket->SocketType != SOCKET_TYPE_ACP)
		return ERR_WRONG_SOCK_TYPE;

	if (socket->proto != IPPROTOCOL_UDP)
		return ERR_WRONG_SOCK_PROTO;

	source = Heap_Alloc(sizeof(struct voice_source));
	if (!source)
		return ERR_NOT_ENOUGH_MEMORY;

	memset(source, 0, sizeof(struct voice_source));

	source->buf = buf;

	source->next = buf->source;
	buf->source = source;

	source->socket_id = start->socket_id;
	source->ssrc = start->ssrc;
	source->timestamp = start->timestamp_base;
	source->seq = start->seq_number_base;

	if (!voice_sources)
		timer_add(&voicebuf_timer, (20 * TIMER_TICKS_PER_SEC) / 1000);

	voice_sources++;

	return NO_ERR;
}


int voice_buffer_command_stop(U16 *cmd, U16 len)
{
	struct voice_buffer_stop *stop = (struct voice_buffer_stop *) cmd;
	struct voice_source *source, *prev;
	int i;

	for (i = 0; i < VOICEBUF_MAX; i++)
	{
		if (!voicebufffer_is_loaded(i))
			continue;

		prev = NULL;
		source = get_voicebuf(i).source;
		while (source)
		{
			if (source->socket_id == stop->socket_id)
			{
				if (prev)
					prev->next = source->next;
				else
					voicebuf[i].source = source->next;

				goto found;
			}

			prev = source;
			source = source->next;
		}
	}

	return ERR_SOCKID_UNKNOWN;

found:
	voice_sources--;

	Heap_Free(source);

	if (!voice_sources)
		timer_del(&voicebuf_timer);

	return NO_ERR;
}

int voice_buffer_command_reset(U16 *cmd, U16 len)
{
	struct voice_buffer *buf;
	struct voice_source *next;
	int i;

	if (voice_sources)
	{
		timer_del(&voicebuf_timer);
		voice_sources = 0;
	}

	for (i = 0; i < VOICEBUF_MAX; i++)
	{
		buf = get_voicebuf(i);

		if (!voicebuffer_is_loaded(i))
			continue;

		while (buf->source)
		{
			next = buf->source->next;
			Heap_Free(buf->source);

			buf->source = next;
		}

		buf->is_loaded = 0;
	}

	return NO_ERR;
}

void voice_buffer_init(void)
{
	timer_init(&voicebuf_timer, voicebuf_timer_handler);
}
#endif
