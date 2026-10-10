#ifndef NET_APP_ZLM_ENCODE_H
#define NET_APP_ZLM_ENCODE_H

#ifdef __cplusplus
extern "C" {
#endif

int zlm_encode_start(void);
void zlm_encode_stop(void);

/* Latest raw frame from video_process capture (thread-safe slot). */
void zlm_encode_publish_i420(const unsigned char *i420, int width, int height,
			     unsigned int frame_no);

#ifdef __cplusplus
}
#endif

#endif
