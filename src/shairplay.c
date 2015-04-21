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

#include <shairplay/dnssd.h>
#include <shairplay/raop.h>
#include <shairplay/airplay.h>

#include "audio.h" 

#include "config.h"

typedef struct {
	char apname[56];
	char password[56];
	unsigned short port_raop;
	unsigned short port_airplay;
	char hwaddr[6];

	char ao_driver[56];
	char ao_devicename[56];
	char ao_deviceid[16];
	int  enable_airplay;
} shairplay_options_t;

static int running;

#ifndef WIN32

#include <signal.h>
static void
signal_handler(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		running = 0;
		break;
	}
}
static void
init_signals(void)
{
	struct sigaction sigact;

	sigact.sa_handler = signal_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
}

#endif


static int
parse_hwaddr(const char *str, char *hwaddr, int hwaddrlen)
{
	int slen, i;

	slen = 3*hwaddrlen-1;
	if (strlen(str) != slen) {
		return 1;
	}
	for (i=0; i<slen; i++) {
		if (str[i] == ':' && (i%3 == 2)) {
			continue;
		}
		if (str[i] >= '0' && str[i] <= '9') {
			continue;
		}
		if (str[i] >= 'a' && str[i] <= 'f') {
			continue;
		}
		return 1;
	}
	for (i=0; i<hwaddrlen; i++) {
		hwaddr[i] = (char) strtol(str+(i*3), NULL, 16);
	}
	return 0;
}

static int
parse_options(shairplay_options_t *opt, int argc, char *argv[])
{
	const char default_hwaddr[] = { 0x00, 0x24, 0xd7, 0xb2, 0x2e, 0x60 };

	char *path = argv[0];
	char *arg;

	/* Set default values for apname and port */
	strncpy(opt->apname, "Shairplay", sizeof(opt->apname)-1);
	opt->port_raop = 5000;
	opt->port_airplay = 7000;
	memcpy(opt->hwaddr, default_hwaddr, sizeof(opt->hwaddr));

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
			opt->port_raop = atoi(*++argv);
		} else if (!strncmp(arg, "--server_port=", 14)) {
			opt->port_raop = atoi(arg+14);
			opt->port_airplay = atoi(arg+14) + 2000;
		} else if (!strncmp(arg, "--hwaddr=", 9)) {
			if (parse_hwaddr(arg+9, opt->hwaddr, sizeof(opt->hwaddr))) {
				fprintf(stderr, "Invalid format given for hwaddr, aborting...\n");
				fprintf(stderr, "Please use hwaddr format: 01:45:89:ab:cd:ef\n");
				return 1;
			}
		} else if (!strncmp(arg, "--ao_driver=", 12)) {
			strncpy(opt->ao_driver, arg+12, sizeof(opt->ao_driver)-1);
		} else if (!strncmp(arg, "--ao_devicename=", 16)) {
			strncpy(opt->ao_devicename, arg+16, sizeof(opt->ao_devicename)-1);
		} else if (!strncmp(arg, "--ao_deviceid=", 14)) {
			strncpy(opt->ao_deviceid, arg+14, sizeof(opt->ao_deviceid)-1);
		} else if (!strncmp(arg, "--enable_airplay", 16)) {
			opt->enable_airplay = 1;
		} else if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
			fprintf(stderr, "Shairplay version %s\n", VERSION);
			fprintf(stderr, "Usage: %s [OPTION...]\n", path);
			fprintf(stderr, "\n");
			fprintf(stderr, "  -a, --apname=AirPort            Sets Airport name\n");
			fprintf(stderr, "  -p, --password=secret           Sets password\n");
			fprintf(stderr, "  -o, --server_port=5000          Sets port for RAOP service, port+2000 for AIRPLAY service\n");
			fprintf(stderr, "      --hwaddr=address            Sets the MAC address, useful if running multiple instances\n");
			fprintf(stderr, "      --ao_driver=driver          Sets the ao driver (optional)\n");
			fprintf(stderr, "      --ao_devicename=devicename  Sets the ao device name (optional)\n");
			fprintf(stderr, "      --ao_deviceid=id            Sets the ao device id (optional)\n");
			fprintf(stderr, "      --enable_airplay            start airplay service\n");
			fprintf(stderr, "  -h, --help                      This help\n");
			fprintf(stderr, "\n");
			return 1;
		}
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	shairplay_options_t options;
	ao_device *device = NULL;

	dnssd_t *dnssd;
	raop_t *raop;
	raop_callbacks_t raop_cbs;
	airplay_t *airplay;
	airplay_callbacks_t airplay_cbs;
	char *password = NULL;

	int error;

#ifndef WIN32
	init_signals();
#endif

	memset(&options, 0, sizeof(options));
	if (parse_options(&options, argc, argv)) {
		return 0;
	}

	if (audio_prepare(options.ao_driver, options.ao_devicename, options.ao_deviceid, &raop_cbs) < 0) 
		return -1;

	raop = raop_init_from_keyfile(10, &raop_cbs, "airport.key", NULL);
	if (raop == NULL) {
		fprintf(stderr, "Could not initialize the RAOP service\n");
		fprintf(stderr, "Please make sure the airport.key file is in the current directory.\n");
		return -1;
	}

	if (strlen(options.password)) {
		password = options.password;
	}
	raop_set_log_level(raop, RAOP_LOG_DEBUG);
	raop_start(raop, &options.port_raop, options.hwaddr, sizeof(options.hwaddr), password);

	if (options.enable_airplay) {
		/* TODO: fix the callbacks */
		memset(&airplay_cbs, 0, sizeof(airplay_cbs));
#if 0
		airplay_cbs.cls = &options;
		airplay_cbs.audio_init = audio_init;
		airplay_cbs.audio_process = audio_process;
		airplay_cbs.audio_destroy = audio_destroy;
		airplay_cbs.audio_set_volume = audio_set_volume;
#endif

		airplay = airplay_init_from_keyfile(10, &airplay_cbs, "airport.key", NULL);
		if (airplay == NULL) {
			fprintf(stderr, "Could not initialize the AIRPLAY service\n");
			fprintf(stderr, "Please make sure the airport.key file is in the current directory.\n");
			return -1;
		}

		airplay_set_log_level(airplay, AIRPLAY_LOG_DEBUG);
		airplay_start(airplay, &options.port_airplay, options.hwaddr, sizeof(options.hwaddr), password);
	}

	error = 0;
	dnssd = dnssd_init(&error);
	if (error) {
		fprintf(stderr, "ERROR: Could not initialize dnssd library!\n");
		fprintf(stderr, "------------------------------------------\n");
		fprintf(stderr, "You could try the following resolutions based on your OS:\n");
		fprintf(stderr, "Windows: Try installing http://support.apple.com/kb/DL999\n");
		fprintf(stderr, "Debian/Ubuntu: Try installing libavahi-compat-libdnssd-dev package\n");
		raop_destroy(raop);
		airplay_destroy(airplay);
		return -1;
	}

	dnssd_register_raop(dnssd, options.apname, options.port_raop, options.hwaddr, sizeof(options.hwaddr), 0);
	if (options.enable_airplay)
		dnssd_register_airplay(dnssd, options.apname, options.port_airplay, options.hwaddr, sizeof(options.hwaddr));

	running = 1;
	while (running) {
#ifndef WIN32
		sleep(1);
#else
		Sleep(1000);
#endif
	}

	dnssd_unregister_raop(dnssd);
	if (options.enable_airplay) dnssd_unregister_airplay(dnssd);
	dnssd_destroy(dnssd);

	raop_stop(raop);
	raop_destroy(raop);

	audio_shutdown();

	if (options.enable_airplay) {
		airplay_stop(airplay);
		airplay_destroy(airplay);
	}

	return 0;
}
