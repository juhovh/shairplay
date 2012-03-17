#ifndef RAOP_H
#define RAOP_H

#if defined (WIN32) && defined(DLL_EXPORT)
# define RAOP_API __declspec(dllexport)
#else
# define RAOP_API
#endif

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

RAOP_API raop_t *raop_init(raop_callbacks_t *callbacks, const char *pemkey);
RAOP_API raop_t *raop_init_from_keyfile(raop_callbacks_t *callbacks, const char *keyfile);

RAOP_API int raop_start(raop_t *raop, unsigned short *port, const char *hwaddr, int hwaddrlen);
RAOP_API void raop_stop(raop_t *raop);

RAOP_API void raop_destroy(raop_t *raop);

#ifdef __cplusplus
}
#endif
#endif
