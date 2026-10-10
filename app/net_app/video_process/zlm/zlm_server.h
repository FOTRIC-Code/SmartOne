#ifndef NET_APP_ZLM_SERVER_H
#define NET_APP_ZLM_SERVER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal to video_process (and zlm_encode). Outside callers must use
 * video_process.h — do not include this header from main/http/provisioning.
 */

int zlm_server_start(void);
void zlm_server_stop(void);
int zlm_server_publish(int stream_id, int width, int height, int fps,
		       int bit_rate);
void zlm_server_unpublish(void);
int zlm_server_is_published(void);
int zlm_server_input_h264(const void *data, int len, unsigned long long dts_ms);
int zlm_server_reader_count(int stream_id);
int zlm_server_is_ready(void);
/*
 * True when an external player is attached, or briefly after play/not-found /
 * WebRTC signaling so the pipeline can publish SPS/IDR before ICE completes.
 */
int zlm_server_has_viewer_demand(int stream_id);
/* Extend the demand window (seconds from now). Used by WebRTC proxy path. */
void zlm_server_bump_viewer_demand(int sec);
/*
 * Event-style MP4 recording: one file per call covering
 * [now - back_ms, now + forward_ms]; ZLM stops it automatically.
 * path is an absolute file path including the .mp4 name.
 */
int zlm_server_start_mp4_record_task(const char *path, unsigned int back_ms,
				     unsigned int forward_ms);
/* Refresh SoftAP/STA default advertise IP (safe after zlm_server_start). */
void zlm_server_update_advertise_ip(void);
/* Match ICE host candidates to the client's HTTP path (USB / SoftAP / STA). */
void zlm_server_apply_rtc_extern_ip(const char *host_or_ip);
/*
 * Blocking WebRTC SDP exchange (play/push/echo).
 * On success returns 0 and *out_answer (caller frees). On failure returns -1
 * and optionally fills errbuf.
 */
int zlm_server_webrtc_answer(const char *type, const char *offer,
			     const char *app, const char *stream,
			     const char *host, char **out_answer,
			     char *errbuf, size_t errbuf_sz);

#ifdef __cplusplus
}
#endif

#endif
