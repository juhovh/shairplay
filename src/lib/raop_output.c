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

#include "raop_output.h"
#include "threads.h"

#define RAOP_EVENT_FLUSH         0x01
#define RAOP_EVENT_PROCESS       0x02
#define RAOP_EVENT_PROGRESS      0x04
#define RAOP_EVENT_VOLUME        0x08
#define RAOP_EVENT_METADATA      0x10
#define RAOP_EVENT_COVERART      0x20
#define RAOP_EVENT_ACTIVE_REMOTE 0x40

struct raop_output_s {
	logger_t *logger;
	raop_callbacks_t callbacks;

	/* MUTEX LOCKED VARIABLES START */
	/* These variables only edited mutex locked */
	int running;
	int joined;

	unsigned char bit_depth;
	unsigned char channels;
	unsigned int sample_rate;

	int events;
	unsigned int timestamp;
	void *buffer;
	int buffer_len;
	int next_seq;
	unsigned int progress_start;
	unsigned int progress_curr;
	unsigned int progress_end;
	float volume;
	void *metadata;
	int metadata_len;
	char *coverart_type;
	void *coverart;
	int coverart_len;
	char *dacp_id;
	char *active_remote;

	thread_handle_t thread;
	mutex_handle_t mutex;
	cond_handle_t cond;
	/* MUTEX LOCKED VARIABLES END */
};

raop_output_t *
raop_output_init(logger_t *logger, raop_callbacks_t *callbacks)
{
	raop_output_t *raop_output;

	assert(logger);
	assert(callbacks);

	raop_output = calloc(1, sizeof(raop_output_t));
	if (!raop_output) {
		return NULL;
	}
	raop_output->logger = logger;
	memcpy(&raop_output->callbacks, callbacks, sizeof(raop_callbacks_t));

	raop_output->running = 0;
	raop_output->joined = 1;

	// FIXME: Use proper size
	raop_output->buffer = malloc(16384);

	MUTEX_CREATE(raop_output->mutex);
	COND_CREATE(raop_output->cond);

	return raop_output;
}

void
raop_output_destroy(raop_output_t *raop_output)
{
	if (raop_output) {
		raop_output_stop(raop_output);

		COND_DESTROY(raop_output->cond);
		MUTEX_DESTROY(raop_output->mutex);
		free(raop_output->coverart);
		free(raop_output->coverart_type);
		free(raop_output->metadata);
		free(raop_output);
	}
}

static int
raop_output_process_events(raop_output_t *raop_output, void *cb_data)
{
	raop_callbacks_t *cbs;
	int events;
	unsigned int timestamp;
	void *buffer;
	int buffer_len;
	unsigned int progress_start;
	unsigned int progress_curr;
	unsigned int progress_end;
	float volume;
	void *metadata;
	int metadata_len;
	char *coverart_type;
	void *coverart;
	int coverart_len;
	char *dacp_id;
	char *active_remote;

	assert(raop_output);

	cbs = &raop_output->callbacks;
	MUTEX_LOCK(raop_output->mutex);
	if (!raop_output->events) {
		if (!raop_output->running) {
			MUTEX_UNLOCK(raop_output->mutex);
			return 1;
		}
		COND_WAIT(raop_output->cond, raop_output->mutex);
	}

	events = raop_output->events;
	timestamp = raop_output->timestamp;
	buffer = raop_output->buffer;
	buffer_len = raop_output->buffer_len;
	progress_start = raop_output->progress_start;
	progress_curr = raop_output->progress_curr;
	progress_end = raop_output->progress_end;
	volume = raop_output->volume;
	metadata = raop_output->metadata;
	metadata_len = raop_output->metadata_len;
	coverart_type = raop_output->coverart_type;
	coverart = raop_output->coverart;
	coverart_len = raop_output->coverart_len;
	dacp_id = raop_output->dacp_id;
	active_remote = raop_output->active_remote;

	raop_output->events = 0;
	/* FIXME: Buffer dequeue needed */
	raop_output->volume = 0.0f;
	raop_output->metadata = NULL;
	raop_output->metadata_len = 0;
	raop_output->coverart_type = NULL;
	raop_output->coverart = NULL;
	raop_output->coverart_len = 0;
	raop_output->dacp_id = NULL;
	raop_output->active_remote = NULL;
	MUTEX_UNLOCK(raop_output->mutex);

	if (events & RAOP_EVENT_FLUSH) {
		if (cbs->audio_flush) {
			cbs->audio_flush(cbs->cls, cb_data);
		}
	}
	if (events & RAOP_EVENT_PROCESS) {
		if (cbs->audio_process) {
			cbs->audio_process(cbs->cls, cb_data, buffer, buffer_len, timestamp);
		}
	}
	if (events & RAOP_EVENT_PROGRESS) {
		if (cbs->audio_set_progress) {
			cbs->audio_set_progress(cbs->cls, cb_data, progress_start, progress_curr, progress_end);
		}
	}
	if (events & RAOP_EVENT_VOLUME) {
		if (cbs->audio_set_volume) {
			cbs->audio_set_volume(cbs->cls, cb_data, volume);
		}
	}
	if (events & RAOP_EVENT_METADATA) {
		if (cbs->audio_set_metadata) {
			cbs->audio_set_metadata(cbs->cls, cb_data, metadata, metadata_len);
		}
	}
	if (events & RAOP_EVENT_COVERART) {
		if (cbs->audio_set_coverart) {
			cbs->audio_set_coverart(cbs->cls, cb_data, coverart_type, coverart, coverart_len);
		}
	}
	if (events & RAOP_EVENT_ACTIVE_REMOTE) {
		if (cbs->audio_set_active_remote) {
			cbs->audio_set_active_remote(cbs->cls, cb_data, dacp_id, active_remote);
		}
	}

	free(metadata);
	free(coverart);
	return 0;
}

static THREAD_RETVAL
raop_output_thread(void *arg)
{
	raop_output_t *raop_output = arg;
	void *cb_data = NULL;

	assert(raop_output);

	logger_log(raop_output->logger, LOGGER_INFO, "Started RAOP output thread");
	cb_data = raop_output->callbacks.audio_init(raop_output->callbacks.cls,
	                                            raop_output->bit_depth,
	                                            raop_output->channels,
	                                            raop_output->sample_rate);

	while (1) {
		/* Check if we are still running and process callbacks */
		if (raop_output_process_events(raop_output, cb_data)) {
			break;
		}
	}
	logger_log(raop_output->logger, LOGGER_INFO, "Exiting RAOP output thread");
	raop_output->callbacks.audio_destroy(raop_output->callbacks.cls, cb_data);

	return 0;
}

void
raop_output_start(raop_output_t *raop_output, unsigned char bit_depth, unsigned char channels, unsigned int sample_rate)
{
	assert(raop_output);

	MUTEX_LOCK(raop_output->mutex);
	if (raop_output->running || !raop_output->joined) {
		MUTEX_UNLOCK(raop_output->mutex);
		return;
	}

	/* Create the thread and initialize running values */
	raop_output->running = 1;
	raop_output->joined = 0;

	raop_output->bit_depth = bit_depth;
	raop_output->channels = channels;
	raop_output->sample_rate = sample_rate;

	THREAD_CREATE(raop_output->thread, raop_output_thread, raop_output);
	MUTEX_UNLOCK(raop_output->mutex);
}

void
raop_output_flush(raop_output_t *raop_output, int next_seq)
{
	assert(raop_output);

	/* Call flush in thread instead */
	MUTEX_LOCK(raop_output->mutex);
	raop_output->next_seq = next_seq;
	raop_output->events |= RAOP_EVENT_FLUSH;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);
}

void
raop_output_process(raop_output_t *raop_output, unsigned int timestamp, const void *data, int datalen)
{
	assert(raop_output);

	if (datalen <= 0) {
		return;
	}

	/* Call process in thread instead */
	MUTEX_LOCK(raop_output->mutex);
	raop_output->timestamp = timestamp;
	memcpy(raop_output->buffer, data, datalen);
	raop_output->buffer_len = datalen;
	raop_output->events |= RAOP_EVENT_PROCESS;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);
}

void
raop_output_set_progress(raop_output_t *raop_output, unsigned int start, unsigned int curr, unsigned int end)
{
	assert(raop_output);

	/* Set progress in thread instead */
	MUTEX_LOCK(raop_output->mutex);
	raop_output->progress_start = start;
	raop_output->progress_curr = curr;
	raop_output->progress_end = end;
	raop_output->events |= RAOP_EVENT_PROGRESS;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);
}

void
raop_output_set_volume(raop_output_t *raop_output, float volume)
{
	assert(raop_output);

	if (volume > 0.0f) {
		volume = 0.0f;
	} else if (volume < -144.0f) {
		volume = -144.0f;
	}

	/* Set volume in thread instead */
	MUTEX_LOCK(raop_output->mutex);
	raop_output->volume = volume;
	raop_output->events |= RAOP_EVENT_VOLUME;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);
}

void
raop_output_set_metadata(raop_output_t *raop_output, const void *data, int datalen)
{
	void *metadata;

	assert(raop_output);

	if (datalen <= 0) {
		return;
	}
	metadata = malloc(datalen);
	if (!metadata) {
		return;
	}
	memcpy(metadata, data, datalen);

	/* Set metadata in thread instead */
	MUTEX_LOCK(raop_output->mutex);
	free(raop_output->metadata);
	raop_output->metadata = metadata;
	raop_output->metadata_len = datalen;
	raop_output->events |= RAOP_EVENT_METADATA;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);
}

void
raop_output_set_coverart(raop_output_t *raop_output, const char *type, const void *data, int datalen)
{
	char *coverart_type;
	void *coverart;

	assert(raop_output);
	assert(type);

	if (datalen <= 0) {
		return;
	}
	coverart_type = strdup(type);
	coverart = malloc(datalen);
	if (!coverart_type || !coverart) {
		free(coverart_type);
		free(coverart);
		return;
	}
	memcpy(coverart, data, datalen);

	/* Set coverart in thread instead */
	MUTEX_LOCK(raop_output->mutex);
	free(raop_output->coverart);
	raop_output->coverart_type = coverart_type;
	raop_output->coverart = coverart;
	raop_output->coverart_len = datalen;
	raop_output->events |= RAOP_EVENT_COVERART;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);
}

void
raop_output_set_active_remote(raop_output_t *raop_output, const char *dacp_id, const char *active_remote)
{
	char *our_dacp_id;
	char *our_active_remote;

	assert(raop_output);

	our_dacp_id = strdup(dacp_id);
	if (!our_dacp_id) {
		return;
	}
	our_active_remote = strdup(active_remote);
	if (!our_active_remote) {
		free(our_dacp_id);
		return;
	}

	/* Set active remote in thread instead */
	MUTEX_LOCK(raop_output->mutex);
	free(raop_output->dacp_id);
	free(raop_output->active_remote);
	raop_output->dacp_id = our_dacp_id;
	raop_output->active_remote = our_active_remote;
	raop_output->events |= RAOP_EVENT_ACTIVE_REMOTE;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);
}

void
raop_output_stop(raop_output_t *raop_output)
{
	assert(raop_output);

	/* Check that we are running and thread is not
	 * joined (should never be while still running) */
	MUTEX_LOCK(raop_output->mutex);
	if (!raop_output->running || raop_output->joined) {
		MUTEX_UNLOCK(raop_output->mutex);
		return;
	}
	raop_output->running = 0;
	MUTEX_UNLOCK(raop_output->mutex);
	COND_SIGNAL(raop_output->cond);

	THREAD_JOIN(raop_output->thread);

	MUTEX_LOCK(raop_output->mutex);
	raop_output->joined = 1;
	MUTEX_UNLOCK(raop_output->mutex);
}
