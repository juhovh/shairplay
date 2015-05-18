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

#ifndef RAOP_OUTPUT_H
#define RAOP_OUTPUT_H

/* For raop_callbacks_t */
#include "raop.h"
#include "logger.h"

typedef struct raop_output_s raop_output_t;

raop_output_t *raop_output_init(logger_t *logger, raop_callbacks_t *callbacks);
void raop_output_start(raop_output_t *raop_output, unsigned char bit_depth, unsigned char channels, unsigned int sample_rate);
void raop_output_flush(raop_output_t *raop_output, int next_seq);
void raop_output_process(raop_output_t *raop_output, unsigned int timestamp, const void *data, int datalen);
void raop_output_set_progress(raop_output_t *raop_output, unsigned int start, unsigned int curr, unsigned int end);
void raop_output_set_volume(raop_output_t *raop_output, float volume);
void raop_output_set_metadata(raop_output_t *raop_output, const void *data, int datalen);
void raop_output_set_coverart(raop_output_t *raop_output, const char *type, const void *data, int datalen);
void raop_output_set_active_remote(raop_output_t *raop_output, const char *dacp_id, const char *active_remote);
void raop_output_stop(raop_output_t *raop_output);
void raop_output_destroy(raop_output_t *raop_output);

#endif
