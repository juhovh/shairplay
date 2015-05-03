/**
 *  Copyright (C) 2011-2012  Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "raop_buffer.h"
#include "raop_rtp.h"
#include "utils.h"
#include "bio.h"

#define RAOP_BUFFER_LENGTH 32
#define RAOP_BUFFER_MAX_RESENDS 3

typedef enum {
	RAOP_STATE_UNAVAILABLE,
	RAOP_STATE_WAITING_RESEND,
	RAOP_STATE_AVAILABLE
} raop_state_t;

typedef struct {
	/* Packet state */
	raop_state_t state;
	int resend_count;

	/* RTP header */
	unsigned char flags;
	unsigned char type;
	unsigned short seqnum;
	unsigned int timestamp;
	unsigned int ssrc;

	/* Audio buffer of valid length */
	int audio_buffer_size;
	int audio_buffer_len;
	void *audio_buffer;
} raop_buffer_entry_t;

struct raop_buffer_s {
	logger_t *logger;
	raop_decoder_t *decoder;

	/* First and last seqnum */
	int is_empty;
	unsigned short first_seqnum;
	unsigned short last_seqnum;

	/* RTP buffer entries */
	raop_buffer_entry_t entries[RAOP_BUFFER_LENGTH];

	/* Buffer of all audio buffers */
	int buffer_size;
	void *buffer;
};

raop_buffer_t *
raop_buffer_init(logger_t *logger, raop_decoder_t *raop_decoder)
{
	raop_buffer_t *raop_buffer;
	int audio_buffer_size;
	int i;

	assert(raop_decoder);

	raop_buffer = calloc(1, sizeof(raop_buffer_t));
	if (!raop_buffer) {
		return NULL;
	}

	raop_buffer->logger = logger;
	raop_buffer->decoder = raop_decoder;

	/* Allocate the output audio buffers */
	audio_buffer_size = raop_decoder_get_channels(raop_decoder) *
	                    raop_decoder_get_frame_length(raop_decoder) *
	                    raop_decoder_get_bit_depth(raop_decoder) / 8;
	raop_buffer->buffer_size = audio_buffer_size *
	                           RAOP_BUFFER_LENGTH;
	raop_buffer->buffer = malloc(raop_buffer->buffer_size);
	if (!raop_buffer->buffer) {
		free(raop_buffer);
		return NULL;
	}
	for (i=0; i<RAOP_BUFFER_LENGTH; i++) {
		raop_buffer_entry_t *entry = &raop_buffer->entries[i];
		entry->audio_buffer_size = audio_buffer_size;
		entry->audio_buffer_len = 0;
		entry->audio_buffer = (char *)raop_buffer->buffer+i*audio_buffer_size;
	}

	/* Mark buffer as empty */
	raop_buffer->is_empty = 1;
	return raop_buffer;
}

void
raop_buffer_destroy(raop_buffer_t *raop_buffer)
{
	if (raop_buffer) {
		free(raop_buffer->buffer);
		free(raop_buffer);
	}
}

static short
seqnum_cmp(unsigned short s1, unsigned short s2)
{
	return (s1 - s2);
}

int
raop_buffer_queue(raop_buffer_t *raop_buffer, unsigned char *data, unsigned short datalen, int use_seqnum)
{
	unsigned char packetbuf[RAOP_PACKET_LEN];
	unsigned short seqnum;
	raop_buffer_entry_t *entry;
	int outputlen;

	assert(raop_buffer);

	/* Check packet data length is valid */
	if (datalen < 12 || datalen > RAOP_PACKET_LEN) {
		return -1;
	}

	/* Get correct seqnum for the packet */
	if (use_seqnum) {
		seqnum = bio_get_be_u16(&data[2]);
	} else {
		seqnum = raop_buffer->first_seqnum;
	}

	/* If this packet is too late, just skip it */
	if (!raop_buffer->is_empty && seqnum_cmp(seqnum, raop_buffer->first_seqnum) < 0) {
		return 0;
	}

	/* Check that there is always space in the buffer, otherwise flush */
	if (seqnum_cmp(seqnum, raop_buffer->first_seqnum+RAOP_BUFFER_LENGTH) >= 0) {
		raop_buffer_flush(raop_buffer, seqnum);
	}

	/* Get entry corresponding our seqnum */
	entry = &raop_buffer->entries[seqnum % RAOP_BUFFER_LENGTH];
	if (entry->state == RAOP_STATE_AVAILABLE && seqnum_cmp(entry->seqnum, seqnum) == 0) {
		/* Packet sent twice, we can safely ignore it */
		return 0;
	}

	/* Update the raop_buffer entry header */
	entry->flags = data[0];
	entry->type = data[1];
	entry->seqnum = seqnum;
	entry->timestamp = bio_get_be_u32(&data[4]);
	entry->ssrc = bio_get_be_u32(&data[8]);
	entry->state = RAOP_STATE_AVAILABLE;
	entry->resend_count = 0;

	/* Decrypt and decode audio data */
	entry->audio_buffer_len = entry->audio_buffer_size;
	raop_decoder_decode(raop_buffer->decoder, &data[12], datalen - 12,
	                    entry->audio_buffer, &entry->audio_buffer_len);

	/* Update the raop_buffer seqnums */
	if (raop_buffer->is_empty) {
		raop_buffer->first_seqnum = seqnum;
		raop_buffer->last_seqnum = seqnum;
		raop_buffer->is_empty = 0;
	}
	if (seqnum_cmp(seqnum, raop_buffer->last_seqnum) > 0) {
		raop_buffer->last_seqnum = seqnum;
	}
	return 1;
}

const void *
raop_buffer_dequeue(raop_buffer_t *raop_buffer, int *length, unsigned int *timestamp, int no_resend)
{
	raop_buffer_entry_t *entry;
	short buflen;

	/* Calculate number of entries in the current buffer */
	buflen = seqnum_cmp(raop_buffer->last_seqnum, raop_buffer->first_seqnum)+1;

	/* Cannot dequeue from empty buffer */
	if (raop_buffer->is_empty || buflen <= 0) {
		return NULL;
	}

	/* Get the first buffer entry for inspection */
	entry = &raop_buffer->entries[raop_buffer->first_seqnum % RAOP_BUFFER_LENGTH];
	if (no_resend) {
		/* If we do no resends, always return the first entry */
	} else if (entry->state != RAOP_STATE_AVAILABLE) {
		/* Check how much we have space left in the buffer */
		if (buflen < RAOP_BUFFER_LENGTH) {
			/* Return nothing and hope resend gets on time */
			return NULL;
		}
		/* Risk of buffer overrun, return empty buffer */
	}

	/* Update buffer and validate entry */
	raop_buffer->first_seqnum += 1;
	if (entry->state != RAOP_STATE_AVAILABLE) {
		/* Return an empty audio buffer to skip audio */
		logger_log(raop_buffer->logger, LOGGER_WARNING,
		           "Receive buffer overrun for seqnum %hu buflen %hd,"
		           " returning empty audio", raop_buffer->first_seqnum-1,
		           buflen);
		*length = entry->audio_buffer_size;
		*timestamp = entry->timestamp;
		memset(entry->audio_buffer, 0, *length);
		return entry->audio_buffer;
	}
	entry->state = RAOP_STATE_UNAVAILABLE;

	/* Return entry audio buffer */
	*length = entry->audio_buffer_len;
	*timestamp = entry->timestamp;
	entry->audio_buffer_len = 0;
	return entry->audio_buffer;
}

void
raop_buffer_handle_resends(raop_buffer_t *raop_buffer, raop_resend_cb_t resend_cb, void *opaque)
{
	raop_buffer_entry_t *entry;
	int first_seqnum;
	int seqnum;

	assert(raop_buffer);
	assert(resend_cb);

	/* First find the first unavailable packet seqnum */
	for (seqnum=raop_buffer->first_seqnum; seqnum_cmp(seqnum, raop_buffer->last_seqnum)<0; seqnum++) {
		entry = &raop_buffer->entries[seqnum % RAOP_BUFFER_LENGTH];
		if (entry->state == RAOP_STATE_UNAVAILABLE) {
			break;
		}
	}
	first_seqnum = seqnum;

	if (seqnum_cmp(first_seqnum, raop_buffer->last_seqnum) < 0) {
		int count;

		for (seqnum=first_seqnum; seqnum_cmp(seqnum, raop_buffer->last_seqnum)<0; seqnum++) {
			entry = &raop_buffer->entries[seqnum % RAOP_BUFFER_LENGTH];
			if (entry->state != RAOP_STATE_UNAVAILABLE) {
				break;
			}
			if (entry->resend_count >= RAOP_BUFFER_MAX_RESENDS) {
				logger_log(raop_buffer->logger, LOGGER_WARNING,
					   "Resend count for %u exceeded, waiting for resend", seqnum);
				entry->state = RAOP_STATE_WAITING_RESEND;
				break;
			}
			entry->resend_count++;
		}
		if (seqnum_cmp(seqnum, first_seqnum) == 0) {
			return;
		}
		count = seqnum_cmp(seqnum, first_seqnum);
		resend_cb(opaque, first_seqnum, count);
	}
}

void
raop_buffer_flush(raop_buffer_t *raop_buffer, int next_seq)
{
	int i;

	assert(raop_buffer);

	for (i=0; i<RAOP_BUFFER_LENGTH; i++) {
		raop_buffer->entries[i].state = RAOP_STATE_UNAVAILABLE;
		raop_buffer->entries[i].audio_buffer_len = 0;
	}
	if (next_seq < 0 || next_seq > 0xffff) {
		raop_buffer->is_empty = 1;
	} else {
		raop_buffer->first_seqnum = next_seq;
		raop_buffer->last_seqnum = next_seq-1;
	}
}
