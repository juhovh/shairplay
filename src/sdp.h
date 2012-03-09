#ifndef SDP_H
#define SDP_H

typedef struct sdp_s sdp_t;

sdp_t *sdp_init(const char *sdpdata, int sdpdatalen);

const char *sdp_get_version(sdp_t *sdp);
const char *sdp_get_origin(sdp_t *sdp);
const char *sdp_get_session(sdp_t *sdp);
const char *sdp_get_connection(sdp_t *sdp);
const char *sdp_get_time(sdp_t *sdp);
const char *sdp_get_media(sdp_t *sdp);
const char *sdp_get_rtpmap(sdp_t *sdp);
const char *sdp_get_fmtp(sdp_t *sdp);
const char *sdp_get_rsaaeskey(sdp_t *sdp);
const char *sdp_get_aesiv(sdp_t *sdp);
const char *sdp_get_min_latency(sdp_t *sdp);

void sdp_destroy(sdp_t *sdp);

#endif
