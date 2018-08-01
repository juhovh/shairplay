/**
 *  Copyright (C) 2018  Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

/* This file should be only included from raop.c as it defines static handler
 * functions and depends on raop internals */

typedef void (*raop_handler_t)(raop_conn_t *, http_request_t *,
                               http_response_t *, char **, int *);

static void
raop_handler_none(raop_conn_t *conn,
                  http_request_t *request, http_response_t *response,
                  char **response_data, int *response_datalen)
{
}

static void
raop_handler_pairsetup(raop_conn_t *conn,
                       http_request_t *request, http_response_t *response,
                       char **response_data, int *response_datalen)
{
	unsigned char public_key[32];
	const char *data;
	int datalen;

	data = http_request_get_data(request, &datalen);
	if (datalen != 32) {
		logger_log(conn->raop->logger, LOGGER_ERR, "Invalid pair-setup data");
		return;
	}

	pairing_get_public_key(conn->raop->pairing, public_key);

	*response_data = malloc(sizeof(public_key));
	if (*response_data) {
		http_response_add_header(response, "Content-Type", "application/octet-stream");
		memcpy(*response_data, public_key, sizeof(public_key));
		*response_datalen = sizeof(public_key);
	}
}

static void
raop_handler_pairverify(raop_conn_t *conn,
                        http_request_t *request, http_response_t *response,
                        char **response_data, int *response_datalen)
{
	unsigned char public_key[32];
	unsigned char signature[64];
	const unsigned char *data;
	int datalen;

	data = (unsigned char *) http_request_get_data(request, &datalen);
	if (datalen < 4) {
		logger_log(conn->raop->logger, LOGGER_ERR, "Invalid pair-verify data");
		return;
	}
	switch (data[0]) {
	case 1:
		if (datalen != 4 + 32 + 32) {
			logger_log(conn->raop->logger, LOGGER_ERR, "Invalid pair-verify data");
			return;
		}

		/* We can fall through these errors, the result will just be garbage... */
		if (pairing_session_handshake(conn->pairing, data + 4, data + 4 + 32)) {
			logger_log(conn->raop->logger, LOGGER_ERR, "Error initializing pair-verify handshake");
		}
		if (pairing_session_get_public_key(conn->pairing, public_key)) {
			logger_log(conn->raop->logger, LOGGER_ERR, "Error getting ECDH public key");
		}
		if (pairing_session_get_signature(conn->pairing, signature)) {
			logger_log(conn->raop->logger, LOGGER_ERR, "Error getting ED25519 signature");
		}
		*response_data = malloc(sizeof(public_key) + sizeof(signature));
		if (*response_data) {
			http_response_add_header(response, "Content-Type", "application/octet-stream");
			memcpy(*response_data, public_key, sizeof(public_key));
			memcpy(*response_data + sizeof(public_key), signature, sizeof(signature));
			*response_datalen = sizeof(public_key) + sizeof(signature);
		}
		break;
	case 0:
		if (datalen != 4 + 64) {
			logger_log(conn->raop->logger, LOGGER_ERR, "Invalid pair-verify data");
			return;
		}

		if (pairing_session_finish(conn->pairing, data + 4)) {
			logger_log(conn->raop->logger, LOGGER_ERR, "Incorrect pair-verify signature");
			http_response_set_disconnect(response, 1);
			return;
		}
		break;
	}
}

static void
raop_handler_fpsetup(raop_conn_t *conn,
                        http_request_t *request, http_response_t *response,
                        char **response_data, int *response_datalen)
{
	const unsigned char *data;
	int datalen;

	data = (unsigned char *) http_request_get_data(request, &datalen);
	if (datalen == 16) {
		*response_data = malloc(142);
		if (*response_data) {
			if (!fairplay_setup(conn->fairplay, data, (unsigned char *) *response_data)) {
				*response_datalen = 142;
			} else {
				// Handle error?
				free(*response_data);
				*response_data = NULL;
			}
		}
	} else if (datalen == 164) {
		*response_data = malloc(32);
		if (*response_data) {
			if (!fairplay_handshake(conn->fairplay, data, (unsigned char *) *response_data)) {
				*response_datalen = 32;
			} else {
				// Handle error?
				free(*response_data);
				*response_data = NULL;
			}
		}
	} else {
		logger_log(conn->raop->logger, LOGGER_ERR, "Invalid fp-setup data length");
		return;
	}
}

static void
raop_handler_options(raop_conn_t *conn,
                     http_request_t *request, http_response_t *response,
                     char **response_data, int *response_datalen)
{
	http_response_add_header(response, "Public", "ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER");
}

static void
raop_handler_announce(raop_conn_t *conn,
                      http_request_t *request, http_response_t *response,
                      char **response_data, int *response_datalen)
{
	const char *data;
	int datalen;

	unsigned char aeskey[16];
	unsigned char aesiv[16];
	int aeskeylen = -1, aesivlen = -1;

	data = http_request_get_data(request, &datalen);
	if (data) {
		sdp_t *sdp;
		const char *remotestr, *rtpmapstr, *fmtpstr, *rsaaeskeystr, *fpaeskeystr, *aesivstr;

		sdp = sdp_init(data, datalen);
		remotestr = sdp_get_connection(sdp);
		rtpmapstr = sdp_get_rtpmap(sdp);
		fmtpstr = sdp_get_fmtp(sdp);
		rsaaeskeystr = sdp_get_rsaaeskey(sdp);
		fpaeskeystr = sdp_get_fpaeskey(sdp);
		aesivstr = sdp_get_aesiv(sdp);

		logger_log(conn->raop->logger, LOGGER_DEBUG, "connection: %s", remotestr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "rtpmap: %s", rtpmapstr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "fmtp: %s", fmtpstr);
		if (rsaaeskeystr) {
			logger_log(conn->raop->logger, LOGGER_DEBUG, "rsaaeskey: %s", rsaaeskeystr);
		}
		if (fpaeskeystr) {
			logger_log(conn->raop->logger, LOGGER_DEBUG, "fpaeskey: %s", fpaeskeystr);
		}
		logger_log(conn->raop->logger, LOGGER_DEBUG, "aesiv: %s", aesivstr);

		if (rsaaeskeystr) {
			aeskeylen = rsakey_decrypt(conn->raop->rsakey, aeskey, sizeof(aeskey), rsaaeskeystr);
		} else if (fpaeskeystr) {
			unsigned char fpaeskey[72];
			int fpaeskeylen;

			fpaeskeylen = rsakey_decode(conn->raop->rsakey, fpaeskey, sizeof(fpaeskey), fpaeskeystr);
			if (fpaeskeylen > 0) {
				fairplay_decrypt(conn->fairplay, fpaeskey, aeskey);
				aeskeylen = sizeof(aeskey);
			}
		}
		aesivlen = rsakey_decode(conn->raop->rsakey, aesiv, sizeof(aesiv), aesivstr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "aeskeylen: %d", aeskeylen);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "aesivlen: %d", aesivlen);

		if (conn->raop_rtp) {
			/* This should never happen */
			raop_rtp_destroy(conn->raop_rtp);
			conn->raop_rtp = NULL;
		}
		if (aeskeylen == sizeof(aeskey) && aesivlen == sizeof(aesiv)) {
			conn->raop_rtp = raop_rtp_init(conn->raop->logger, &conn->raop->callbacks,
						       remotestr, rtpmapstr, fmtpstr, aeskey, aesiv);
		}
		if (!conn->raop_rtp) {
			logger_log(conn->raop->logger, LOGGER_ERR, "Error initializing the audio decoder");
			http_response_set_disconnect(response, 1);
		}
		sdp_destroy(sdp);
	}
}

static void
raop_handler_setup(raop_conn_t *conn,
                   http_request_t *request, http_response_t *response,
                   char **response_data, int *response_datalen)
{
	unsigned short remote_cport=0, remote_tport=0;
	unsigned short cport=0, tport=0, dport=0;
	const char *transport;
	char buffer[1024];
	int use_udp;
	const char *dacp_id;
	const char *active_remote_header;

	dacp_id = http_request_get_header(request, "DACP-ID");
	active_remote_header = http_request_get_header(request, "Active-Remote");

	if (dacp_id && active_remote_header) {
		logger_log(conn->raop->logger, LOGGER_DEBUG, "DACP-ID: %s", dacp_id);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "Active-Remote: %s", active_remote_header);
		if (conn->raop_rtp) {
		    raop_rtp_remote_control_id(conn->raop_rtp, dacp_id, active_remote_header);
		}
	}

	transport = http_request_get_header(request, "Transport");
	assert(transport);

	logger_log(conn->raop->logger, LOGGER_INFO, "Transport: %s", transport);
	use_udp = strncmp(transport, "RTP/AVP/TCP", 11);
	if (use_udp) {
		char *original, *current, *tmpstr;
		
		current = original = strdup(transport);
		if (original) {
			while ((tmpstr = utils_strsep(&current, ";")) != NULL) {
				unsigned short value;
				int ret;
				
				ret = sscanf(tmpstr, "control_port=%hu", &value);
				if (ret == 1) {
					logger_log(conn->raop->logger, LOGGER_DEBUG, "Found remote control port: %hu", value);
					remote_cport = value;
				}
				ret = sscanf(tmpstr, "timing_port=%hu", &value);
				if (ret == 1) {
					logger_log(conn->raop->logger, LOGGER_DEBUG, "Found remote timing port: %hu", value);
					remote_tport = value;
				}
			}
		}
		free(original);
	}
	if (conn->raop_rtp) {
		raop_rtp_start(conn->raop_rtp, use_udp, remote_cport, remote_tport, &cport, &tport, &dport);
	} else {
		logger_log(conn->raop->logger, LOGGER_ERR, "RAOP not initialized at SETUP, playing will fail!");
		http_response_set_disconnect(response, 1);
	}

	memset(buffer, 0, sizeof(buffer));
	if (use_udp) {
		snprintf(buffer, sizeof(buffer)-1,
			 "RTP/AVP/UDP;unicast;mode=record;timing_port=%hu;events;control_port=%hu;server_port=%hu",
			 tport, cport, dport);
	} else {
		snprintf(buffer, sizeof(buffer)-1,
			 "RTP/AVP/TCP;unicast;interleaved=0-1;mode=record;server_port=%u",
			 dport);
	}
	logger_log(conn->raop->logger, LOGGER_INFO, "Responding with %s", buffer);
	http_response_add_header(response, "Transport", buffer);
	http_response_add_header(response, "Session", "DEADBEEF");
}

static void
raop_handler_get_parameter(raop_conn_t *conn,
                           http_request_t *request, http_response_t *response,
                           char **response_data, int *response_datalen)
{
	const char *content_type;
	const char *data;
	int datalen;

	content_type = http_request_get_header(request, "Content-Type");
	data = http_request_get_data(request, &datalen);
	if (!strcmp(content_type, "text/parameters")) {
		const char *current = data;

		while (current) {
			const char *next;
			int handled = 0;

			/* This is a bit ugly, but seems to be how airport works too */
			if (!strncmp(current, "volume\r\n", 8)) {
				const char volume[] = "volume: 0.000000\r\n";

				http_response_add_header(response, "Content-Type", "text/parameters");
				*response_data = strdup(volume);
				if (*response_data) {
					*response_datalen = strlen(*response_data);
				}
				handled = 1;
			}

			next = strstr(current, "\r\n");
			if (next && !handled) {
				logger_log(conn->raop->logger, LOGGER_WARNING,
				           "Found an unknown parameter: %.*s", (next - current), current);
				current = next + 2;
			} else if (next) {
				current = next + 2;
			} else {
				current = NULL;
			}
		}
	}
}

static void
raop_handler_set_parameter(raop_conn_t *conn,
                           http_request_t *request, http_response_t *response,
                           char **response_data, int *response_datalen)
{
	const char *content_type;
	const char *data;
	int datalen;

	content_type = http_request_get_header(request, "Content-Type");
	data = http_request_get_data(request, &datalen);
	if (!strcmp(content_type, "text/parameters")) {
		char *datastr;
		datastr = calloc(1, datalen+1);
		if (data && datastr && conn->raop_rtp) {
			memcpy(datastr, data, datalen);
			if (!strncmp(datastr, "volume: ", 8)) {
				float vol = 0.0;
				sscanf(datastr+8, "%f", &vol);
				raop_rtp_set_volume(conn->raop_rtp, vol);
			} else if (!strncmp(datastr, "progress: ", 10)) {
				unsigned int start, curr, end;
				sscanf(datastr+10, "%u/%u/%u", &start, &curr, &end);
				raop_rtp_set_progress(conn->raop_rtp, start, curr, end);
			}
		} else if (!conn->raop_rtp) {
			logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP not initialized at SET_PARAMETER");
		}
		free(datastr);
	} else if (!strcmp(content_type, "image/jpeg") || !strcmp(content_type, "image/png")) {
		logger_log(conn->raop->logger, LOGGER_INFO, "Got image data of %d bytes", datalen);
		if (conn->raop_rtp) {
			raop_rtp_set_coverart(conn->raop_rtp, data, datalen);
		} else {
			logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP not initialized at SET_PARAMETER coverart");
		}
	} else if (!strcmp(content_type, "application/x-dmap-tagged")) {
		logger_log(conn->raop->logger, LOGGER_INFO, "Got metadata of %d bytes", datalen);
		if (conn->raop_rtp) {
			raop_rtp_set_metadata(conn->raop_rtp, data, datalen);
		} else {
			logger_log(conn->raop->logger, LOGGER_WARNING, "RAOP not initialized at SET_PARAMETER metadata");
		}
	}
}
