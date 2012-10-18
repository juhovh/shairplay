#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <shairplay/dnssd.h>
#include <shairplay/raop.h>

#include <ao/ao.h>

#include "config.h"

typedef struct {
	char apname[56];
	char password[56];
	unsigned short port;

	char ao_driver[56];
	char ao_devicename[56];
	char ao_deviceid[16];
} shairplay_options_t;

typedef struct {
	ao_device *device;

	int buffering;
	int buflen;
	char buffer[8192];

	float volume;
} shairplay_session_t;


static ao_device *
audio_open_device(shairplay_options_t *opt, int bits, int channels, int samplerate)
{
	ao_device *device = NULL;
	ao_option *ao_options = NULL;
	ao_sample_format format;
	int driver_id;

	/* Get the libao driver ID */
	if (strlen(opt->ao_driver)) {
		driver_id = ao_driver_id(opt->ao_driver);
	} else {
		driver_id = ao_default_driver_id();
	}

	/* Add all available libao options */
	ao_append_option(&ao_options, "client_name", opt->apname);
	if (strlen(opt->ao_devicename)) {
		ao_append_option(&ao_options, "dev", opt->ao_devicename);
	}
	if (strlen(opt->ao_deviceid)) {
		ao_append_option(&ao_options, "id", opt->ao_deviceid);
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
	shairplay_options_t *options = cls;
	shairplay_session_t *session;

	session = calloc(1, sizeof(shairplay_session_t));
	assert(session);

	session->device = audio_open_device(options, bits, channels, samplerate);
	if (session->device == NULL) {
		printf("Error opening device %d\n", errno);
	}
	assert(session->device);

	session->buffering = 1;
	session->volume = 1.0f;
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
		printf("Buffering...\n");
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

static int
parse_options(shairplay_options_t *opt, int argc, char *argv[])
{
	char *path = argv[0];
	char *arg;

	strcpy(opt->apname, "Shairplay");
	opt->port = 5000;

	while ((arg = *++argv)) {
		if (!strcmp(arg, "-a")) {
			strncpy(opt->apname, *++argv, sizeof(opt->apname)-1);
		} else if (!strncmp(arg, "--apname=", 9)) {
			strncpy(opt->apname, arg+9, sizeof(opt->apname)-1);
		} else if (!strcmp(arg, "-p")) {
			strncpy(opt->password, *++argv, sizeof(opt->password)-1);
		} else if (!strncmp(arg, "--password=", 11)) {
			strncpy(opt->password, arg+11, sizeof(opt->password)-1);
		} else if (!strcmp(arg, "-o")) {
			opt->port = atoi(*++argv);
		} else if (!strncmp(arg, "--server_port=", 14)) {
			opt->port = atoi(arg+14);
		} else if (!strncmp(arg, "--ao_driver=", 12)) {
			strncpy(opt->ao_driver, arg+12, sizeof(opt->ao_driver)-1);
		} else if (!strncmp(arg, "--ao_devicename=", 16)) {
			strncpy(opt->ao_devicename, arg+16, sizeof(opt->ao_devicename)-1);
		} else if (!strncmp(arg, "--ao_deviceid=", 14)) {
			strncpy(opt->ao_deviceid, arg+14, sizeof(opt->ao_deviceid)-1);
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
			fprintf(stderr, "Shairplay version %s\n", VERSION);
			fprintf(stderr, "Usage: %s [OPTION...]\n", path);
			fprintf(stderr, "\n");
			fprintf(stderr, "  -a, --apname=AirPort            Sets Airport name\n");
			fprintf(stderr, "  -p, --password=secret           Sets password\n");
			fprintf(stderr, "  -o, --server_port=5000          Sets port for RAOP service\n");
			fprintf(stderr, "      --ao_driver=driver          Sets the ao driver (optional)\n");
			fprintf(stderr, "      --ao_devicename=devicename  Sets the ao device name (optional)\n");
			fprintf(stderr, "      --ao_deviceid=id            Sets the ao device id (optional)\n");
			fprintf(stderr, "  -h, --help                      This help\n");
			fprintf(stderr, "\n");
			return 1;
		}
	}

	/* Set default values for apname and port */
	if (!strlen(opt->apname)) {
		strncpy(opt->apname, "Shairplay", sizeof(opt->apname)-1);
	}
	if (!opt->port) {
		opt->port = 5000;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	const char hwaddr[] = { 0x48, 0x5d, 0x60, 0x7c, 0xee, 0x22 };

	shairplay_options_t options;
	ao_device *device = NULL;

	dnssd_t *dnssd;
	raop_t *raop;
	raop_callbacks_t raop_cbs;

	memset(&options, 0, sizeof(options));
	if (parse_options(&options, argc, argv)) {
		return 0;
	}

	ao_initialize();

	device = audio_open_device(&options, 16, 2, 44100);
	if (device == NULL) {
		fprintf(stderr, "Error opening audio device %d\n", errno);
		fprintf(stderr, "Please check your libao settings and try again\n");
		return -1;
	} else {
		ao_close(device);
		device = NULL;
	}

	memset(&raop_cbs, 0, sizeof(raop_cbs));
	raop_cbs.cls = &options;
	raop_cbs.audio_init = audio_init;
	raop_cbs.audio_process = audio_process;
	raop_cbs.audio_destroy = audio_destroy;
	raop_cbs.audio_set_volume = audio_set_volume;

	raop = raop_init_from_keyfile(10, &raop_cbs, "airport.key", NULL);
	if (raop == NULL) {
		fprintf(stderr, "Could not initialize the RAOP service\n");
		return -1;
	}

	raop_set_log_level(raop, RAOP_LOG_DEBUG);
	raop_start(raop, &options.port, hwaddr, sizeof(hwaddr), NULL);

	dnssd = dnssd_init(NULL);
	dnssd_register_raop(dnssd, options.apname, options.port, hwaddr, sizeof(hwaddr), 0);

#ifndef WIN32
	sleep(100);
#else
	Sleep(100*1000);
#endif

	dnssd_unregister_raop(dnssd);
	dnssd_destroy(dnssd);

	raop_stop(raop);
	raop_destroy(raop);

	ao_shutdown();

	return 0;
}
