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

#ifndef RAOP_DECODER_H
#define RAOP_DECODER_H

#include "raop.h"
#include "logger.h"

typedef struct raop_decoder_s raop_decoder_t;

raop_decoder_t *raop_decoder_init(logger_t *logger,
                                  const char *rtpmap, const char *fmtp,
                                  const unsigned char *aeskey,
                                  const unsigned char *aesiv);
void raop_decoder_destroy(raop_decoder_t *raop_decoder);

unsigned char raop_decoder_get_channels(raop_decoder_t *raop_decoder);
unsigned char raop_decoder_get_bit_depth(raop_decoder_t *raop_decoder);
unsigned int raop_decoder_get_sample_rate(raop_decoder_t *raop_decoder);
unsigned int raop_decoder_get_frame_length(raop_decoder_t *raop_decoder);

void raop_decoder_decode(raop_decoder_t *raop_decoder, void *input, int inputlen, void *output, int *outputlen);

#endif
