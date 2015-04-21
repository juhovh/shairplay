/**
 *  Copyright (C) 2012-2013  Juho Vähä-Herttua
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included
 *  in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#ifdef WIN32
# include <windows.h>
#endif

#include <ao/ao.h>
#include "shairplay/raop.h"
//#include "lib/threads.h"

typedef struct {
	ao_device *device;

	int buffering;
	int buflen;
	char buffer[8192];

	float volume;

//	int running;
//	thread_handle_t thread;
} shairplay_session_t;

static char *ao_driver = NULL, *ao_devicename = NULL, *ao_deviceid = NULL;

static ao_device *
audio_open_device(char *driver, char *devicename, char *deviceid, int bits, int channels, int samplerate)
{
	ao_device *device = NULL;
	ao_option *ao_options = NULL;
	ao_sample_format format;
	int driver_id;

	/* Get the libao driver ID */
	if (strlen(driver)) {
		driver_id = ao_driver_id(driver);
	} else {
		driver_id = ao_default_driver_id();
	}

	/* Add all available libao options */
	if (strlen(devicename)) {
		ao_append_option(&ao_options, "dev", devicename);
	}
	if (strlen(deviceid)) {
		ao_append_option(&ao_options, "id", deviceid);
	}


	/* Set audio format */
	memset(&format, 0, sizeof(format));
	format.bits = bits;
	format.channels = channels;
	format.rate = samplerate;
	format.byte_format = AO_FMT_NATIVE;

	/* Try opening the actual device */
	device = ao_open_live(driver_id, &format, ao_options);
	ao_free_options(ao_options);
	return device;
}

static void *
audio_init(void *cls, int bits, int channels, int samplerate)
{
	shairplay_session_t *session;

	session = calloc(1, sizeof(shairplay_session_t));
	assert(session);

	session->device = audio_open_device(ao_driver, ao_devicename, ao_deviceid, bits, channels, samplerate);
	if (session->device == NULL) {
		printf("Error opening device %d\n", errno);
	}
	assert(session->device);

	session->buffering = 1;
	session->volume = 1.0f;

	//session->running = 1;
	//THREAD_CREATE(&session->thread, audio_thread, cls);
	return session;
}

static int
audio_output(shairplay_session_t *session, const void *buffer, int buflen)
{
	short *shortbuf;
	char tmpbuf[4096];
	int tmpbuflen, i;

	tmpbuflen = (buflen > sizeof(tmpbuf)) ? sizeof(tmpbuf) : buflen;
	memcpy(tmpbuf, buffer, tmpbuflen);
	if (ao_is_big_endian()) {
		for (i=0; i<tmpbuflen/2; i++) {
			char tmpch = tmpbuf[i*2];
			tmpbuf[i*2] = tmpbuf[i*2+1];
			tmpbuf[i*2+1] = tmpch;
		}
	}
	shortbuf = (short *)tmpbuf;
	for (i=0; i<tmpbuflen/2; i++) {
		shortbuf[i] = shortbuf[i] * session->volume;
	}
	ao_play(session->device, tmpbuf, tmpbuflen);
	return tmpbuflen;
}

static void
audio_process(void *cls, void *opaque, const void *buffer, int buflen)
{
	shairplay_session_t *session = opaque;
	int processed;

	if (session->buffering) {
		printf("Buffering... %d %d\n", session->buflen + buflen, sizeof(session->buffer));
		if (session->buflen+buflen < sizeof(session->buffer)) {
			memcpy(session->buffer+session->buflen, buffer, buflen);
			session->buflen += buflen;
			return;
		}
		session->buffering = 0;
		printf("Finished buffering...\n");

		processed = 0;
		while (processed < session->buflen) {
			processed += audio_output(session,
			                          session->buffer+processed,
			                          session->buflen-processed);
		}
		session->buflen = 0;
	}

	processed = 0;
	while (processed < buflen) {
		processed += audio_output(session,
		                          buffer+processed,
		                          buflen-processed);
	}
}

static void
audio_destroy(void *cls, void *opaque)
{
	shairplay_session_t *session = opaque;

	ao_close(session->device);
	free(session);
}

static void
audio_set_volume(void *cls, void *opaque, float volume)
{
	shairplay_session_t *session = opaque;
	session->volume = pow(10.0, 0.05*volume);
}

int audio_prepare(char *driver, char *devicename, char *deviceid, raop_callbacks_t *raop_cbs)
{
	ao_device *device = NULL;

	ao_initialize();

	device = audio_open_device(driver, devicename, deviceid, 16, 2, 44100);
	if (device == NULL) {
		fprintf(stderr, "Error opening audio device %d\n", errno);
		fprintf(stderr, "Please check your libao settings and try again\n");
		return -1;
	} else {
		ao_close(device);
		device = NULL;
	}

	ao_driver = driver;
	ao_devicename = devicename;
	ao_deviceid = deviceid;

	memset(raop_cbs, 0, sizeof(*raop_cbs));
	raop_cbs->cls = NULL;
	raop_cbs->audio_init = audio_init;
	raop_cbs->audio_process = audio_process;
	raop_cbs->audio_destroy = audio_destroy;
	raop_cbs->audio_set_volume = audio_set_volume;

	return 0;
}

void audio_shutdown()
{
	ao_shutdown();
}
