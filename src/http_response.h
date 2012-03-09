#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

typedef struct http_response_s http_response_t;

http_response_t *http_response_init(const char *protocol, int code, const char *message);

void http_response_add_header(http_response_t *response, const char *name, const char *value);
void http_response_finish(http_response_t *response, const char *data, int datalen);

const char *http_response_get_data(http_response_t *response, int *datalen);

void http_response_destroy(http_response_t *response);

#endif
