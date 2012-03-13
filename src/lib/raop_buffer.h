#ifndef RAOP_BUFFER_H
#define RAOP_BUFFER_H

typedef struct raop_buffer_s raop_buffer_t;

/* From ALACMagicCookieDescription.txt at http://http://alac.macosforge.org/ */
typedef struct {
	unsigned int frameLength;
	unsigned char compatibleVersion;
	unsigned char bitDepth;
	unsigned char pb;
	unsigned char mb;
	unsigned char kb;
	unsigned char numChannels;
	unsigned short maxRun;
	unsigned int maxFrameBytes;
	unsigned int avgBitRate;
	unsigned int sampleRate;
} ALACSpecificConfig;

typedef int (*raop_resend_cb_t)(void *opaque, unsigned short seqno, unsigned short count);

raop_buffer_t *raop_buffer_init(const char *fmtp,
                                const unsigned char *aeskey,
                                const unsigned char *aesiv);

const ALACSpecificConfig *raop_buffer_get_config(raop_buffer_t *raop_buffer);
int raop_buffer_queue(raop_buffer_t *raop_buffer, unsigned char *data, unsigned short datalen, int use_seqnum);
const void *raop_buffer_dequeue(raop_buffer_t *raop_buffer, int *length, int no_resend);
void raop_buffer_handle_resends(raop_buffer_t *raop_buffer, raop_resend_cb_t resend_cb, void *opaque);
void raop_buffer_flush(raop_buffer_t *raop_buffer, int next_seq);

void raop_buffer_destroy(raop_buffer_t *raop_buffer);

#endif
