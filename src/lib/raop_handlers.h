
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
	int aeskeylen, aesivlen;

	data = http_request_get_data(request, &datalen);
	if (data) {
		sdp_t *sdp;
		const char *remotestr, *rtpmapstr, *fmtpstr, *aeskeystr, *aesivstr;

		sdp = sdp_init(data, datalen);
		remotestr = sdp_get_connection(sdp);
		rtpmapstr = sdp_get_rtpmap(sdp);
		fmtpstr = sdp_get_fmtp(sdp);
		aeskeystr = sdp_get_rsaaeskey(sdp);
		aesivstr = sdp_get_aesiv(sdp);

		logger_log(conn->raop->logger, LOGGER_DEBUG, "connection: %s", remotestr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "rtpmap: %s", rtpmapstr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "fmtp: %s", fmtpstr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "rsaaeskey: %s", aeskeystr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "aesiv: %s", aesivstr);

		aeskeylen = rsakey_decrypt(conn->raop->rsakey, aeskey, sizeof(aeskey), aeskeystr);
		aesivlen = rsakey_parseiv(conn->raop->rsakey, aesiv, sizeof(aesiv), aesivstr);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "aeskeylen: %d", aeskeylen);
		logger_log(conn->raop->logger, LOGGER_DEBUG, "aesivlen: %d", aesivlen);

		if (conn->raop_rtp) {
			/* This should never happen */
			raop_rtp_destroy(conn->raop_rtp);
			conn->raop_rtp = NULL;
		}
		conn->raop_rtp = raop_rtp_init(conn->raop->logger, &conn->raop->callbacks,
		                               remotestr, rtpmapstr, fmtpstr, aeskey, aesiv);
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
