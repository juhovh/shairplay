#ifndef __SHAIRPLAY_AUDIO_H__
#define __SHAIRPLAY_AUDIO_H__

#include <ao/ao.h>

int audio_prepare(char *driver, char *devicename, char *deviceid, raop_callbacks_t *raop_cbs);
void audio_shutdown();

#endif
