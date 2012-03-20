#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "dnssd.h"
#include "raop.h"

static void *
audio_init(void *cls, int bits, int channels, int samplerate)
{
	return fopen("audio.pcm", "wb");
}

static void
audio_set_volume(void *cls, void *session, float volume)
{
	printf("Setting volume to %f\n", volume);
}

static void
audio_process(void *cls, void *session, const void *buffer, int buflen)
{
	int orig = buflen;
	while (buflen > 0) {
		buflen -= fwrite(buffer+orig-buflen, 1, buflen, session);
	}
}

static void
audio_flush(void *cls, void *session)
{
	printf("Flushing audio\n");
}

static void
audio_destroy(void *cls, void *session)
{
	fclose(session);
}

int
main(int argc, char *argv[])
{
        const char *name = "AppleTV";
        unsigned short raop_port = 5000;
        const char hwaddr[] = { 0x48, 0x5d, 0x60, 0x7c, 0xee, 0x22 };

	dnssd_t *dnssd;
	raop_t *raop;
	raop_callbacks_t raop_cbs;

	raop_cbs.cls = NULL;
	raop_cbs.audio_init = audio_init;
	raop_cbs.audio_set_volume = audio_set_volume;
	raop_cbs.audio_process = audio_process;
	raop_cbs.audio_flush = audio_flush;
	raop_cbs.audio_destroy = audio_destroy;

	raop = raop_init_from_keyfile(&raop_cbs, "airport.key");
	raop_start(raop, &raop_port, hwaddr, sizeof(hwaddr), "test");

	dnssd = dnssd_init(NULL);
	dnssd_register_raop(dnssd, name, raop_port, hwaddr, sizeof(hwaddr), 1);

#ifndef WIN32
	sleep(100);
#else
	Sleep(100*1000);
#endif

	dnssd_unregister_raop(dnssd);
	dnssd_destroy(dnssd);

	raop_stop(raop);
	raop_destroy(raop);

	return 0;
}

