#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "raop.h"
#include "raop_rtp.h"
#include "rsakey.h"
#include "httpd.h"
#include "sdp.h"

#include "global.h"
#include "utils.h"
#include "netutils.h"
#include "logger.h"

/* Actually 345 bytes for 2048-bit key */
#define MAX_SIGNATURE_LEN 512

struct raop_s {
	/* Callbacks for audio */
	raop_callbacks_t callbacks;

	/* Logger instance */
	logger_t logger;

	/* HTTP daemon and RSA key */
	httpd_t *httpd;
	rsakey_t *rsakey;

	/* Hardware address information */
	unsigned char hwaddr[MAX_HWADDR_LEN];
	int hwaddrlen;
};

struct raop_conn_s {
	raop_t *raop;
	raop_rtp_t *raop_rtp;

	unsigned char *local;
	int locallen;

	unsigned char *remote;
	int remotelen;
};
typedef struct raop_conn_s raop_conn_t;

static void *
conn_init(void *opaque, unsigned char *local, int locallen, unsigned char *remote, int remotelen)
{
	raop_conn_t *conn;
	int i;

	conn = calloc(1, sizeof(raop_conn_t));
	if (!conn) {
		return NULL;
	}
	conn->raop = opaque;
	conn->raop_rtp = NULL;

	logger_log(&conn->raop->logger, LOGGER_INFO, "Local: ");
	for (i=0; i<locallen; i++) {
		logger_log(&conn->raop->logger, LOGGER_INFO, "%02x", local[i]);
	}
	logger_log(&conn->raop->logger, LOGGER_INFO, "\n");
	logger_log(&conn->raop->logger, LOGGER_INFO, "Remote: ");
	for (i=0; i<remotelen; i++) {
		logger_log(&conn->raop->logger, LOGGER_INFO, "%02x", remote[i]);
	}
	logger_log(&conn->raop->logger, LOGGER_INFO, "\n");

	conn->local = malloc(locallen);
	assert(conn->local);
	memcpy(conn->local, local, locallen);

	conn->remote = malloc(remotelen);
	assert(conn->remote);
	memcpy(conn->remote, remote, remotelen);

	conn->locallen = locallen;
	conn->remotelen = remotelen;
	return conn;
}

static void
conn_request(void *ptr, http_request_t *request, http_response_t **response)
{
	raop_conn_t *conn = ptr;
	raop_t *raop = conn->raop;

	http_response_t *res;
	const char *method;
	const char *cseq;
	const char *challenge;

	method = http_request_get_method(request);
	cseq = http_request_get_header(request, "CSeq");
	if (!method || !cseq) {
		return;
	}

	res = http_response_init("RTSP/1.0", 200, "OK");
	http_response_add_header(res, "CSeq", cseq);
	http_response_add_header(res, "Apple-Jack-Status", "connected; type=analog");

	challenge = http_request_get_header(request, "Apple-Challenge");
	if (challenge) {
		char signature[MAX_SIGNATURE_LEN];

		memset(signature, 0, sizeof(signature));
		rsakey_sign(raop->rsakey, signature, sizeof(signature), challenge,
		            conn->local, conn->locallen, raop->hwaddr, raop->hwaddrlen);
		logger_log(&conn->raop->logger, LOGGER_DEBUG, "Got signature: %s\n", signature);
		http_response_add_header(res, "Apple-Response", signature);
	}
	if (!strcmp(method, "OPTIONS")) {
		http_response_add_header(res, "Public", "ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER");
	} else if (!strcmp(method, "ANNOUNCE")) {
		const char *data;
		int datalen;

		unsigned char aeskey[16];
		unsigned char aesiv[16];
		int aeskeylen, aesivlen;

		data = http_request_get_data(request, &datalen);
		if (data) {
			sdp_t *sdp = sdp_init(data, datalen);
			logger_log(&conn->raop->logger, LOGGER_DEBUG, "rsaaeskey: %s\n", sdp_get_rsaaeskey(sdp));
			logger_log(&conn->raop->logger, LOGGER_DEBUG, "aesiv: %s\n", sdp_get_aesiv(sdp));

			aeskeylen = rsakey_decrypt(raop->rsakey, aeskey, sizeof(aeskey),
			                           sdp_get_rsaaeskey(sdp));
			aesivlen = rsakey_parseiv(raop->rsakey, aesiv, sizeof(aesiv),
			                          sdp_get_aesiv(sdp));
			logger_log(&conn->raop->logger, LOGGER_DEBUG, "aeskeylen: %d\n", aeskeylen);
			logger_log(&conn->raop->logger, LOGGER_DEBUG, "aesivlen: %d\n", aesivlen);

			conn->raop_rtp = raop_rtp_init(&raop->logger, &raop->callbacks, sdp_get_fmtp(sdp), aeskey, aesiv);
			sdp_destroy(sdp);
		}
	} else if (!strcmp(method, "SETUP")) {
		unsigned short cport=0, tport=0, dport=0;
		const char *transport;
		char buffer[1024];
		int use_udp;

		transport = http_request_get_header(request, "Transport");
		assert(transport);

		logger_log(&conn->raop->logger, LOGGER_INFO, "Transport: %s\n", transport);
		use_udp = strncmp(transport, "RTP/AVP/TCP", 11);

		/* FIXME: Should use the parsed ports for resend */
		raop_rtp_start(conn->raop_rtp, use_udp, 1234, 1234, &cport, &tport, &dport);

		memset(buffer, 0, sizeof(buffer));
		if (use_udp) {
			snprintf(buffer, sizeof(buffer)-1,
			         "RTP/AVP/UDP;unicast;mode=record;timing_port=%u;events;control_port=%u;server_port=%u",
			         tport, cport, dport);
		} else {
			snprintf(buffer, sizeof(buffer)-1,
			         "RTP/AVP/TCP;unicast;interleaved=0-1;mode=record;server_port=%u",
			         dport);
		}
		logger_log(&conn->raop->logger, LOGGER_INFO, "Responding with %s\n", buffer);
		http_response_add_header(res, "Transport", buffer);
		http_response_add_header(res, "Session", "DEADBEEF");
	} else if (!strcmp(method, "SET_PARAMETER")) {
		const char *data;
		int datalen;
		char *datastr;

		data = http_request_get_data(request, &datalen);
		datastr = calloc(1, datalen+1);
		if (datastr) {
			memcpy(datastr, data, datalen);
			if (!strncmp(datastr, "volume: ", 8)) {
				float vol = 0.0;
				sscanf(data+8, "%f", &vol);
				raop_rtp_set_volume(conn->raop_rtp, vol);
			}
		}
	} else if (!strcmp(method, "FLUSH")) {
		const char *rtpinfo;
		int next_seq = -1;

		rtpinfo = http_request_get_header(request, "RTP-Info");
		assert(rtpinfo);

		logger_log(&conn->raop->logger, LOGGER_INFO, "RTP-Info: %s\n", rtpinfo);
		if (!strncmp(rtpinfo, "seq=", 4)) {
			next_seq = strtol(rtpinfo+4, NULL, 10);
		}
		raop_rtp_flush(conn->raop_rtp, next_seq);
	} else if (!strcmp(method, "TEARDOWN")) {
		http_response_add_header(res, "Connection", "close");
		raop_rtp_stop(conn->raop_rtp);
		raop_rtp_destroy(conn->raop_rtp);
		conn->raop_rtp = NULL;
	}
	http_response_finish(res, NULL, 0);

	logger_log(&conn->raop->logger, LOGGER_DEBUG, "Got request %s with URL %s\n", method, http_request_get_url(request));
	*response = res;
}

static void
conn_destroy(void *ptr)
{
	raop_conn_t *conn = ptr;

	if (conn->raop_rtp) {
		raop_rtp_destroy(conn->raop_rtp);
	}
	free(conn->local);
	free(conn->remote);
	free(conn);
}

raop_t *
raop_init(raop_callbacks_t *callbacks, const char *pemkey, const char *hwaddr, int hwaddrlen)
{
	raop_t *raop;
	httpd_t *httpd;
	rsakey_t *rsakey;
	httpd_callbacks_t httpd_cbs;

	assert(callbacks);
	assert(pemkey);
	assert(hwaddr);

	/* Initialize the network */
	if (netutils_init() < 0) {
		return NULL;
	}

	/* Validate the callbacks structure */
	if (!callbacks->audio_init || !callbacks->audio_set_volume ||
	    !callbacks->audio_process || !callbacks->audio_flush ||
	    !callbacks->audio_destroy) {
		return NULL;
	}

	/* Validate hardware address */
	if (hwaddrlen > MAX_HWADDR_LEN) {
		return NULL;
	}

	/* Allocate the raop_t structure */
	raop = calloc(1, sizeof(raop_t));
	if (!raop) {
		return NULL;
	}

	/* Initialize the logger */
	logger_init(&raop->logger);

	/* Set HTTP callbacks to our handlers */
	memset(&httpd_cbs, 0, sizeof(httpd_cbs));
	httpd_cbs.opaque = raop;
	httpd_cbs.conn_init = &conn_init;
	httpd_cbs.conn_request = &conn_request;
	httpd_cbs.conn_destroy = &conn_destroy;

	/* Initialize the http daemon */
	httpd = httpd_init(&raop->logger, &httpd_cbs, 10, 1);
	if (!httpd) {
		free(raop);
		return NULL;
	}

	/* Copy callbacks structure */
	memcpy(&raop->callbacks, callbacks, sizeof(raop_callbacks_t));

	/* Initialize RSA key handler */
	rsakey = rsakey_init_pem(pemkey);
	if (!rsakey) {
		free(httpd);
		free(raop);
		return NULL;
	}

	raop->httpd = httpd;
	raop->rsakey = rsakey;

	/* Copy hwaddr to resulting structure */
	memcpy(raop->hwaddr, hwaddr, hwaddrlen);
	raop->hwaddrlen = hwaddrlen;

	return raop;
}

raop_t *
raop_init_from_keyfile(raop_callbacks_t *callbacks, const char *keyfile, const char *hwaddr, int hwaddrlen)
{
	raop_t *raop;
	char *pemstr;

	if (utils_read_file(&pemstr, keyfile) < 0) {
		return NULL;
	}
	raop = raop_init(callbacks, pemstr, hwaddr, hwaddrlen);
	free(pemstr);
	return raop;
}

void
raop_destroy(raop_t *raop)
{
	if (raop) {
		raop_stop(raop);

		httpd_destroy(raop->httpd);
		rsakey_destroy(raop->rsakey);
		free(raop);

		/* Cleanup the network */
		netutils_cleanup();
	}
}

int
raop_start(raop_t *raop, unsigned short *port)
{
	assert(raop);
	assert(port);

	return httpd_start(raop->httpd, port);
}

void
raop_stop(raop_t *raop)
{
	assert(raop);

	httpd_stop(raop->httpd);
}

