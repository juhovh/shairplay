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
#include <plist/plist.h>

#include "airplay.h"
#include "raop_rtp.h"
#include "rsakey.h"
#include "digest.h"
#include "httpd.h"
#include "sdp.h"

#include "global.h"
#include "utils.h"
#include "netutils.h"
#include "logger.h"
#include "compat.h"
#include "fpsetup.h"

/* Actually 345 bytes for 2048-bit key */
#define MAX_SIGNATURE_LEN 512

/* Let's just decide on some length */
#define MAX_PASSWORD_LEN 64

/* MD5 as hex fits here */
#define MAX_NONCE_LEN 32

#define MAX_PACKET_LEN 4096

struct airplay_s {
	/* Callbacks for audio */
	airplay_callbacks_t callbacks;

	/* Logger instance */
	logger_t *logger;

	/* HTTP daemon and RSA key */
	httpd_t *httpd;
	rsakey_t *rsakey;

	httpd_t *mirror_server;

	/* Hardware address information */
	unsigned char hwaddr[MAX_HWADDR_LEN];
	int hwaddrlen;

	/* Password information */
	char password[MAX_PASSWORD_LEN+1];
};

struct airplay_conn_s {
	airplay_t *airplay;
	raop_rtp_t *airplay_rtp;

	unsigned char *local;
	int locallen;

	unsigned char *remote;
	int remotelen;

	char nonce[MAX_NONCE_LEN+1];

	/* for mirror stream */
	unsigned char aeskey[16];
	unsigned char iv[16];
	unsigned char buffer[MAX_PACKET_LEN];
	int pos;
};
typedef struct airplay_conn_s airplay_conn_t;

#define RECEIVEBUFFER 1024

#define AIRPLAY_STATUS_OK                  200
#define AIRPLAY_STATUS_SWITCHING_PROTOCOLS 101
#define AIRPLAY_STATUS_NEED_AUTH           401
#define AIRPLAY_STATUS_NOT_FOUND           404
#define AIRPLAY_STATUS_METHOD_NOT_ALLOWED  405
#define AIRPLAY_STATUS_PRECONDITION_FAILED 412
#define AIRPLAY_STATUS_NOT_IMPLEMENTED     501
#define AIRPLAY_STATUS_NO_RESPONSE_NEEDED  1000

#define EVENT_NONE     -1
#define EVENT_PLAYING   0
#define EVENT_PAUSED    1
#define EVENT_LOADING   2
#define EVENT_STOPPED   3
const char *eventStrings[] = {"playing", "paused", "loading", "stopped"};

#define STREAM_INFO  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>width</key>\r\n"\
"<integer>1280</integer>\r\n"\
"<key>height</key>\r\n"\
"<integer>720</integer>\r\n"\
"<key>version</key>\r\n"\
"<string>110.92</string>\r\n"\
"</dict>\r\n"\
"</plist>\r\n"

#define PLAYBACK_INFO  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>duration</key>\r\n"\
"<real>%f</real>\r\n"\
"<key>loadedTimeRanges</key>\r\n"\
"<array>\r\n"\
"\t\t<dict>\r\n"\
"\t\t\t<key>duration</key>\r\n"\
"\t\t\t<real>%f</real>\r\n"\
"\t\t\t<key>start</key>\r\n"\
"\t\t\t<real>0.0</real>\r\n"\
"\t\t</dict>\r\n"\
"</array>\r\n"\
"<key>playbackBufferEmpty</key>\r\n"\
"<true/>\r\n"\
"<key>playbackBufferFull</key>\r\n"\
"<false/>\r\n"\
"<key>playbackLikelyToKeepUp</key>\r\n"\
"<true/>\r\n"\
"<key>position</key>\r\n"\
"<real>%f</real>\r\n"\
"<key>rate</key>\r\n"\
"<real>%d</real>\r\n"\
"<key>readyToPlay</key>\r\n"\
"<true/>\r\n"\
"<key>seekableTimeRanges</key>\r\n"\
"<array>\r\n"\
"\t\t<dict>\r\n"\
"\t\t\t<key>duration</key>\r\n"\
"\t\t\t<real>%f</real>\r\n"\
"\t\t\t<key>start</key>\r\n"\
"\t\t\t<real>0.0</real>\r\n"\
"\t\t</dict>\r\n"\
"</array>\r\n"\
"</dict>\r\n"\
"</plist>\r\n"

#define PLAYBACK_INFO_NOT_READY  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>readyToPlay</key>\r\n"\
"<false/>\r\n"\
"</dict>\r\n"\
"</plist>\r\n"

#define SERVER_INFO  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>deviceid</key>\r\n"\
"<string>%s</string>\r\n"\
"<key>features</key>\r\n"\
"<integer>119</integer>\r\n"\
"<key>model</key>\r\n"\
"<string>Kodi,1</string>\r\n"\
"<key>protovers</key>\r\n"\
"<string>1.0</string>\r\n"\
"<key>srcvers</key>\r\n"\
"<string>"AIRPLAY_SERVER_VERSION_STR"</string>\r\n"\
"</dict>\r\n"\
"</plist>\r\n"

#define EVENT_INFO "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\r\n"\
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n\r\n"\
"<plist version=\"1.0\">\r\n"\
"<dict>\r\n"\
"<key>category</key>\r\n"\
"<string>video</string>\r\n"\
"<key>sessionID</key>\r\n"\
"<integer>%d</integer>\r\n"\
"<key>state</key>\r\n"\
"<string>%s</string>\r\n"\
"</dict>\r\n"\
"</plist>\r\n"\

#define AUTH_REALM "AirPlay"
#define AUTH_REQUIRED "WWW-Authenticate: Digest realm=\""  AUTH_REALM  "\", nonce=\"%s\"\r\n"

static void *
conn_init(void *opaque, unsigned char *local, int locallen, unsigned char *remote, int remotelen)
{
	airplay_conn_t *conn;

	conn = calloc(1, sizeof(airplay_conn_t));
	if (!conn) {
		return NULL;
	}
	conn->airplay = opaque;
	conn->airplay_rtp = NULL;

	if (locallen == 4) {
		logger_log(conn->airplay->logger, LOGGER_INFO,
		           "Local: %d.%d.%d.%d",
		           local[0], local[1], local[2], local[3]);
	} else if (locallen == 16) {
		logger_log(conn->airplay->logger, LOGGER_INFO,
		           "Local: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
		           local[0], local[1], local[2], local[3], local[4], local[5], local[6], local[7],
		           local[8], local[9], local[10], local[11], local[12], local[13], local[14], local[15]);
	}
	if (remotelen == 4) {
		logger_log(conn->airplay->logger, LOGGER_INFO,
		           "Remote: %d.%d.%d.%d",
		           remote[0], remote[1], remote[2], remote[3]);
	} else if (remotelen == 16) {
		logger_log(conn->airplay->logger, LOGGER_INFO,
		           "Remote: %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
		           remote[0], remote[1], remote[2], remote[3], remote[4], remote[5], remote[6], remote[7],
		           remote[8], remote[9], remote[10], remote[11], remote[12], remote[13], remote[14], remote[15]);
	}

	conn->local = malloc(locallen);
	assert(conn->local);
	memcpy(conn->local, local, locallen);

	conn->remote = malloc(remotelen);
	assert(conn->remote);
	memcpy(conn->remote, remote, remotelen);

	conn->locallen = locallen;
	conn->remotelen = remotelen;

	digest_generate_nonce(conn->nonce, sizeof(conn->nonce));
	return conn;
}

static void
conn_request(void *ptr, http_request_t *request, http_response_t **response)
{
	const char realm[] = "airplay";
	airplay_conn_t *conn = ptr;
	airplay_t *airplay = conn->airplay;

	const char *method;
	const char *cseq;
	const char *challenge;
	int require_auth = 0;
	char responseHeader[4096];
	char responseBody[4096];
        int  responseLength = 0;

	const char * uri = http_request_get_url(request);
	method = http_request_get_method(request);

	if (!method) {
		return;
	}

        const char * contentType = http_request_get_header(request, "content-type");
        const char * m_sessionId = http_request_get_header(request, "x-apple-session-id");
  	const char * authorization = http_request_get_header(request, "authorization");
	const char * photoAction = http_request_get_header(request, "x-apple-assetaction");
  	const char * photoCacheId = http_request_get_header(request, "x-apple-assetkey");

	int status = AIRPLAY_STATUS_OK;
	int needAuth = 0;

	logger_log(conn->airplay->logger, LOGGER_INFO,"%s uri=%s\n", method, uri);

	{ 
		const char *data;
		int len;
		data = http_request_get_data(request,&len);
		logger_log(conn->airplay->logger, LOGGER_INFO,"data len %d:%s\n", len, data); 
	}
	/*
	   size_t startQs = uri.find('?');
	   if (startQs != char *::npos)
	   {
	   uri.erase(startQs);
	   }
	 */

	// This is the socket which will be used for reverse HTTP
	// negotiate reverse HTTP via upgrade
	if (strcmp(uri, "/reverse") == 0) 
	{
		status = AIRPLAY_STATUS_SWITCHING_PROTOCOLS;
		sprintf(responseHeader, "Upgrade: PTTH/1.0\r\nConnection: Upgrade\r\n");
	}

	// The rate command is used to play/pause media.
	// A value argument should be supplied which indicates media should be played or paused.
	// 0.000000 => pause
	// 1.000000 => play
	else if (strcmp(uri, "/rate") == 0)
	{
/*
		const char* found = strstr(queryString.c_str(), "value=");
		int rate = found ? (int)(atof(found + strlen("value=")) + 0.5f) : 0;

		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s with rate %i", uri.c_str(), rate);

		if (needAuth && !checkAuthorization(authorization, method, uri))
		{
			status = AIRPLAY_STATUS_NEED_AUTH;
		}
		else if (rate == 0)
		{
			if (g_application.m_pPlayer->IsPlaying() && !g_application.m_pPlayer->IsPaused())
			{
				CApplicationMessenger::Get().MediaPause();
			}
		}
		else
		{
			if (g_application.m_pPlayer->IsPausedPlayback())
			{
				CApplicationMessenger::Get().MediaPause();
			}
		}
*/
	}

	// The volume command is used to change playback volume.
	// A value argument should be supplied which indicates how loud we should get.
	// 0.000000 => silent
	// 1.000000 => loud
	else if (strcmp(uri, "/volume") == 0)
	{
/*
		const char* found = strstr(queryString.c_str(), "volume=");
		float volume = found ? (float)strtod(found + strlen("volume="), NULL) : 0;

		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s with volume %f", uri.c_str(), volume);

		if (needAuth && !checkAuthorization(authorization, method, uri))
		{
			status = AIRPLAY_STATUS_NEED_AUTH;
		}
		else if (volume >= 0 && volume <= 1)
		{
			float oldVolume = g_application.GetVolume();
			volume *= 100;
			if(oldVolume != volume && CSettings::Get().GetBool("services.airplayvolumecontrol"))
			{
				backupVolume();
				g_application.SetVolume(volume);          
				CApplicationMessenger::Get().ShowVolumeBar(oldVolume < volume);
			}
		}
*/
	}


	// Contains a header like format in the request body which should contain a
	// Content-Location and optionally a Start-Position
	else if (strcmp(uri, "/play") == 0)
	{
/*
		char * location;
		float position = 0.0;
		int startPlayback = true;
		m_lastEvent = EVENT_NONE;

		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s", uri.c_str());

		if (needAuth && !checkAuthorization(authorization, method, uri))
		{
			status = AIRPLAY_STATUS_NEED_AUTH;
		}
		else if (contentType == "application/x-apple-binary-plist")
		{
			m_isPlaying++;    

			if (m_pLibPlist->Load())
			{
				m_pLibPlist->EnableDelayedUnload(false);

				const char* bodyChr = m_httpParser->getBody();

				plist_t dict = NULL;
				m_pLibPlist->plist_from_bin(bodyChr, m_httpParser->getContentLength(), &dict);

				if (m_pLibPlist->plist_dict_get_size(dict))
				{
					plist_t tmpNode = m_pLibPlist->plist_dict_get_item(dict, "Start-Position");
					if (tmpNode)
					{
						double tmpDouble = 0;
						m_pLibPlist->plist_get_real_val(tmpNode, &tmpDouble);
						position = (float)tmpDouble;
					}

					tmpNode = m_pLibPlist->plist_dict_get_item(dict, "Content-Location");
					if (tmpNode)
					{
						location = getStringFromPlist(m_pLibPlist, tmpNode);
						tmpNode = NULL;
					}

					tmpNode = m_pLibPlist->plist_dict_get_item(dict, "rate");
					if (tmpNode)
					{
						double rate = 0;
						m_pLibPlist->plist_get_real_val(tmpNode, &rate);
						if (rate == 0.0)
						{
							startPlayback = false;
						}
						tmpNode = NULL;
					}

					// in newer protocol versions the location is given
					// via host and path where host is ip:port and path is /path/file.mov
					if (location.empty())
						tmpNode = m_pLibPlist->plist_dict_get_item(dict, "host");
					if (tmpNode)
					{
						location = "http://";
						location += getStringFromPlist(m_pLibPlist, tmpNode);

						tmpNode = m_pLibPlist->plist_dict_get_item(dict, "path");
						if (tmpNode)
						{
							location += getStringFromPlist(m_pLibPlist, tmpNode);
						}
					}

					if (dict)
					{
						m_pLibPlist->plist_free(dict);
					}
				}
				else
				{
					logger_log(conn->airplay->logger, LOGGER_INFO, "Error parsing plist");
				}
				m_pLibPlist->Unload();
			}
		}
		else
		{
			m_isPlaying++;        
			// Get URL to play
			char * contentLocation = "Content-Location: ";
			size_t start = body.find(contentLocation);
			if (start == char *::npos)
				return AIRPLAY_STATUS_NOT_IMPLEMENTED;
			start += contentLocation.size();
			int end = body.find('\n', start);
			location = body.substr(start, end - start);

			char * startPosition = "Start-Position: ";
			start = body.find(startPosition);
			if (start != char *::npos)
			{
				start += startPosition.size();
				int end = body.find('\n', start);
				char * positionStr = body.substr(start, end - start);
				position = (float)atof(positionStr.c_str());
			}
		}

		if (status != AIRPLAY_STATUS_NEED_AUTH)
		{
			char * userAgent(CURL::Encode("AppleCoreMedia/1.0.0.8F455 (AppleTV; U; CPU OS 4_3 like Mac OS X; de_de)"));
			location += "|User-Agent=" + userAgent;

			CFileItem fileToPlay(location, false);
			fileToPlay.SetProperty("StartPercent", position*100.0f);
			airplayp->AnnounceToClients(EVENT_LOADING);
			// froce to internal dvdplayer cause it is the only
			// one who will work well with airplay
			g_application.m_eForcedNextPlayer = EPC_DVDPLAYER;
			CApplicationMessenger::Get().MediaPlay(fileToPlay);

			// allow starting the player paused in ios8 mode (needed by camera roll app)
			if (CSettings::Get().GetBool("services.airplayios8compat") && !startPlayback)
			{
				CApplicationMessenger::Get().MediaPause();
				g_application.m_pPlayer->SeekPercentage(position * 100.0f);
			}
		}
*/
	}

	// Used to perform seeking (POST request) and to retrieve current player position (GET request).
	// GET scrub seems to also set rate 1 - strange but true
	else if (strcmp(uri, "/scrub") == 0)
	{
/*
		if (needAuth && !checkAuthorization(authorization, method, uri))
		{
			status = AIRPLAY_STATUS_NEED_AUTH;
		}
		else if (method == "GET")
		{
			logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got GET request %s", uri.c_str());

			if (g_application.m_pPlayer->GetTotalTime())
			{
				float position = ((float) g_application.m_pPlayer->GetTime()) / 1000;
				sprintf(responseBody, "duration: %.6f\r\nposition: %.6f\r\n", (float)g_application.m_pPlayer->GetTotalTime() / 1000, position);
			}
			else 
			{
				status = AIRPLAY_STATUS_METHOD_NOT_ALLOWED;
			}
		}
		else
		{
			const char* found = strstr(queryString.c_str(), "position=");

			if (found && g_application.m_pPlayer->HasPlayer())
			{
				int64_t position = (int64_t) (atof(found + strlen("position=")) * 1000.0);
				g_application.m_pPlayer->SeekTime(position);
				logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got POST request %s with pos %" PRId64, uri.c_str(), position);
			}
		}
*/
	}

	// Sent when media playback should be stopped
	else if (strcmp(uri, "/stop") == 0)
	{
/*
		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s", uri.c_str());
		if (needAuth && !checkAuthorization(authorization, method, uri))
		{
			status = AIRPLAY_STATUS_NEED_AUTH;
		}
		else
		{
			if (IsPlaying()) //only stop player if we started him
			{
				CApplicationMessenger::Get().MediaStop();
				m_isPlaying--;
			}
			else //if we are not playing and get the stop request - we just wanna stop picture streaming
			{
				CApplicationMessenger::Get().SendAction(ACTION_PREVIOUS_MENU);
			}
		}
		ClearPhotoAssetCache();
*/
	}

	// RAW JPEG data is contained in the request body
	else if (strcmp(uri, "/photo") == 0)
	{
/*
		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s", uri.c_str());
		if (needAuth && !checkAuthorization(authorization, method, uri))
		{
			status = AIRPLAY_STATUS_NEED_AUTH;
		}
		else if (m_httpParser->getContentLength() > 0 || photoAction == "displayCached")
		{
			XFILE::CFile tmpFile;
			char * tmpFileName = "special://temp/airplayasset";
			int showPhoto = true;
			int receivePhoto = true;


			if (photoAction == "cacheOnly")
				showPhoto = false;
			else if (photoAction == "displayCached")
			{
				receivePhoto = false;
				if (photoCacheId.length())
					logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: Trying to show from cache asset: %s", photoCacheId.c_str());
			}

			if (photoCacheId.length())
				tmpFileName += photoCacheId;
			else
				tmpFileName += "airplay_photo";

			if( receivePhoto && m_httpParser->getContentLength() > 3 &&
					m_httpParser->getBody()[1] == 'P' &&
					m_httpParser->getBody()[2] == 'N' &&
					m_httpParser->getBody()[3] == 'G')
			{
				tmpFileName += ".png";
			}
			else
			{
				tmpFileName += ".jpg";
			}

			int writtenBytes=0;
			if (receivePhoto)
			{
				if (tmpFile.OpenForWrite(tmpFileName, true))
				{
					writtenBytes = tmpFile.Write(m_httpParser->getBody(), m_httpParser->getContentLength());
					tmpFile.Close();
				}
				if (photoCacheId.length())
					logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: Cached asset: %s", photoCacheId.c_str());
			}

			if (showPhoto)
			{
				if ((writtenBytes > 0 && (unsigned int)writtenBytes == m_httpParser->getContentLength()) || !receivePhoto)
				{
					if (!receivePhoto && !XFILE::CFile::Exists(tmpFileName))
					{
						status = AIRPLAY_STATUS_PRECONDITION_FAILED; //image not found in the cache
						if (photoCacheId.length())
							CLog::Log(LOGWARNING, "AIRPLAY: Asset %s not found in our cache.", photoCacheId.c_str());
					}
					else
						CApplicationMessenger::Get().PictureShow(tmpFileName);
				}
				else
				{
					logger_log(conn->airplay->logger, LOGGER_INFO,"AirPlayServer: Error writing tmpFile.");
				}
			}
		}
*/
	}

	else if (strcmp(uri, "/playback-info") == 0)
	{
/*
		float position = 0.0f;
		float duration = 0.0f;
		float cachePosition = 0.0f;
		int playing = false;

		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s", uri.c_str());

		if (needAuth && !checkAuthorization(authorization, method, uri))
		{
			status = AIRPLAY_STATUS_NEED_AUTH;
		}
		else if (g_application.m_pPlayer->HasPlayer())
		{
			if (g_application.m_pPlayer->GetTotalTime())
			{
				position = ((float) g_application.m_pPlayer->GetTime()) / 1000;
				duration = ((float) g_application.m_pPlayer->GetTotalTime()) / 1000;
				playing = !g_application.m_pPlayer->IsPaused();
				cachePosition = position + (duration * g_application.m_pPlayer->GetCachePercentage() / 100.0f);
			}

			sprintf(responseBody, PLAYBACK_INFO, duration, cachePosition, position, (playing ? 1 : 0), duration);
			sprintf(responseHeader, "Content-Type: text/x-apple-plist+xml\r\n");

			if (g_application.m_pPlayer->IsCaching())
			{
				airplayp->AnnounceToClients(EVENT_LOADING);
			}
		}
		else
		{
			sprintf(responseBody, PLAYBACK_INFO_NOT_READY);
			sprintf(responseHeader, "Content-Type: text/x-apple-plist+xml\r\n");     
		}
*/
	}

	else if (strcmp(uri, "/stream.xml") == 0)
	{
		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s", uri);
		sprintf(responseBody, "%s", STREAM_INFO);
		sprintf(responseHeader, "Content-Type: text/x-apple-plist+xml\r\n");
		
	}
	else if (strcmp(uri, "/stream") == 0)
	{
		const char *plist_bin = NULL;
		char *xml = NULL;
		int size = 0;
		int type;
		plist_t root = NULL;

		plist_bin = http_request_get_data(request, &size);
 		plist_from_bin(plist_bin, size, &root);
		if (root) { 
			plist_to_xml(root, &xml, &size);
			if (xml) fprintf(stderr, "%s\n", xml);
			httpd_set_mirror_streaming(conn->airplay->mirror_server);
			return;
		} else {
			logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: Invalid bplist");
			status = AIRPLAY_STATUS_NOT_FOUND;
		}
	}
	else if (strcmp(uri, "/server-info") == 0)
	{
		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY: got request %s", uri);
		sprintf(responseBody, "%s\r\n", conn->airplay->hwaddr);
		sprintf(responseHeader, "Content-Type: text/x-apple-plist+xml\r\n");
	}

	else if (strcmp(uri, "/slideshow-features") == 0)
	{
		// Ignore for now.
	}

	else if (strcmp(uri, "/authorize") == 0)
	{
		// DRM, ignore for now.
	}

	else if (strcmp(uri, "/setProperty") == 0)
	{
		status = AIRPLAY_STATUS_NOT_FOUND;
	}

	else if (strcmp(uri, "/getProperty") == 0)
	{
		status = AIRPLAY_STATUS_NOT_FOUND;
	}

	else if (strcmp(uri, "/fp-setup") == 0)
	{
		//status = AIRPLAY_STATUS_PRECONDITION_FAILED;
		const unsigned char *data;
		int datalen, size;
                char *buf;

		extern unsigned char * send_fairplay_query(int cmd, const unsigned char *data, int len, int *size_p);

		data = http_request_get_data(request, &datalen);

	        buf = send_fairplay_query((datalen==16?1:2), data, datalen, &size);

		if (buf) {
		  memcpy(responseBody, buf, size);
		  responseLength = size;
		  sprintf(responseHeader, "Content-Type: application/octet-stream\r\n");
                }
	}  

	else if (strcmp(uri, "200") == 0) //response OK from the event reverse message
	{
		status = AIRPLAY_STATUS_NO_RESPONSE_NEEDED;
	}
	else
	{
		logger_log(conn->airplay->logger, LOGGER_INFO, "AIRPLAY Server: unhandled request [%s]\n", uri);
		status = AIRPLAY_STATUS_NOT_IMPLEMENTED;
	}

	if (status == AIRPLAY_STATUS_NEED_AUTH)
	{
		//ComposeAuthRequestAnswer(responseHeader, responseBody);
	}

	char * statusMsg = "OK";

	switch(status)
	{
		case AIRPLAY_STATUS_NOT_IMPLEMENTED:
			statusMsg = "Not Implemented";
			break;
		case AIRPLAY_STATUS_SWITCHING_PROTOCOLS:
			statusMsg = "Switching Protocols";
			//reverseSockets[sessionId] = m_socket;//save this socket as reverse http socket for this sessionid
			break;
		case AIRPLAY_STATUS_NEED_AUTH:
			statusMsg = "Unauthorized";
			break;
		case AIRPLAY_STATUS_NOT_FOUND:
			statusMsg = "Not Found";
			break;
		case AIRPLAY_STATUS_METHOD_NOT_ALLOWED:
			statusMsg = "Method Not Allowed";
			break;
		case AIRPLAY_STATUS_PRECONDITION_FAILED:
			statusMsg = "Precondition Failed";
			break;
	}

	// Prepare the response
	char resbuf[4096];
	const time_t ltime = time(NULL);
	char *date = asctime(gmtime(&ltime)); //Fri, 17 Dec 2010 11:18:01 GMT;
        int headerLength = 0;
	date[strlen(date) - 1] = '\0'; // remove \n
	sprintf(resbuf, "HTTP/1.1 %d %s\nDate: %s\r\n", status, statusMsg, date);
	if (responseHeader[0] != '\0') 
	{
		strcat(resbuf, responseHeader);
	}
  
        if (responseLength == 0) responseLength = strlen(responseBody); 

	sprintf(resbuf, "%sContent-Length: %d\r\n\r\n", resbuf, responseLength);

        headerLength = strlen(resbuf);

	if (responseLength)
	{
           memcpy(resbuf + strlen(resbuf), responseBody, responseLength);
	   resbuf[headerLength + responseLength] = 0;
	}

	http_response_t *res;
	res = http_response_init1(resbuf, headerLength + responseLength);

	logger_log(conn->airplay->logger, LOGGER_DEBUG, "AIRPLAY Handled request %s with response %s", method, http_response_get_data(res,&responseLength));
	*response = res;
}

static void
conn_destroy(void *ptr)
{
	airplay_conn_t *conn = ptr;

	if (conn->airplay_rtp) {
		/* This is done in case TEARDOWN was not called */
		raop_rtp_destroy(conn->airplay_rtp);
	}
	free(conn->local);
	free(conn->remote);
	free(conn);
}

static void 
conn_datafeed(void *ptr, unsigned char *data, int len)
{
	int size;
	unsigned short type;
	unsigned short type1;

	airplay_conn_t *conn = ptr;

	size = *(int*)data;
	type = *(unsigned short*)(data + 4);
	type1 = *(unsigned short*)(data + 6);

	logger_log(conn->airplay->logger, LOGGER_DEBUG, "Add data size=%d type %2x %2x", size, type, type1);
}

airplay_t *
airplay_init(int max_clients, airplay_callbacks_t *callbacks, const char *pemkey, int *error)
{
	airplay_t *airplay;
	httpd_t *httpd;
	httpd_t *mirror_server;
	rsakey_t *rsakey;
	httpd_callbacks_t httpd_cbs;

	assert(callbacks);
	assert(max_clients > 0);
	assert(max_clients < 100);
	assert(pemkey);

	/* Initialize the network */
	if (netutils_init() < 0) {
		return NULL;
	}

	/* Validate the callbacks structure */
	if (!callbacks->audio_init ||
	    !callbacks->audio_process ||
	    !callbacks->audio_destroy) {
		return NULL;
	}

	/* Allocate the airplay_t structure */
	airplay = calloc(1, sizeof(airplay_t));
	if (!airplay) {
		return NULL;
	}

	/* Initialize the logger */
	airplay->logger = logger_init();

	/* Set HTTP callbacks to our handlers */
	memset(&httpd_cbs, 0, sizeof(httpd_cbs));
	httpd_cbs.opaque = airplay;
	httpd_cbs.conn_init = &conn_init;
	httpd_cbs.conn_request = &conn_request;
	httpd_cbs.conn_destroy = &conn_destroy;
	httpd_cbs.conn_datafeed = &conn_datafeed;

	/* Initialize the http daemon */
	httpd = httpd_init(airplay->logger, &httpd_cbs, max_clients);
	if (!httpd) {
		free(airplay);
		return NULL;
	}

	/* Initialize the mirror server daemon */
	mirror_server = httpd_init(airplay->logger, &httpd_cbs, max_clients);
	if (!mirror_server) {
		free(httpd);
		free(airplay);
		return NULL;
	}

	/* Copy callbacks structure */
	memcpy(&airplay->callbacks, callbacks, sizeof(airplay_callbacks_t));

	/* Initialize RSA key handler */
	rsakey = rsakey_init_pem(pemkey);
	if (!rsakey) {
		free(httpd);
		free(mirror_server);
		free(airplay);
		return NULL;
	}

	airplay->httpd = httpd;
	airplay->rsakey = rsakey;

	airplay->mirror_server = mirror_server;

	return airplay;
}

airplay_t *
airplay_init_from_keyfile(int max_clients, airplay_callbacks_t *callbacks, const char *keyfile, int *error)
{
	airplay_t *airplay;
	char *pemstr;

	if (utils_read_file(&pemstr, keyfile) < 0) {
		return NULL;
	}
	airplay = airplay_init(max_clients, callbacks, pemstr, error);
	free(pemstr);
	return airplay;
}

void
airplay_destroy(airplay_t *airplay)
{
	if (airplay) {
		airplay_stop(airplay);

		httpd_destroy(airplay->httpd);
		httpd_destroy(airplay->mirror_server);
		rsakey_destroy(airplay->rsakey);
		logger_destroy(airplay->logger);
		free(airplay);

		/* Cleanup the network */
		netutils_cleanup();
	}
}

int
airplay_is_running(airplay_t *airplay)
{
	assert(airplay);

	return httpd_is_running(airplay->httpd);
}

void
airplay_set_log_level(airplay_t *airplay, int level)
{
	assert(airplay);

	logger_set_level(airplay->logger, level);
}

void
airplay_set_log_callback(airplay_t *airplay, airplay_log_callback_t callback, void *cls)
{
	assert(airplay);

	logger_set_callback(airplay->logger, callback, cls);
}

int
airplay_start(airplay_t *airplay, unsigned short *port, const char *hwaddr, int hwaddrlen, const char *password)
{
	int ret;
	unsigned short mirror_port;

	assert(airplay);
	assert(port);
	assert(hwaddr);

	/* Validate hardware address */
	if (hwaddrlen > MAX_HWADDR_LEN) {
		return -1;
	}

	memset(airplay->password, 0, sizeof(airplay->password));
	if (password) {
		/* Validate password */
		if (strlen(password) > MAX_PASSWORD_LEN) {
			return -1;
		}

		/* Copy password to the airplay structure */
		strncpy(airplay->password, password, MAX_PASSWORD_LEN);
	}

	/* Copy hwaddr to the airplay structure */
	memcpy(airplay->hwaddr, hwaddr, hwaddrlen);
	airplay->hwaddrlen = hwaddrlen;

	ret = httpd_start(airplay->httpd, port);
	if (ret != 1) return ret;

	mirror_port = 7100;
	ret = httpd_start(airplay->mirror_server, &mirror_port);
	return ret;
}

void
airplay_stop(airplay_t *airplay)
{
	assert(airplay);

	httpd_stop(airplay->httpd);
	httpd_stop(airplay->mirror_server);
}

