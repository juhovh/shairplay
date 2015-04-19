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

#include <stdint.h>
#include "crypto/crypto.h"
#include "alac/alac.h"
#include "aac_eld/aac_eld.h"

#define RAOP_BUFFER_LENGTH 32

typedef struct {
	/* Packet available */
	int available;

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
	/* AES key and IV */
	unsigned char aeskey[RAOP_AESKEY_LEN];
	unsigned char aesiv[RAOP_AESIV_LEN];

	/* New AES key ctx */
        AES_KEY key_ctx;

	/* ALAC decoder */
	ALACSpecificConfig alacConfig;
	alac_file *alac;
       
        /* AAC-ELD decoder */
        aac_eld_file *aac_eld;

	/* First and last seqnum */
	int is_empty;
	unsigned short first_seqnum;
	unsigned short last_seqnum;

	/* RTP buffer entries */
	raop_buffer_entry_t entries[RAOP_BUFFER_LENGTH];

	/* Buffer of all audio buffers */
	int buffer_size;
	void *buffer;

        /* encryption type*/
        int et;

        /* audio codecs*/
        int cn;
};



static int
get_fmtp_info(ALACSpecificConfig *config, const char *fmtp, int cn)
{
	if (cn == 1) {
		int intarr[12];
		char *original;
		char *strptr;
		int i;

		/* Parse fmtp string to integers */
		original = strptr = strdup(fmtp);
		for (i=0; i<12; i++) {
			if (strptr == NULL) {
				free(original);
				return -1;
			}
			intarr[i] = atoi(utils_strsep(&strptr, " "));
		}
		free(original);
		original = strptr = NULL;

		/* Fill the config struct */
		config->frameLength = intarr[1];
		config->compatibleVersion = intarr[2];
		config->bitDepth = intarr[3];
		config->pb = intarr[4];
		config->mb = intarr[5];
		config->kb = intarr[6];
		config->numChannels = intarr[7];
		config->maxRun = intarr[8];
		config->maxFrameBytes = intarr[9];
		config->avgBitRate = intarr[10];
		config->sampleRate = intarr[11];

		/* Validate supported audio types */
		if (config->bitDepth != 16) {
			return -2;
		}
		if (config->numChannels != 2) {
			return -3;
		}
	} else {
		/* Fill the config struct */
		config->frameLength = 4096;
		config->compatibleVersion = 0;
		config->bitDepth = 16;
		config->pb = 40;
		config->mb = 10;
		config->kb = 14;
		config->numChannels = 2;
		config->maxRun = 255;
		config->maxFrameBytes = 0;
		config->avgBitRate = 0;
		config->sampleRate = 44100;
	}

	return 0;
}

static void
set_decoder_info(alac_file *alac, ALACSpecificConfig *config, int cn)
{
	if (cn == 1) {
		unsigned char decoder_info[48];
		memset(decoder_info, 0, sizeof(decoder_info));

#define SET_UINT16(buf, value)do{\
	(buf)[0] = (unsigned char)((value) >> 8);\
	(buf)[1] = (unsigned char)(value);\
}while(0)

#define SET_UINT32(buf, value)do{\
	(buf)[0] = (unsigned char)((value) >> 24);\
	(buf)[1] = (unsigned char)((value) >> 16);\
	(buf)[2] = (unsigned char)((value) >> 8);\
	(buf)[3] = (unsigned char)(value);\
}while(0)

		/* Construct decoder info buffer */
		SET_UINT32(&decoder_info[24], config->frameLength);
		decoder_info[28] = config->compatibleVersion;
		decoder_info[29] = config->bitDepth;
		decoder_info[30] = config->pb;
		decoder_info[31] = config->mb;
		decoder_info[32] = config->kb;
		decoder_info[33] = config->numChannels;
		SET_UINT16(&decoder_info[34], config->maxRun);
		SET_UINT32(&decoder_info[36], config->maxFrameBytes);
		SET_UINT32(&decoder_info[40], config->avgBitRate);
		SET_UINT32(&decoder_info[44], config->sampleRate);
		alac_set_info(alac, (char *) decoder_info);
	} else {
		/* Nothing to do */
	}
}

raop_buffer_t *
raop_buffer_init(const char *rtpmap,
                 const char *fmtp,
                 const unsigned char *aeskey,
                 const unsigned char *aesiv, int et, int cn)
{
	raop_buffer_t *raop_buffer;
	int audio_buffer_size;
	ALACSpecificConfig *alacConfig;
	int i;

        assert(rtpmap);
	assert(fmtp);
	assert(aeskey);
	assert(aesiv);

	raop_buffer = calloc(1, sizeof(raop_buffer_t));
	if (!raop_buffer) {
		return NULL;
	}

	/* Parse fmtp information */
	alacConfig = &raop_buffer->alacConfig;
	if (get_fmtp_info(alacConfig, fmtp, cn) < 0) {
		free(raop_buffer);
		return NULL;
	}

	/* Allocate the output audio buffers */
	audio_buffer_size = alacConfig->frameLength *
	                    alacConfig->numChannels *
	                    alacConfig->bitDepth/8;
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

	if (cn == 1) {
		/* Initialize ALAC decoder */
		raop_buffer->alac = create_alac(alacConfig->bitDepth,
				alacConfig->numChannels);
		if (!raop_buffer->alac) {
			free(raop_buffer->buffer);
			free(raop_buffer);
			return NULL;
		}
		set_decoder_info(raop_buffer->alac, alacConfig, cn);
	} else if (cn == 3) {
		/* Initialize AAC-ELD decoder */
		raop_buffer->aac_eld = create_aac_eld();
	} else {
		/* Todo */
		return NULL;
	}

	if (et == 1) {
		/* Initialize AES keys */
		memcpy(raop_buffer->aeskey, aeskey, RAOP_AESKEY_LEN);
		memcpy(raop_buffer->aesiv, aesiv, RAOP_AESIV_LEN);
	} else if (et == 3) {
		/* Initialize AES key ctx */
		AES_set_decrypt_key(aeskey, 128, &raop_buffer->key_ctx);
		memcpy(&raop_buffer->key_ctx.in, aeskey, RAOP_AESKEY_LEN);
		memcpy(&raop_buffer->key_ctx.iv, aesiv, RAOP_AESIV_LEN);
		raop_buffer->key_ctx.remain_flags = 0;
		raop_buffer->key_ctx.remain_bytes = 0;
	} else {
		/* Todo */
		return NULL;
	}

	/* Mark buffer as empty */
	raop_buffer->is_empty = 1;

	raop_buffer->et = et;
	raop_buffer->cn = cn;

	return raop_buffer;
}

void
raop_buffer_destroy(raop_buffer_t *raop_buffer)
{
	if (raop_buffer) {
		if (raop_buffer->cn == 1)
		  destroy_alac(raop_buffer->alac);
                else
		  destroy_aac_eld(raop_buffer->aac_eld);
		free(raop_buffer->buffer);
		free(raop_buffer);
	}
}

const ALACSpecificConfig *
raop_buffer_get_config(raop_buffer_t *raop_buffer)
{
	assert(raop_buffer);

	return &raop_buffer->alacConfig;
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
	int encryptedlen;
	AES_CTX aes_ctx;
	int outputlen;
	int i;

	assert(raop_buffer);

	/* Check packet data length is valid */
	if (datalen < 12 || datalen > RAOP_PACKET_LEN) {
		return -1;
	}

	/* Get correct seqnum for the packet */
	if (use_seqnum) {
		seqnum = (data[2] << 8) | data[3];
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
	if (entry->available && seqnum_cmp(entry->seqnum, seqnum) == 0) {
		/* Packet resend, we can safely ignore */
		return 0;
	}

	/* Update the raop_buffer entry header */
	entry->flags = data[0];
	entry->type = data[1];
	entry->seqnum = seqnum;
	entry->timestamp = (data[4] << 24) | (data[5] << 16) |
	                   (data[6] << 8) | data[7];
	entry->ssrc = (data[8] << 24) | (data[9] << 16) |
	              (data[10] << 8) | data[11];
	entry->available = 1;
	
	fprintf(stderr, "type=%x\n", data[1]);

#if 0
	{
		char datain[]={0x80, 0x60, 0xdf, 0x5b, 0x32, 0x37, 0x29, 0x2b
,  0x00, 0x00, 0x00, 0x00, 0x28, 0xf6, 0x71, 0x01
,  0x29, 0x5a, 0xbb, 0x0e, 0xa4, 0x4d, 0x9f, 0xc2
,  0x76, 0x83, 0xf6, 0x13, 0x7a, 0x16, 0x9a, 0x9f
,  0xab, 0xb0, 0xcd, 0xa7, 0x8a, 0x23, 0xb5, 0x2a
,  0x64, 0x87, 0x2b, 0xd8};
		char aesiv[]={0xa1, 0x2a, 0xf4, 0x6a, 0xf0, 0x5b, 0x83, 0x33,0x9b, 0xe9, 0xa8, 0x89, 0xd0, 0x38, 0x6e, 0x88};

		char aeskey[]={0xfb, 0x3b, 0xef, 0x53, 0xad, 0xd1, 0xf8, 0xcf, 0x27, 0xc6, 0xe7, 0x32, 0x82, 0xa1, 0x4e, 0x30};

		AES_KEY key_ctx;

		char key[]= {0x7f, 0xe1, 0x35, 0x7a, 0x22, 0x0a, 0xaf, 0xd8
,  0x3e, 0x67, 0xdd, 0xa0, 0x03, 0x1d, 0x49, 0x60
, 0xde, 0xa8, 0xeb, 0x1f, 0x1b, 0x31, 0xdb, 0x7f
, 0xf9, 0x55, 0x61, 0xb6, 0xc5, 0xa1, 0x4a, 0x3d
, 0x08, 0xc2, 0x52, 0xee, 0xc5, 0x99, 0x30, 0x60
, 0xe2, 0x64, 0xba, 0xc9, 0x3c, 0xf4, 0x2b, 0x8b
, 0x1d, 0x2d, 0xd7, 0x41, 0xcd, 0x5b, 0x62, 0x8e
, 0x27, 0xfd, 0x8a, 0xa9, 0xde, 0x90, 0x91, 0x42
, 0x99, 0xaf, 0xea, 0xa2, 0xd0, 0x76, 0xb5, 0xcf
, 0xea, 0xa6, 0xe8, 0x27, 0xf9, 0x6d, 0x1b, 0xeb
, 0x66, 0x7d, 0x4c, 0x70, 0x49, 0xd9, 0x5f, 0x6d
, 0x3a, 0xd0, 0x5d, 0xe8, 0x13, 0xcb, 0xf3, 0xcc
, 0x15, 0x5a, 0x5a, 0x80, 0x2f, 0xa4, 0x13, 0x1d
,0x73, 0x09, 0x02, 0x85, 0x29, 0x1b, 0xae, 0x24
,0x82, 0x6d, 0x61, 0xfe, 0x3a, 0xfe, 0x49, 0x9d
,0x5c, 0xad, 0x11, 0x98, 0x5a, 0x12, 0xac, 0xa1
,0x0c, 0xa7, 0xae, 0x59, 0xb8, 0x93, 0x28, 0x63
,0x66, 0x53, 0x58, 0x05, 0x06, 0xbf, 0xbd, 0x39
,0xe2, 0xc8, 0x58, 0x05, 0xb4, 0x34, 0x86, 0x3a
,0xde, 0xc0, 0x70, 0x66, 0x60, 0xec, 0xe5, 0x3c
,0x53, 0xef, 0x3b, 0xfb, 0xcf, 0xf8, 0xd1, 0xad
,0x32, 0xe7, 0xc6, 0x27, 0x30, 0x4e, 0xa1, 0x82
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
,0x0a, 0x00, 0x00, 0x00};

		fprintf(stderr, "testing\n");

		/* Initialize AES key ctx */
		AES_set_decrypt_key(aeskey, 128, &key_ctx);
		
		//new_AES_cbc_encrypt(&datain[12], &packetbuf, 32, &key_ctx, &aesiv, 0);
		new_AES_cbc_encrypt(&datain[12], &packetbuf, 32, (AES_KEY*)key, &aesiv, 0);

		alac_decode_frame(raop_buffer->alac, packetbuf, entry->audio_buffer, &outputlen);

		fprintf(stderr, "outputlen=%d\n", outputlen);
	}
#endif

	/* Decrypt audio data */
	if (raop_buffer->et == 1) {
		encryptedlen = (datalen-12)/16*16;
		AES_set_key(&aes_ctx, raop_buffer->aeskey, raop_buffer->aesiv, AES_MODE_128);
		AES_convert_key(&aes_ctx);
		AES_cbc_decrypt(&aes_ctx, &data[12], packetbuf, encryptedlen);
		memcpy(packetbuf+encryptedlen, &data[12+encryptedlen], datalen-12-encryptedlen);
	} else if (raop_buffer->et == 3) {
		encryptedlen = (datalen-12)/16*16;
		new_AES_cbc_encrypt(&data[12], &packetbuf, encryptedlen, &raop_buffer->key_ctx, &raop_buffer->key_ctx.iv, 0);
		memcpy(packetbuf+encryptedlen, &data[12+encryptedlen], datalen-12-encryptedlen);
		fprintf(stderr, "datain\n");
		for (i=0;i<32;i++)
			fprintf(stderr, "%2x ", data[12+i]);
		
		fprintf(stderr, "\ndata decrypted\n");
		for (i=0;i<32;i++)
			fprintf(stderr, "%2x ", packetbuf[i]);
	} else {
		return -2;
	}

	/* Decode audio data */
	outputlen = entry->audio_buffer_size;
	fprintf(stderr, "outputlen=%d, buffer=%p\n", outputlen, entry->audio_buffer);
	if (raop_buffer->cn == 1) 
		alac_decode_frame(raop_buffer->alac, packetbuf, entry->audio_buffer, &outputlen);
        else if (raop_buffer->cn == 3)
		aac_eld_decode_frame(raop_buffer->aac_eld, packetbuf, datalen - 12, entry->audio_buffer, &outputlen);
	else return -3;

	fprintf(stderr, "\naudio data\n");
	for (i=0;i<32;i++)
		fprintf(stderr, "%2x ", *((unsigned char*)entry->audio_buffer + i));

	fprintf(stderr, "outputlen=%d, datalen=%d\n", outputlen, datalen);
	/* fixme */
	if (outputlen < 0 || outputlen > 1408) outputlen = 1408;
	entry->audio_buffer_len = outputlen;

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
raop_buffer_dequeue(raop_buffer_t *raop_buffer, int *length, int no_resend)
{
	short buflen;
	raop_buffer_entry_t *entry;

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
	} else if (!entry->available) {
		/* Check how much we have space left in the buffer */
		if (buflen < RAOP_BUFFER_LENGTH) {
			/* Return nothing and hope resend gets on time */
			return NULL;
		}
		/* Risk of buffer overrun, return empty buffer */
	}

	/* Update buffer and validate entry */
	raop_buffer->first_seqnum += 1;
	if (!entry->available) {
		/* Return an empty audio buffer to skip audio */
		*length = entry->audio_buffer_size;
		memset(entry->audio_buffer, 0, *length);
		return entry->audio_buffer;
	}
	entry->available = 0;

	/* Return entry audio buffer */
	*length = entry->audio_buffer_len;
	entry->audio_buffer_len = 0;
	return entry->audio_buffer;
}

void
raop_buffer_handle_resends(raop_buffer_t *raop_buffer, raop_resend_cb_t resend_cb, void *opaque)
{
	raop_buffer_entry_t *entry;

	assert(raop_buffer);
	assert(resend_cb);

	if (seqnum_cmp(raop_buffer->first_seqnum, raop_buffer->last_seqnum) < 0) {
		int seqnum, count;

		for (seqnum=raop_buffer->first_seqnum; seqnum_cmp(seqnum, raop_buffer->last_seqnum)<0; seqnum++) {
			entry = &raop_buffer->entries[seqnum % RAOP_BUFFER_LENGTH];
			if (entry->available) {
				break;
			}
		}
		if (seqnum_cmp(seqnum, raop_buffer->first_seqnum) == 0) {
			return;
		}
		count = seqnum_cmp(seqnum, raop_buffer->first_seqnum);
		resend_cb(opaque, raop_buffer->first_seqnum, count);
	}
}

void
raop_buffer_flush(raop_buffer_t *raop_buffer, int next_seq)
{
	int i;

	assert(raop_buffer);

	for (i=0; i<RAOP_BUFFER_LENGTH; i++) {
		raop_buffer->entries[i].available = 0;
		raop_buffer->entries[i].audio_buffer_len = 0;
	}
	if (next_seq < 0 || next_seq > 0xffff) {
		raop_buffer->is_empty = 1;
	} else {
		raop_buffer->first_seqnum = next_seq;
		raop_buffer->last_seqnum = next_seq-1;
	}
}
