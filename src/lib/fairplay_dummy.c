#include <stdlib.h>

#include "fairplay.h"

struct fairplay_s {
};

fairplay_t *
fairplay_init(logger_t *logger)
{
	return NULL;
}

int
fairplay_setup(fairplay_t *fp, const unsigned char req[16], unsigned char res[142])
{
	return -1;
}

int
fairplay_handshake(fairplay_t *fp, const unsigned char req[164], unsigned char res[32])
{
	return -1;
}

int
fairplay_decrypt(fairplay_t *fp, const unsigned char input[72], unsigned char output[16])
{
	return -1;
}

void
fairplay_destroy(fairplay_t *fp)
{
}
