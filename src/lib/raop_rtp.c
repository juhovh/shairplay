#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "raop_rtp.h"
#include "raop.h"
#include "raop_buffer.h"
#include "netutils.h"
#include "compat.h"
#include "logger.h"

#define NO_FLUSH (-42)

struct raop_rtp_s {
	logger_t *logger;
	raop_callbacks_t callbacks;

	raop_buffer_t *buffer;

	/* These variables only edited mutex locked */
	int running;
	int joined;
	float volume;
	int flush;
	thread_handle_t thread;
	mutex_handle_t run_mutex;

	/* Remote control and timing ports */
	unsigned short control_rport;
	unsigned short timing_rport;

	/* Sockets for control, timing and data */
	int csock, tsock, dsock;

	/* Local control, timing and data ports */
	unsigned short control_lport;
	unsigned short timing_lport;
	unsigned short data_lport;

	struct sockaddr_storage control_saddr;
	socklen_t control_saddr_len;
	unsigned short control_seqnum;
};

raop_rtp_t *
raop_rtp_init(logger_t *logger, raop_callbacks_t *callbacks, const char *fmtp,
              const unsigned char *aeskey, const unsigned char *aesiv)
{
	raop_rtp_t *raop_rtp;

	assert(logger);

	raop_rtp = calloc(1, sizeof(raop_rtp_t));
	if (!raop_rtp) {
		return NULL;
	}
	raop_rtp->logger = logger;
	memcpy(&raop_rtp->callbacks, callbacks, sizeof(raop_callbacks_t));
	raop_rtp->buffer = raop_buffer_init(fmtp, aeskey, aesiv);
	if (!raop_rtp->buffer) {
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

	addr = (struct sockaddr *)&raop_rtp->control_saddr;
	addrlen = raop_rtp->control_saddr_len;

	logger_log(raop_rtp->logger, LOGGER_DEBUG, "Got resend request %d %d\n", seqnum, count);
	ourseqnum = raop_rtp->control_seqnum++;

	/* Fill the request buffer */
	packet[0] = 0x80;
	packet[1] = 0x55|0x80;
	packet[2] = (ourseqnum >> 8);
	packet[3] =  ourseqnum;
	packet[4] = (seqnum >> 8);
	packet[5] =  seqnum;
	packet[6] = (count >> 8);
	packet[7] =  count;

	sendto(raop_rtp->csock, (const char *)packet, sizeof(packet), 0, addr, addrlen);
	return 0;
}

static THREAD_RETVAL
raop_rtp_thread_udp(void *arg)
{
	raop_rtp_t *raop_rtp = arg;
	unsigned char packet[RAOP_PACKET_LEN];
	unsigned int packetlen;
	struct sockaddr_storage saddr;
	socklen_t saddrlen;

	const ALACSpecificConfig *config;
	void *cb_data = NULL;

	assert(raop_rtp);

	config = raop_buffer_get_config(raop_rtp->buffer);
	raop_rtp->callbacks.audio_init(raop_rtp->callbacks.cls, &cb_data,
	                               config->bitDepth,
	                               config->numChannels,
	                               config->sampleRate);

	while(1) {
		int volume_changed;
		float volume = 0.0;
		int flush;

		fd_set rfds;
		struct timeval tv;
		int nfds, ret;

		MUTEX_LOCK(raop_rtp->run_mutex);
		if (!raop_rtp->running) {
			MUTEX_UNLOCK(raop_rtp->run_mutex);
			break;
		}
		/* Read the volume level */
		volume_changed = (volume != raop_rtp->volume);
		volume = raop_rtp->volume;

		/* Read the flush value */
		flush = raop_rtp->flush;
		raop_rtp->flush = NO_FLUSH;
		MUTEX_UNLOCK(raop_rtp->run_mutex);

		/* Call set_volume callback if changed */
		if (volume_changed) {
			raop_rtp->callbacks.audio_set_volume(raop_rtp->callbacks.cls, cb_data, volume);
		}
		if (flush != NO_FLUSH) {
			raop_buffer_flush(raop_rtp->buffer, flush);
			raop_rtp->callbacks.audio_flush(raop_rtp->callbacks.cls, cb_data);
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

			/* FIXME: Get destination address here */
			memcpy(&raop_rtp->control_saddr, &saddr, saddrlen);
			raop_rtp->control_saddr_len = saddrlen;

			if (packetlen >= 12) {
				char type = packet[1] & ~0x80;

				logger_log(raop_rtp->logger, LOGGER_DEBUG, "Got control packet of type 0x%02x\n", type);
				if (type == 0x56) {
					/* Handle resent data packet */
					int ret = raop_buffer_queue(raop_rtp->buffer, packet+4, packetlen-4, 1);
					assert(ret >= 0);
				}
			}
		} else if (FD_ISSET(raop_rtp->tsock, &rfds)) {
			logger_log(raop_rtp->logger, LOGGER_INFO, "Would have timing packet in queue\n");
		} else if (FD_ISSET(raop_rtp->dsock, &rfds)) {
			saddrlen = sizeof(saddr);
			packetlen = recvfrom(raop_rtp->dsock, (char *)packet, sizeof(packet), 0,
			                     (struct sockaddr *)&saddr, &saddrlen);
			if (packetlen >= 12) {
				int no_resend = (raop_rtp->control_rport == 0);
				int ret;

				const void *audiobuf;
				int audiobuflen;

				ret = raop_buffer_queue(raop_rtp->buffer, packet, packetlen, 1);
				assert(ret >= 0);

				/* Decode all frames in queue */
				while ((audiobuf = raop_buffer_dequeue(raop_rtp->buffer, &audiobuflen, no_resend))) {
					raop_rtp->callbacks.audio_process(raop_rtp->callbacks.cls, cb_data, audiobuf, audiobuflen);
				}

				/* Handle possible resend requests */
				if (!no_resend) {
					raop_buffer_handle_resends(raop_rtp->buffer, raop_rtp_resend_callback, raop_rtp);
				}
			}
		}
	}
	logger_log(raop_rtp->logger, LOGGER_INFO, "Exiting thread\n");
	raop_rtp->callbacks.audio_destroy(raop_rtp->callbacks.cls, cb_data);

	return 0;
}

static THREAD_RETVAL
raop_rtp_thread_tcp(void *arg)
{
	raop_rtp_t *raop_rtp = arg;
	int stream_fd = -1;
	unsigned char packet[RAOP_PACKET_LEN];
	unsigned int packetlen = 0;

	const ALACSpecificConfig *config;
	void *cb_data = NULL;

	assert(raop_rtp);

	config = raop_buffer_get_config(raop_rtp->buffer);
	raop_rtp->callbacks.audio_init(raop_rtp->callbacks.cls, &cb_data,
	                               config->bitDepth,
	                               config->numChannels,
	                               config->sampleRate);

	while (1) {
		int volume_changed;
		float volume = 0.0;

		fd_set rfds;
		struct timeval tv;
		int nfds, ret;

		MUTEX_LOCK(raop_rtp->run_mutex);
		if (!raop_rtp->running) {
			MUTEX_UNLOCK(raop_rtp->run_mutex);
			break;
		}
		volume_changed = (volume != raop_rtp->volume);
		volume = raop_rtp->volume;
		MUTEX_UNLOCK(raop_rtp->run_mutex);

		/* Call set_volume callback if changed */
		if (volume_changed) {
			raop_rtp->callbacks.audio_set_volume(raop_rtp->callbacks.cls, cb_data, volume);
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
			logger_log(raop_rtp->logger, LOGGER_INFO, "Error in select\n");
			break;
		}
		if (stream_fd == -1 && FD_ISSET(raop_rtp->dsock, &rfds)) {
			struct sockaddr_storage saddr;
			socklen_t saddrlen;

			logger_log(raop_rtp->logger, LOGGER_INFO, "Accepting client\n");
			saddrlen = sizeof(saddr);
			stream_fd = accept(raop_rtp->dsock, (struct sockaddr *)&saddr, &saddrlen);
			if (stream_fd == -1) {
				/* FIXME: Error happened */
				logger_log(raop_rtp->logger, LOGGER_INFO, "Error in accept %d %s\n", errno, strerror(errno));
				break;
			}
		}
		if (stream_fd != -1 && FD_ISSET(stream_fd, &rfds)) {
			unsigned int rtplen=0;
			char type;

			const void *audiobuf;
			int audiobuflen;

			ret = recv(stream_fd, (char *)(packet+packetlen), sizeof(packet)-packetlen, 0);
			if (ret == 0) {
				/* TCP socket closed */
				logger_log(raop_rtp->logger, LOGGER_INFO, "TCP socket closed\n");
				break;
			} else if (ret == -1) {
				/* FIXME: Error happened */
				logger_log(raop_rtp->logger, LOGGER_INFO, "Error in recv\n");
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
				logger_log(raop_rtp->logger, LOGGER_INFO, "Error, packet too long %d\n", rtplen);
				break;
			}
			if (packetlen < 4+rtplen) {
				continue;
			}

			/* Packet is valid, process it */
			type = packet[4+1] & ~0x80;
			ret = raop_buffer_queue(raop_rtp->buffer, packet+4, rtplen, 0);
			assert(ret >= 0);

			/* Remove processed bytes from packet buffer */
			memmove(packet, packet+4+rtplen, packetlen-rtplen);
			packetlen -= 4+rtplen;

			/* Decode the received frame */
			if ((audiobuf = raop_buffer_dequeue(raop_rtp->buffer, &audiobuflen, 1))) {
				raop_rtp->callbacks.audio_process(raop_rtp->callbacks.cls, cb_data, audiobuf, audiobuflen);
			}
		}
	}

	/* Close the stream file descriptor */
	if (stream_fd != -1) {
		closesocket(stream_fd);
	}

	logger_log(raop_rtp->logger, LOGGER_INFO, "Exiting thread\n");
	raop_rtp->callbacks.audio_destroy(raop_rtp->callbacks.cls, cb_data);

	return 0;
}

void
raop_rtp_start(raop_rtp_t *raop_rtp, int use_udp, unsigned short control_rport, unsigned short timing_rport,
               unsigned short *control_lport, unsigned short *timing_lport, unsigned short *data_lport)
{
	assert(raop_rtp);

	MUTEX_LOCK(raop_rtp->run_mutex);
	if (raop_rtp->running || !raop_rtp->joined) {
		MUTEX_UNLOCK(raop_rtp->run_mutex);
		return;
	}

	/* Initialize ports and sockets */
	raop_rtp->control_rport = control_rport;
	raop_rtp->timing_rport = timing_rport;
	if (raop_rtp_init_sockets(raop_rtp, 1, use_udp) < 0) {
		logger_log(raop_rtp->logger, LOGGER_INFO, "Initializing sockets failed\n");
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
