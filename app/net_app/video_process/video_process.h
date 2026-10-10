#ifndef NET_APP_VIDEO_PROCESS_H
#define NET_APP_VIDEO_PROCESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Capture (UVC→I420) + ZLM encode/push. zlm_server is internal to this module. */
int video_process_start(void);
void video_process_stop(void);
/*
 * Event MP4 recording: one file per alarm covering pre-roll (built into ZLM)
 * plus duration_sec of live video. path is the record file planned at event
 * publish time (main_process), carried on the alarm event.
 */
int video_process_record_alarm(const char *path, int duration_sec);

/* Still frames (BGR) composed into an MP4, one second per frame. */
#define PHOTO_FRAMES_MAX 10

typedef struct {
	const unsigned char *bgr;
	int width;
	int height;
} video_still_frame_t;

int video_process_compose_stills_mp4(const video_still_frame_t *frames,
				     int count, const char *path);

/* Applies RTC advertise IP from Host, then blocking SDP exchange. */
int video_process_webrtc_answer(const char *type, const char *offer,
				const char *app, const char *stream,
				const char *host, char **out_answer,
				char *errbuf, size_t errbuf_sz);

#ifdef __cplusplus
}
#endif

#endif
