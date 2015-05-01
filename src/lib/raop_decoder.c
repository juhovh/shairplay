/**
 *  Copyright (C) 2015  Juho Vähä-Herttua
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

#include "raop_decoder.h"
#include "utils.h"
#include "bio.h"

#include <stdint.h>
#include "crypto/crypto.h"
#include "alac/alac.h"

#define RAOP_AESKEY_LEN 16
#define RAOP_AESIV_LEN  16
#define RAOP_BUFFER_LEN 32768

/* From ALACMagicCookieDescription.txt at http://http://alac.macosforge.org/ */
typedef struct {
	unsigned int frameLength;
	unsigned char compatibleVersion;
	unsigned char bitDepth;
	unsigned char pb;
	unsigned char mb;
	unsigned char kb;
	unsigned char numChannels;
	unsigned short maxRun;
	unsigned int maxFrameBytes;
	unsigned int avgBitRate;
	unsigned int sampleRate;
} ALACSpecificConfig;

struct raop_decoder_s {
	logger_t *logger;

	/* ALAC decoder */
	ALACSpecificConfig alacConfig;
	alac_file *alac;

	/* AES key and IV */
	int is_encrypted;
	unsigned char aeskey[RAOP_AESKEY_LEN];
	unsigned char aesiv[RAOP_AESIV_LEN];
};

static int
get_alac_fmtp_info(ALACSpecificConfig *config, const char *fmtp)
{
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

	return 0;
}

static void
set_decoder_info(alac_file *alac, ALACSpecificConfig *config)
{
	unsigned char decoder_info[48];
	memset(decoder_info, 0, sizeof(decoder_info));

	/* Construct decoder info buffer */
	bio_set_be_u32(&decoder_info[24], config->frameLength);
	decoder_info[28] = config->compatibleVersion;
	decoder_info[29] = config->bitDepth;
	decoder_info[30] = config->pb;
	decoder_info[31] = config->mb;
	decoder_info[32] = config->kb;
	decoder_info[33] = config->numChannels;
	bio_set_be_u16(&decoder_info[34], config->maxRun);
	bio_set_be_u32(&decoder_info[36], config->maxFrameBytes);
	bio_set_be_u32(&decoder_info[40], config->avgBitRate);
	bio_set_be_u32(&decoder_info[44], config->sampleRate);
	alac_set_info(alac, (char *) decoder_info);
}

raop_decoder_t *
raop_decoder_init(logger_t *logger,
                  const char *rtpmap, const char *fmtp,
                  const unsigned char *aeskey,
                  const unsigned char *aesiv)
{
	raop_decoder_t *raop_decoder;
	ALACSpecificConfig *alacConfig;

	assert(logger);
	assert(fmtp);

	raop_decoder = calloc(1, sizeof(raop_decoder_t));
	if (!raop_decoder) {
		return NULL;
	}
	raop_decoder->logger = logger;

	alacConfig = &raop_decoder->alacConfig;
	if (get_alac_fmtp_info(alacConfig, fmtp) < 0) {
		free(raop_decoder);
		return NULL;
	}

	/* Initialize ALAC decoder */
	raop_decoder->alac = alac_create(alacConfig->bitDepth,
	                                 alacConfig->numChannels);
	if (!raop_decoder->alac) {
		free(raop_decoder);
		return NULL;
	}
	set_decoder_info(raop_decoder->alac, alacConfig);

	/* Initialize AES keys */
	if (aeskey && aesiv) {
		raop_decoder->is_encrypted = 1;
		memcpy(raop_decoder->aeskey, aeskey, RAOP_AESKEY_LEN);
		memcpy(raop_decoder->aesiv, aesiv, RAOP_AESIV_LEN);
	}

	return raop_decoder;
}

void
raop_decoder_destroy(raop_decoder_t *raop_decoder)
{
	if (raop_decoder) {
		free(raop_decoder);
	}
}

unsigned char raop_decoder_get_channels(raop_decoder_t *raop_decoder)
{
	assert(raop_decoder);

	return raop_decoder->alacConfig.numChannels;
}

unsigned char raop_decoder_get_bit_depth(raop_decoder_t *raop_decoder)
{
	assert(raop_decoder);

	return raop_decoder->alacConfig.bitDepth;
}

unsigned int raop_decoder_get_sample_rate(raop_decoder_t *raop_decoder)
{
	assert(raop_decoder);

	return raop_decoder->alacConfig.sampleRate;
}

unsigned int raop_decoder_get_frame_length(raop_decoder_t *raop_decoder)
{
	assert(raop_decoder);

	return raop_decoder->alacConfig.frameLength;
}

void raop_decoder_decode(raop_decoder_t *raop_decoder,
                         void *input, int inputlen,
                         void *output, int *outputlen)
{
	unsigned char buffer[RAOP_BUFFER_LEN];

	assert(raop_decoder);
	assert(input);
	assert(inputlen >= 0);
	assert(inputlen <= RAOP_BUFFER_LEN);
	assert(output);
	assert(outputlen);

	if (raop_decoder->is_encrypted) {
		int encryptedlen = (inputlen / 16) * 16;
		AES_CTX aes_ctx;

		AES_set_key(&aes_ctx, raop_decoder->aeskey, raop_decoder->aesiv, AES_MODE_128);
		AES_convert_key(&aes_ctx); /* Convert to decrypt key */
		AES_cbc_decrypt(&aes_ctx, input, buffer, encryptedlen);
		memcpy(buffer + encryptedlen, &input[encryptedlen], inputlen - encryptedlen);
	} else {
		memcpy(buffer, input, inputlen);
	}

	/* Decode ALAC audio data */
	alac_decode_frame(raop_decoder->alac, buffer, output, outputlen);
}
