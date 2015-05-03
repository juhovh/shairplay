/**
 *  Copyright (C) 2011-2012  Juho Vähä-Herttua
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "raop_rtp.h"
#include "raop.h"
#include "raop_buffer.h"
#include "raop_ntp.h"
#include "netutils.h"
#include "utils.h"
#include "compat.h"
#include "logger.h"
#include "bio.h"

#define NO_FLUSH (-42)

struct raop_rtp_s {
	logger_t *logger;
	raop_decoder_t *decoder;
	raop_callbacks_t callbacks;

	/* Buffer to handle all resends */
	raop_buffer_t *buffer;

	/* Remote address as sockaddr */
	struct sockaddr_storage remote_saddr;
	socklen_t remote_saddr_len;

	/* MUTEX LOCKED VARIABLES START */
	/* These variables only edited mutex locked */
	int running;
	int joined;

	float volume;
	int volume_changed;
	unsigned char *metadata;
	int metadata_len;
	unsigned char *coverart;
	int coverart_len;
	char *dacp_id;
	char *active_remote_header;
	unsigned int progress_start;
	unsigned int progress_curr;
	unsigned int progress_end;
	int progress_changed;

	int flush;
	thread_handle_t thread;
	mutex_handle_t run_mutex;
	/* MUTEX LOCKED VARIABLES END */

	/* Remote control and timing ports */
	unsigned short control_rport;
	unsigned short timing_rport;

	/* Sockets for control, timing and data */
	int csock, tsock, dsock;

	/* Local control, timing and data ports */
	unsigned short control_lport;
	unsigned short timing_lport;
	unsigned short data_lport;

	/* Initialized after the first control packet */
	struct sockaddr_storage control_saddr;
	socklen_t control_saddr_len;
	unsigned short control_seqnum;

	/* Sync information */
	unsigned long long  sync_ntp;
	unsigned int        sync_rtp;

	/* NTP */
	struct raop_ntp_s   ntp;
	unsigned long long  ntp_transmit;
	long long           ntp_offset;
	unsigned long long  ntp_dispersion;
};

static int
raop_rtp_parse_remote(raop_rtp_t *raop_rtp, const char *remote)
{
	char *original;
	char *current;
	char *tmpstr;
	int family;
	int ret;

	assert(raop_rtp);

	current = original = strdup(remote);
	if (!original) {
		return -1;
	}
	tmpstr = utils_strsep(&current, " ");
	if (strcmp(tmpstr, "IN")) {
		free(original);
		return -1;
	}
	tmpstr = utils_strsep(&current, " ");
	if (!strcmp(tmpstr, "IP4") && current) {
		family = AF_INET;
	} else if (!strcmp(tmpstr, "IP6") && current) {
		family = AF_INET6;
	} else {
		free(original);
		return -1;
	}
	if (strstr(current, ":")) {
		/* FIXME: iTunes sends IP4 even with an IPv6 address, does it mean something */
		family = AF_INET6;
	}
	ret = netutils_parse_address(family, current,
	                             &raop_rtp->remote_saddr,
	                             sizeof(raop_rtp->remote_saddr));
	if (ret < 0) {
		free(original);
		return -1;
	}
	raop_rtp->remote_saddr_len = ret;
	free(original);
	return 0;
}

static int
raop_rtp_get_clock_internal(raop_rtp_t *raop_rtp, unsigned long long* clock)
{
	if (raop_rtp->callbacks.audio_get_clock) {
		raop_rtp->callbacks.audio_get_clock(raop_rtp->callbacks.cls, clock);
		*clock += RAOP_NTP_CLOCK_BASE;
		return 0;
	} else {
		return -1;
	}
}

raop_rtp_t *
raop_rtp_init(logger_t *logger, raop_decoder_t *decoder,
              raop_callbacks_t *callbacks, const char *remote)
{
	raop_rtp_t *raop_rtp;

	assert(logger);
	assert(decoder);
	assert(callbacks);
	assert(remote);

	raop_rtp = calloc(1, sizeof(raop_rtp_t));
	if (!raop_rtp) {
		return NULL;
	}
	raop_rtp->logger = logger;
	raop_rtp->decoder = decoder;
	memcpy(&raop_rtp->callbacks, callbacks, sizeof(raop_callbacks_t));

	raop_rtp->buffer = raop_buffer_init(raop_rtp->logger, raop_rtp->decoder);
	if (!raop_rtp->buffer) {
		free(raop_rtp);
		return NULL;
	}

	raop_rtp_get_clock_internal(raop_rtp, &raop_rtp->ntp_transmit);
	raop_ntp_init(&raop_rtp->ntp, logger, raop_rtp->ntp_transmit);
	raop_rtp->ntp_dispersion = RAOP_NTP_MAXDISP;

	if (raop_rtp_parse_remote(raop_rtp, remote) < 0) {
		raop_buffer_destroy(raop_rtp->buffer);
		free(raop_rtp);
		return NULL;
	}

	raop_rtp->running = 0;
	raop_rtp->joined = 1;
	raop_rtp->flush = NO_FLUSH;
	MUTEX_CREATE(raop_rtp->run_mutex);

	return raop_rtp;
}

void
raop_rtp_destroy(raop_rtp_t *raop_rtp)
{
	if (raop_rtp) {
		raop_rtp_stop(raop_rtp);

		MUTEX_DESTROY(raop_rtp->run_mutex);
		raop_buffer_destroy(raop_rtp->buffer);
		free(raop_rtp->metadata);
		free(raop_rtp->coverart);
		free(raop_rtp->dacp_id);
		free(raop_rtp->active_remote_header);
		free(raop_rtp);
	}
}

static int
raop_rtp_init_sockets(raop_rtp_t *raop_rtp, int use_ipv6, int use_udp)
{
	int csock = -1, tsock = -1, dsock = -1;
	unsigned short cport = 0, tport = 0, dport = 0;

	assert(raop_rtp);

	if (use_udp) {
		csock = netutils_init_socket(&cport, use_ipv6, use_udp);
		tsock = netutils_init_socket(&tport, use_ipv6, use_udp);
		if (csock == -1 || tsock == -1) {
			goto sockets_cleanup;
		}
	}
	dsock = netutils_init_socket(&dport, use_ipv6, use_udp);
	if (dsock == -1) {
		goto sockets_cleanup;
	}

	/* Listen to the data socket if using TCP */
	if (!use_udp) {
		if (listen(dsock, 1) < 0)
			goto sockets_cleanup;
	}

	/* Set socket descriptors */
	raop_rtp->csock = csock;
	raop_rtp->tsock = tsock;
	raop_rtp->dsock = dsock;

	/* Set port values */
	raop_rtp->control_lport = cport;
	raop_rtp->timing_lport = tport;
	raop_rtp->data_lport = dport;
	return 0;

sockets_cleanup:
	if (csock != -1) closesocket(csock);
	if (tsock != -1) closesocket(tsock);
	if (dsock != -1) closesocket(dsock);
	return -1;
}

static int
raop_rtp_resend_callback(void *opaque, unsigned short seqnum, unsigned short count)
{
	raop_rtp_t *raop_rtp = opaque;
	unsigned char packet[8];
	unsigned short ourseqnum;
	struct sockaddr *addr;
	socklen_t addrlen;
	int ret;

	addr = (struct sockaddr *)&raop_rtp->control_saddr;
	addrlen = raop_rtp->control_saddr_len;

	logger_log(raop_rtp->logger, LOGGER_DEBUG, "Sending resend request %d %d", seqnum, count);
	ourseqnum = raop_rtp->control_seqnum++;

	/* Fill the request buffer */
	packet[0] = 0x80;
	packet[1] = 0x55|0x80;
	bio_set_be_u16(&packet[2], ourseqnum);
	bio_set_be_u16(&packet[4], seqnum);
	bio_set_be_u16(&packet[6], count);

	ret = sendto(raop_rtp->csock, (const char *)packet, sizeof(packet), 0, addr, addrlen);
	if (ret == -1) {
		logger_log(raop_rtp->logger, LOGGER_WARNING, "Resend failed: %d", SOCKET_GET_ERROR());
	}

	return 0;
}

static int
raop_rtp_request_ntp(raop_rtp_t *raop_rtp)
{
	unsigned char packet[32];
	unsigned short ourseqnum;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int ret;

	addrlen = raop_rtp->control_saddr_len;
	memcpy(&addr, &raop_rtp->control_saddr, addrlen);
	if(addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
		sin6->sin6_port = htons(raop_rtp->timing_rport);
	} else if(addr.ss_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
		sin->sin_port = htons(raop_rtp->timing_rport);
	}

	ret = raop_rtp_get_clock_internal(raop_rtp, &raop_rtp->ntp_transmit);
	if (ret < 0) {
		return ret;
	}

	/* Fill the request buffer */
	packet[0] = 0x80;
	packet[1] = 0x52|0x80;
	bio_set_be_u16(&packet[2] , 7u);                 /* sequence hard coded to 7 */
	bio_set_be_u32(&packet[4] , 0u);                 /* timestamp */
	bio_set_be_u64(&packet[8] , 0u);                 /* origin */
	bio_set_be_u64(&packet[16], 0u);                 /* receive */
	bio_set_be_u64(&packet[24], raop_rtp->ntp_transmit); /* transmit */

	ret = sendto(raop_rtp->tsock, (const char *)packet, sizeof(packet), 0, (struct sockaddr*)&addr, addrlen);
	if (ret == -1) {
		logger_log(raop_rtp->logger, LOGGER_WARNING, "Timesend failed: %d", SOCKET_GET_ERROR());
	} else {
		logger_log(raop_rtp->logger, LOGGER_DEBUG, "Timesend request %llu", raop_rtp->ntp_transmit);
	}

	return 0;
}

static int
raop_rtp_process_events(raop_rtp_t *raop_rtp, void *cb_data)
{
	int flush;
	float volume;
	int volume_changed;
	unsigned char *metadata;
	int metadata_len;
	unsigned char *coverart;
	int coverart_len;
	char *dacp_id;
	char *active_remote_header;
	unsigned int progress_start;
	unsigned int progress_curr;
	unsigned int progress_end;
	int progress_changed;

	assert(raop_rtp);

	MUTEX_LOCK(raop_rtp->run_mutex);
	if (!raop_rtp->running) {
		MUTEX_UNLOCK(raop_rtp->run_mutex);
		return 1;
	}

	/* Read the volume level */
	volume = raop_rtp->volume;
	volume_changed = raop_rtp->volume_changed;
	raop_rtp->volume_changed = 0;

	/* Read the flush value */
	flush = raop_rtp->flush;
	raop_rtp->flush = NO_FLUSH;

	/* Read the metadata */
	metadata = raop_rtp->metadata;
	metadata_len = raop_rtp->metadata_len;
	raop_rtp->metadata = NULL;
	raop_rtp->metadata_len = 0;

	/* Read the coverart */
	coverart = raop_rtp->coverart;
	coverart_len = raop_rtp->coverart_len;
	raop_rtp->coverart = NULL;
	raop_rtp->coverart_len = 0;
	
	/* Read DACP remote control data */
	dacp_id = raop_rtp->dacp_id;
	active_remote_header = raop_rtp->active_remote_header;
	raop_rtp->dacp_id = NULL;
	raop_rtp->active_remote_header = NULL;

	/* Read the progress values */
	progress_start = raop_rtp->progress_start;
	progress_curr = raop_rtp->progress_curr;
	progress_end = raop_rtp->progress_end;
	progress_changed = raop_rtp->progress_changed;
	raop_rtp->progress_changed = 0;

	MUTEX_UNLOCK(raop_rtp->run_mutex);

	/* Call set_volume callback if changed */
	if (volume_changed) {
		if (raop_rtp->callbacks.audio_set_volume) {
			raop_rtp->callbacks.audio_set_volume(raop_rtp->callbacks.cls, cb_data, volume);
		}
	}

	/* Handle flush if requested */
	if (flush != NO_FLUSH) {
		raop_buffer_flush(raop_rtp->buffer, flush);
		if (raop_rtp->callbacks.audio_flush) {
			raop_rtp->callbacks.audio_flush(raop_rtp->callbacks.cls, cb_data);
		}
	}

	if (metadata != NULL) {
		if (raop_rtp->callbacks.audio_set_metadata) {
			raop_rtp->callbacks.audio_set_metadata(raop_rtp->callbacks.cls, cb_data, metadata, metadata_len);
		}
		free(metadata);
		metadata = NULL;
	}

	if (coverart != NULL) {
		if (raop_rtp->callbacks.audio_set_coverart) {
			raop_rtp->callbacks.audio_set_coverart(raop_rtp->callbacks.cls, cb_data, coverart, coverart_len);
		}
		free(coverart);
		coverart = NULL;
	}
	if (dacp_id && active_remote_header) {
		if (raop_rtp->callbacks.audio_remote_control_id) {
			raop_rtp->callbacks.audio_remote_control_id(raop_rtp->callbacks.cls, dacp_id, active_remote_header);
		}
		free(dacp_id);
		free(active_remote_header);
		dacp_id = NULL;
		active_remote_header = NULL;
	}

	if (progress_changed) {
		if (raop_rtp->callbacks.audio_set_progress) {
			raop_rtp->callbacks.audio_set_progress(raop_rtp->callbacks.cls, cb_data, progress_start, progress_curr, progress_end);
		}
	}
	return 0;
}

static THREAD_RETVAL
raop_rtp_thread_udp(void *arg)
{
	raop_rtp_t *raop_rtp = arg;
	unsigned int sample_rate;
	unsigned char bit_depth;
	unsigned char channels;
	unsigned char packet[RAOP_PACKET_LEN];
	unsigned int packetlen;
	struct sockaddr_storage saddr;
	socklen_t saddrlen;

	void *cb_data = NULL;

	assert(raop_rtp);

	sample_rate = raop_decoder_get_sample_rate(raop_rtp->decoder);
	bit_depth = raop_decoder_get_bit_depth(raop_rtp->decoder);
	channels = raop_decoder_get_channels(raop_rtp->decoder);
	cb_data = raop_rtp->callbacks.audio_init(raop_rtp->callbacks.cls,
	                                         bit_depth, channels, sample_rate);

	while (1) {
		fd_set rfds;
		struct timeval tv;
		int nfds, ret;
		unsigned long long clock;

		/* Check if we are still running and process callbacks */
		if (raop_rtp_process_events(raop_rtp, cb_data)) {
			break;
		}

		/* Request time every 3 seconds */
		if (raop_rtp_get_clock_internal(raop_rtp, &clock) == 0) {
			if ((clock - raop_rtp->ntp_transmit) > (3ull << 32)) {
				raop_rtp_request_ntp(raop_rtp);
			}
		}


		/* Set timeout value to 5ms */
		tv.tv_sec = 0;
		tv.tv_usec = 5000;

		/* Get the correct nfds value */
		nfds = raop_rtp->csock+1;
		if (raop_rtp->tsock >= nfds)
			nfds = raop_rtp->tsock+1;
		if (raop_rtp->dsock >= nfds)
			nfds = raop_rtp->dsock+1;

		/* Set rfds and call select */
		FD_ZERO(&rfds);
		FD_SET(raop_rtp->csock, &rfds);
		FD_SET(raop_rtp->tsock, &rfds);
		FD_SET(raop_rtp->dsock, &rfds);
		ret = select(nfds, &rfds, NULL, NULL, &tv);
		if (ret == 0) {
			/* Timeout happened */
			continue;
		} else if (ret == -1) {
			/* FIXME: Error happened */
			break;
		}

		if (FD_ISSET(raop_rtp->csock, &rfds)) {
			saddrlen = sizeof(saddr);
			packetlen = recvfrom(raop_rtp->csock, (char *)packet, sizeof(packet), 0,
			                     (struct sockaddr *)&saddr, &saddrlen);

			/* Get the destination address here, because we need the sin6_scope_id */
			memcpy(&raop_rtp->control_saddr, &saddr, saddrlen);
			raop_rtp->control_saddr_len = saddrlen;

			if (packetlen >= 12) {
				char type = packet[1] & ~0x80;

				logger_log(raop_rtp->logger, LOGGER_DEBUG, "Got control packet of type 0x%02x", type);
				if (type == 0x56) {
					/* Handle resent data packet */
					int ret = raop_buffer_queue(raop_rtp->buffer, packet+4, packetlen-4, 1);
					assert(ret >= 0);
				} else if (type == 0x54 && packetlen >= 20) {
					unsigned int       now_rtp;
					raop_rtp->sync_rtp = bio_get_be_u32(&packet[4]) - 11025;
					raop_rtp->sync_ntp = bio_get_be_u64(&packet[8]);
					now_rtp            = bio_get_be_u32(&packet[16]);
					logger_log(raop_rtp->logger, LOGGER_DEBUG, "Got time sync packet 0x%08x, 0x%08x, %u, %u", (unsigned int)raop_rtp->sync_ntp, (unsigned int)(raop_rtp->sync_ntp >> 32), raop_rtp->sync_rtp, now_rtp - raop_rtp->sync_rtp);
					if (raop_rtp->callbacks.audio_sync) {
						unsigned long long clock;
						if (raop_rtp->ntp_dispersion < RAOP_NTP_MAXDIST) {
							raop_rtp->callbacks.audio_sync(raop_rtp->callbacks.cls,
							                               cb_data,
							                               raop_rtp->sync_ntp - raop_rtp->ntp_offset - RAOP_NTP_CLOCK_BASE,
							                               raop_rtp->ntp_dispersion,
							                               raop_rtp->sync_rtp,
							                               now_rtp - raop_rtp->sync_rtp);
						} else if (raop_rtp_get_clock_internal(raop_rtp, &clock) == 0) {
							raop_rtp->callbacks.audio_sync(raop_rtp->callbacks.cls,
							                               cb_data,
							                               clock  - RAOP_NTP_CLOCK_BASE,
							                               RAOP_NTP_MAXDIST*RAOP_NTP_COUNT,
							                               raop_rtp->sync_rtp,
							                               now_rtp - raop_rtp->sync_rtp);
						}
					}
				}
			}
		} else if (FD_ISSET(raop_rtp->tsock, &rfds)) {
			logger_log(raop_rtp->logger, LOGGER_INFO, "Would have timing packet in queue");
			saddrlen = sizeof(saddr);
			packetlen = recvfrom(raop_rtp->tsock, (char *)packet, sizeof(packet), 0,
			                     (struct sockaddr *)&saddr, &saddrlen);
			if (packetlen >= 12) {
				char type = packet[1] & ~0x80;

				logger_log(raop_rtp->logger, LOGGER_DEBUG, "Got time packet of type 0x%02x", type);
				if (type == 0x53 && packetlen >= 32) {
					unsigned long long origin   = bio_get_be_u64(&packet[8]);
					unsigned long long receive  = bio_get_be_u64(&packet[16]);
					unsigned long long transmit = bio_get_be_u64(&packet[24]);
					unsigned long long current  = 0u;

					if (raop_rtp_get_clock_internal(raop_rtp, &current) == 0) {
						raop_ntp_add(&raop_rtp->ntp, origin, receive, transmit, current);
						raop_ntp_get_offset(&raop_rtp->ntp, current, &raop_rtp->ntp_offset, &raop_rtp->ntp_dispersion);
					}
				}
			}
		} else if (FD_ISSET(raop_rtp->dsock, &rfds)) {
			saddrlen = sizeof(saddr);
			packetlen = recvfrom(raop_rtp->dsock, (char *)packet, sizeof(packet), 0,
			                     (struct sockaddr *)&saddr, &saddrlen);
			if (packetlen >= 12) {
				int no_resend = (raop_rtp->control_rport == 0);
				int ret;

				const void *audiobuf;
				int audiobuflen;
				unsigned int timestamp;

				ret = raop_buffer_queue(raop_rtp->buffer, packet, packetlen, 1);
				assert(ret >= 0);

				/* Decode all frames in queue */
				while ((audiobuf = raop_buffer_dequeue(raop_rtp->buffer, &audiobuflen, &timestamp, no_resend))) {
					raop_rtp->callbacks.audio_process(raop_rtp->callbacks.cls, cb_data, audiobuf, audiobuflen, timestamp);
				}

				/* Handle possible resend requests */
				if (!no_resend) {
					raop_buffer_handle_resends(raop_rtp->buffer, raop_rtp_resend_callback, raop_rtp);
				}
			}
		}
	}
	logger_log(raop_rtp->logger, LOGGER_INFO, "Exiting UDP RAOP thread");
	raop_rtp->callbacks.audio_destroy(raop_rtp->callbacks.cls, cb_data);

	return 0;
}

static THREAD_RETVAL
raop_rtp_thread_tcp(void *arg)
{
	raop_rtp_t *raop_rtp = arg;
	int stream_fd = -1;
	unsigned int sample_rate;
	unsigned char bit_depth;
	unsigned char channels;
	unsigned char packet[RAOP_PACKET_LEN];
	unsigned int packetlen = 0;

	void *cb_data = NULL;

	assert(raop_rtp);

	sample_rate = raop_decoder_get_sample_rate(raop_rtp->decoder);
	bit_depth = raop_decoder_get_bit_depth(raop_rtp->decoder);
	channels = raop_decoder_get_channels(raop_rtp->decoder);
	cb_data = raop_rtp->callbacks.audio_init(raop_rtp->callbacks.cls,
	                                         bit_depth, channels, sample_rate);

	while (1) {
		fd_set rfds;
		struct timeval tv;
		int nfds, ret;

		/* Check if we are still running and process callbacks */
		if (raop_rtp_process_events(raop_rtp, cb_data)) {
			break;
		}

		/* Set timeout value to 5ms */
		tv.tv_sec = 0;
		tv.tv_usec = 5000;

		/* Get the correct nfds value and set rfds */
		FD_ZERO(&rfds);
		if (stream_fd == -1) {
			FD_SET(raop_rtp->dsock, &rfds);
			nfds = raop_rtp->dsock+1;
		} else {
			FD_SET(stream_fd, &rfds);
			nfds = stream_fd+1;
		}
		ret = select(nfds, &rfds, NULL, NULL, &tv);
		if (ret == 0) {
			/* Timeout happened */
			continue;
		} else if (ret == -1) {
			/* FIXME: Error happened */
			logger_log(raop_rtp->logger, LOGGER_INFO, "Error in select");
			break;
		}
		if (stream_fd == -1 && FD_ISSET(raop_rtp->dsock, &rfds)) {
			struct sockaddr_storage saddr;
			socklen_t saddrlen;

			logger_log(raop_rtp->logger, LOGGER_INFO, "Accepting client");
			saddrlen = sizeof(saddr);
			stream_fd = accept(raop_rtp->dsock, (struct sockaddr *)&saddr, &saddrlen);
			if (stream_fd == -1) {
				/* FIXME: Error happened */
				logger_log(raop_rtp->logger, LOGGER_INFO, "Error in accept %d %s", errno, strerror(errno));
				break;
			}
		}
		if (stream_fd != -1 && FD_ISSET(stream_fd, &rfds)) {
			unsigned int rtplen=0;

			const void *audiobuf;
			int audiobuflen;
			unsigned int timestamp;

			ret = recv(stream_fd, (char *)(packet+packetlen), sizeof(packet)-packetlen, 0);
			if (ret == 0) {
				/* TCP socket closed */
				logger_log(raop_rtp->logger, LOGGER_INFO, "TCP socket closed");
				break;
			} else if (ret == -1) {
				/* FIXME: Error happened */
				logger_log(raop_rtp->logger, LOGGER_INFO, "Error in recv");
				break;
			}
			packetlen += ret;

			/* Check that we have enough bytes */
			if (packetlen < 4) {
				continue;
			}
			if (packet[0] != '$' || packet[1] != '\0') {
				/* FIXME: Incorrect RTP magic bytes */
				break;
			}
			rtplen = (packet[2] << 8) | packet[3];
			if (rtplen > sizeof(packet)) {
				/* FIXME: Too long packet */
				logger_log(raop_rtp->logger, LOGGER_INFO, "Error, packet too long %d", rtplen);
				break;
			}
			if (packetlen < 4+rtplen) {
				continue;
			}

			/* Packet is valid, process it */
			ret = raop_buffer_queue(raop_rtp->buffer, packet+4, rtplen, 0);
			assert(ret >= 0);

			/* Remove processed bytes from packet buffer */
			memmove(packet, packet+4+rtplen, packetlen-rtplen);
			packetlen -= 4+rtplen;

			/* Decode the received frame */
			if ((audiobuf = raop_buffer_dequeue(raop_rtp->buffer, &audiobuflen, &timestamp, 1))) {
				raop_rtp->callbacks.audio_process(raop_rtp->callbacks.cls, cb_data, audiobuf, audiobuflen, timestamp);
			}
		}
	}

	/* Close the stream file descriptor */
	if (stream_fd != -1) {
		closesocket(stream_fd);
	}

	logger_log(raop_rtp->logger, LOGGER_INFO, "Exiting TCP RAOP thread");
	raop_rtp->callbacks.audio_destroy(raop_rtp->callbacks.cls, cb_data);

	return 0;
}

void
raop_rtp_start(raop_rtp_t *raop_rtp, int use_udp, unsigned short control_rport, unsigned short timing_rport,
               unsigned short *control_lport, unsigned short *timing_lport, unsigned short *data_lport)
{
	int use_ipv6 = 0;

	assert(raop_rtp);

	MUTEX_LOCK(raop_rtp->run_mutex);
	if (raop_rtp->running || !raop_rtp->joined) {
		MUTEX_UNLOCK(raop_rtp->run_mutex);
		return;
	}

	/* Initialize ports and sockets */
	raop_rtp->control_rport = control_rport;
	raop_rtp->timing_rport = timing_rport;
	if (raop_rtp->remote_saddr.ss_family == AF_INET6) {
		use_ipv6 = 1;
	}
	if (raop_rtp_init_sockets(raop_rtp, use_ipv6, use_udp) < 0) {
		logger_log(raop_rtp->logger, LOGGER_INFO, "Initializing sockets failed");
		MUTEX_UNLOCK(raop_rtp->run_mutex);
		return;
	}
	if (control_lport) *control_lport = raop_rtp->control_lport;
	if (timing_lport) *timing_lport = raop_rtp->timing_lport;
	if (data_lport) *data_lport = raop_rtp->data_lport;
	
	/* Create the thread and initialize running values */
	raop_rtp->running = 1;
	raop_rtp->joined = 0;
	if (use_udp) {
		THREAD_CREATE(raop_rtp->thread, raop_rtp_thread_udp, raop_rtp);
	} else {
		THREAD_CREATE(raop_rtp->thread, raop_rtp_thread_tcp, raop_rtp);
	}
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

void
raop_rtp_set_volume(raop_rtp_t *raop_rtp, float volume)
{
	assert(raop_rtp);

	if (volume > 0.0f) {
		volume = 0.0f;
	} else if (volume < -144.0f) {
		volume = -144.0f;
	}

	/* Set volume in thread instead */
	MUTEX_LOCK(raop_rtp->run_mutex);
	raop_rtp->volume = volume;
	raop_rtp->volume_changed = 1;
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

void
raop_rtp_set_metadata(raop_rtp_t *raop_rtp, const char *data, int datalen)
{
	unsigned char *metadata;

	assert(raop_rtp);

	if (datalen <= 0) {
		return;
	}
	metadata = malloc(datalen);
	assert(metadata);
	memcpy(metadata, data, datalen);

	/* Set metadata in thread instead */
	MUTEX_LOCK(raop_rtp->run_mutex);
	raop_rtp->metadata = metadata;
	raop_rtp->metadata_len = datalen;
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

void
raop_rtp_set_coverart(raop_rtp_t *raop_rtp, const char *data, int datalen)
{
	unsigned char *coverart;

	assert(raop_rtp);

	if (datalen <= 0) {
		return;
	}
	coverart = malloc(datalen);
	assert(coverart);
	memcpy(coverart, data, datalen);

	/* Set coverart in thread instead */
	MUTEX_LOCK(raop_rtp->run_mutex);
	raop_rtp->coverart = coverart;
	raop_rtp->coverart_len = datalen;
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

void 
raop_rtp_remote_control_id(raop_rtp_t *raop_rtp, const char *dacp_id, const char *active_remote_header)
{
	assert(raop_rtp);

	if (!dacp_id || !active_remote_header) {
		return;
	}

	/* Set dacp stuff in thread instead */
	MUTEX_LOCK(raop_rtp->run_mutex);
	raop_rtp->dacp_id = strdup(dacp_id);
	raop_rtp->active_remote_header = strdup(active_remote_header);
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

void
raop_rtp_set_progress(raop_rtp_t *raop_rtp, unsigned int start, unsigned int curr, unsigned int end)
{
	assert(raop_rtp);

	/* Set progress in thread instead */
	MUTEX_LOCK(raop_rtp->run_mutex);
	raop_rtp->progress_start = start;
	raop_rtp->progress_curr = curr;
	raop_rtp->progress_end = end;
	raop_rtp->progress_changed = 1;
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

void
raop_rtp_flush(raop_rtp_t *raop_rtp, int next_seq)
{
	assert(raop_rtp);

	/* Call flush in thread instead */
	MUTEX_LOCK(raop_rtp->run_mutex);
	raop_rtp->flush = next_seq;
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

void
raop_rtp_stop(raop_rtp_t *raop_rtp)
{
	assert(raop_rtp);

	/* Check that we are running and thread is not
	 * joined (should never be while still running) */
	MUTEX_LOCK(raop_rtp->run_mutex);
	if (!raop_rtp->running || raop_rtp->joined) {
		MUTEX_UNLOCK(raop_rtp->run_mutex);
		return;
	}
	raop_rtp->running = 0;
	MUTEX_UNLOCK(raop_rtp->run_mutex);

	/* Join the thread */
	THREAD_JOIN(raop_rtp->thread);
	if (raop_rtp->csock != -1) closesocket(raop_rtp->csock);
	if (raop_rtp->tsock != -1) closesocket(raop_rtp->tsock);
	if (raop_rtp->dsock != -1) closesocket(raop_rtp->dsock);

	/* Flush buffer into initial state */
	raop_buffer_flush(raop_rtp->buffer, -1);

	/* Mark thread as joined */
	MUTEX_LOCK(raop_rtp->run_mutex);
	raop_rtp->joined = 1;
	MUTEX_UNLOCK(raop_rtp->run_mutex);
}

