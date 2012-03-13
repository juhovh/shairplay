#ifndef BASE64_H
#define BASE64_H

typedef struct base64_s base64_t;

base64_t *base64_init(const char *charlist, int use_padding, int skip_spaces);

int base64_encoded_length(base64_t *base64, int srclen);

int base64_encode(base64_t *base64, char *dst, const unsigned char *src, int srclen);
int base64_decode(base64_t *base64, unsigned char **dst, const char *src, int srclen);

void base64_destroy(base64_t *base64);

#endif
