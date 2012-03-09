#ifndef HTTPD_H
#define HTTPD_H

#include "logger.h"
#include "http_request.h"
#include "http_response.h"

typedef struct httpd_s httpd_t;

struct httpd_callbacks_s {
	void* opaque;
	void* (*conn_init)(void *opaque, unsigned char *local, int locallen, unsigned char *remote, int remotelen);
	void  (*conn_request)(void *ptr, http_request_t *request, http_response_t **response);
	void  (*conn_destroy)(void *ptr);
};
typedef struct httpd_callbacks_s httpd_callbacks_t;


httpd_t *httpd_init(logger_t *logger, httpd_callbacks_t *callbacks, int max_connections, int use_rtsp);

int httpd_start(httpd_t *httpd, unsigned short *port);
void httpd_stop(httpd_t *httpd);

void httpd_destroy(httpd_t *httpd);


#endif
