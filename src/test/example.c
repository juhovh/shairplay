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
audio_set_metadata(void *cls, void *session, const void *buffer, int buflen)
{
	int orig = buflen;
	FILE *file = fopen("metadata.bin", "wb");
	while (buflen > 0) {
		buflen -= fwrite(buffer+orig-buflen, 1, buflen, file);
	}
	fclose(file);
	printf("Metadata of length %d saved as metadata.bin\n", orig);
}

static void
audio_set_coverart(void *cls, void *session, const void *buffer, int buflen)
{
	int orig = buflen;
	FILE *file = fopen("coverart.jpg", "wb");
	while (buflen > 0) {
		buflen -= fwrite(buffer+orig-buflen, 1, buflen, file);
	}
	fclose(file);
	printf("Coverart of length %d saved as coverart.jpg\n", orig);
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

static void
raop_log_callback(void *cls, int level, const char *msg)
{
	printf("RAOP LOG(%d): %s\n", level, msg);
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
	raop_cbs.audio_set_metadata = audio_set_metadata;
	raop_cbs.audio_set_coverart = audio_set_coverart;
	raop_cbs.audio_process = audio_process;
	raop_cbs.audio_flush = audio_flush;
	raop_cbs.audio_destroy = audio_destroy;

	raop = raop_init_from_keyfile(10, &raop_cbs, "airport.key", NULL);
	raop_set_log_level(raop, RAOP_LOG_DEBUG);
	raop_set_log_callback(raop, &raop_log_callback, NULL);
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

