#include "video_process.h"

#include "thermal/thermal_data_provider.h"
#include "image_convert.h"
#include "libs/log/log.h"
#include "net_common/net_common.h"
#include "zlm/zlm_encode.h"
#include "zlm/zlm_server.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* UVC → BGR → I420; publish to zlm_encode for demand-driven encode/push. */

#define VIDEO_MAX_ENC_W 320
#define VIDEO_POLL_US 2000
#define VIDEO_UVC_RETRY_US (1 * 1000000)
#define VIDEO_UVC_STALL_US (3 * 1000000)
#define VIDEO_ALARM_PRE_ROLL_MS 10000U

static pthread_t s_thread;
static volatile int s_running;
static int s_started;

int video_process_record_alarm(const char *path, int duration_sec)
{
	unsigned int forward_ms;
	unsigned int total_ms;

	if (!s_started || duration_sec <= 0 || !path || !path[0])
		return -1;
	/* record_duration_sec is the total length, pre-roll included. */
	total_ms = (unsigned int)duration_sec * 1000U;
	forward_ms = total_ms > VIDEO_ALARM_PRE_ROLL_MS ?
			     total_ms - VIDEO_ALARM_PRE_ROLL_MS :
			     1000U;
	if (zlm_server_start_mp4_record_task(path, VIDEO_ALARM_PRE_ROLL_MS,
					     forward_ms) != 0) {
		LOGE("[VIDEO] alarm MP4 task record start failed: %s", path);
		return -1;
	}
	LOGI("[VIDEO] alarm MP4 task record started: %s", path);
	return 0;
}

static uint64_t now_us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static stream_info_t *stream_101(void)
{
	int i;

	for (i = 0; i < g_config.stream_count; ++i) {
		if (g_config.streams[i].id == 101)
			return &g_config.streams[i];
	}
	return g_config.stream_count > 0 ? &g_config.streams[0] : NULL;
}

static void target_size(int src_w, int src_h, int *out_w, int *out_h)
{
	stream_info_t *s = stream_101();
	int w = src_w;

	if (s && s->width > 0 && s->width < w)
		w = s->width;
	if (w > VIDEO_MAX_ENC_W)
		w = VIDEO_MAX_ENC_W;
	*out_w = image_align16(w);
	*out_h = image_align16((src_h * (*out_w) + src_w / 2) / src_w);
	if (*out_h < 16)
		*out_h = 16;
}

static int ensure_buf(unsigned char **buf, int *cap, int need)
{
	if (*cap >= need)
		return 0;
	free(*buf);
	*buf = malloc((size_t)need);
	*cap = *buf ? need : 0;
	return *buf ? 0 : -1;
}

static int to_i420(const unsigned char *bgr, int src_w, int src_h, int dst_w,
		   int dst_h, unsigned char **i420, int *i420_cap,
		   unsigned char **scaled, int *scaled_cap)
{
	if (ensure_buf(i420, i420_cap, dst_w * dst_h * 3 / 2) != 0)
		return -1;

	if (src_w == dst_w && src_h == dst_h) {
		image_bgr_to_i420(bgr, dst_w, dst_h, *i420);
		return 0;
	}
	if (ensure_buf(scaled, scaled_cap, dst_w * dst_h * 3) != 0)
		return -1;
	image_scale_bgr(bgr, src_w, src_h, *scaled, dst_w, dst_h);
	image_bgr_to_i420(*scaled, dst_w, dst_h, *i420);
	return 0;
}

static void *capture_thread(void *arg)
{
	unsigned char *bgr = NULL;
	unsigned char *scaled = NULL;
	unsigned char *i420 = NULL;
	int bgr_cap = 0;
	int scaled_cap = 0;
	int i420_cap = 0;
	uint64_t last_uvc_try = 0;
	uint64_t native_ok_since = 0;
	unsigned int last_frame_no = 0;
	int have_frame = 0;
	int uvc_restarts = 0;

	(void)arg;
	LOGI("[VIDEO] capture started");

	while (s_running) {
		thermal_preview_frame_t preview = { 0 };
		int src_w = 0;
		int src_h = 0;
		int dst_w = 0;
		int dst_h = 0;
		unsigned int frame_no = 0;
		uint64_t t = now_us();
		stream_info_t *stream;

		if (thermal_provider_get_native_size(&src_w, &src_h) != 0 ||
		    src_w <= 0 || src_h <= 0) {
			native_ok_since = 0;
			if (t - last_uvc_try >= VIDEO_UVC_RETRY_US) {
				last_uvc_try = t;
				thermal_provider_start();
			}
			usleep(VIDEO_POLL_US);
			continue;
		}
		if (!native_ok_since)
			native_ok_since = t;

		if (ensure_buf(&bgr, &bgr_cap, src_w * src_h * 3) != 0) {
			usleep(VIDEO_POLL_US);
			continue;
		}
		preview.data = bgr;
		preview.capacity = (size_t)bgr_cap;
		if (thermal_provider_copy_preview_bgr(&preview) != 0) {
			if (!have_frame && native_ok_since &&
			    t - native_ok_since > VIDEO_UVC_STALL_US &&
			    uvc_restarts < 5) {
				thermal_frame_stats_t rs = { 0 };

				thermal_provider_get_frame_stats(&rs);
				if (rs.callbacks == 0) {
					uvc_restarts++;
					thermal_provider_stop();
					usleep(500000);
					thermal_provider_start();
					native_ok_since = 0;
				} else {
					native_ok_since = t;
				}
			}
			usleep(VIDEO_POLL_US);
			continue;
		}
		uvc_restarts = 0;
		src_w = preview.width;
		src_h = preview.height;
		frame_no = preview.frame_no;

		/* Same UVC frame_no → keep last published I420; encode fills fps. */
		if (have_frame && frame_no == last_frame_no) {
			usleep(VIDEO_POLL_US);
			continue;
		}

		target_size(src_w, src_h, &dst_w, &dst_h);
		stream = stream_101();
		if (stream)
			stream->height = dst_h;

		if (to_i420(bgr, src_w, src_h, dst_w, dst_h, &i420, &i420_cap,
			    &scaled, &scaled_cap) != 0)
			continue;

		zlm_encode_publish_i420(i420, dst_w, dst_h, frame_no);
		last_frame_no = frame_no;
		have_frame = 1;
	}

	free(bgr);
	free(scaled);
	free(i420);
	LOGI("[VIDEO] capture stopped");
	return NULL;
}

int video_process_start(void)
{
	if (s_started)
		return 0;
	s_running = 1;
	if (pthread_create(&s_thread, NULL, capture_thread, NULL) != 0) {
		s_running = 0;
		LOGE("[VIDEO] capture thread failed");
		return -1;
	}
	s_started = 1;
	if (zlm_server_start() != 0)
		LOGW("[VIDEO] ZLM start failed, RTSP/WebRTC unavailable");
	return 0;
}

void video_process_stop(void)
{
	if (!s_started)
		return;
	s_running = 0;
	pthread_join(s_thread, NULL);
	zlm_server_stop();
	s_started = 0;
}

int video_process_webrtc_answer(const char *type, const char *offer,
				const char *app, const char *stream,
				const char *host, char **out_answer,
				char *errbuf, size_t errbuf_sz)
{
	zlm_server_apply_rtc_extern_ip(host && host[0] ? host : NULL);
	return zlm_server_webrtc_answer(type, offer, app, stream, host,
					out_answer, errbuf, errbuf_sz);
}
