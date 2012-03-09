#ifndef RAOP_H
#define RAOP_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct raop_s raop_t;

struct raop_callbacks_s {
	void* cls;
	void  (*audio_init)(void *cls, void **session, int bits, int channels, int samplerate);
	void  (*audio_set_volume)(void *cls, void *session, float volume);
	void  (*audio_process)(void *cls, void *session, const void *buffer, int buflen);
	void  (*audio_flush)(void *cls, void *session);
	void  (*audio_destroy)(void *cls, void *session);
};
typedef struct raop_callbacks_s raop_callbacks_t;

raop_t *raop_init(raop_callbacks_t *callbacks, const char *pemkey, const char *hwaddr, int hwaddrlen);
raop_t *raop_init_from_keyfile(raop_callbacks_t *callbacks, const char *keyfile, const char *hwaddr, int hwaddrlen);

int raop_start(raop_t *raop, unsigned short *port);
void raop_stop(raop_t *raop);

void raop_destroy(raop_t *raop);

#ifdef __cplusplus
}
#endif
#endif
