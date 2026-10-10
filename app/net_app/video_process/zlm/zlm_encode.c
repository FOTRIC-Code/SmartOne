#include "zlm_encode.h"

#include "h264_encoder.h"
#include "libs/log/log.h"
#include "net_common/net_common.h"
#include "zlm_server.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * Encode/push path:
 *   video_process publishes I420 → on viewer demand → soft H.264 → ZLM
 */

#define ZLM_TARGET_FPS 25
#define ZLM_LOW_LATENCY_GOP 6
#define ZLM_IDLE_SLEEP_US 50000ULL

static pthread_t s_encode_thread;
static volatile int s_encode_running;
static int s_encode_started;

static pthread_mutex_t s_frame_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned char *s_i420;
static int s_i420_bytes;
static int s_w;
static int s_h;
static unsigned int s_frame_no;
static unsigned int s_gen;

static int ensure_buf(unsigned char **buf, int *cap, int need)
{
	if (*cap >= need)
		return 0;
	free(*buf);
	*buf = malloc((size_t)need);
	*cap = *buf ? need : 0;
	return *buf ? 0 : -1;
}

void zlm_encode_publish_i420(const unsigned char *i420, int width, int height,
			     unsigned int frame_no)
{
	int bytes;

	if (!i420 || width <= 0 || height <= 0)
		return;
	bytes = width * height * 3 / 2;

	pthread_mutex_lock(&s_frame_lock);
	if (ensure_buf(&s_i420, &s_i420_bytes, bytes) != 0) {
		s_w = s_h = 0;
		pthread_mutex_unlock(&s_frame_lock);
		return;
	}
	memcpy(s_i420, i420, (size_t)bytes);
	s_w = width;
	s_h = height;
	s_frame_no = frame_no;
	s_gen++;
	pthread_mutex_unlock(&s_frame_lock);
}

static int latest_i420_bytes(int *width, int *height)
{
	int bytes = 0;

	pthread_mutex_lock(&s_frame_lock);
	if (s_i420 && s_w > 0 && s_h > 0) {
		if (width)
			*width = s_w;
		if (height)
			*height = s_h;
		bytes = s_w * s_h * 3 / 2;
	}
	pthread_mutex_unlock(&s_frame_lock);
	return bytes;
}

static int copy_latest_i420(unsigned char *dst, int dst_bytes, int *width,
			    int *height, unsigned int *frame_no,
			    unsigned int *gen)
{
	int bytes;
	int rc = -1;

	if (!dst || dst_bytes <= 0)
		return -1;

	pthread_mutex_lock(&s_frame_lock);
	if (s_i420 && s_w > 0 && s_h > 0) {
		bytes = s_w * s_h * 3 / 2;
		if (dst_bytes >= bytes) {
			memcpy(dst, s_i420, (size_t)bytes);
			if (width)
				*width = s_w;
			if (height)
				*height = s_h;
			if (frame_no)
				*frame_no = s_frame_no;
			if (gen)
				*gen = s_gen;
			rc = 0;
		}
	}
	pthread_mutex_unlock(&s_frame_lock);
	return rc;
}

/* Y=0, U=V=128. Used only until the first camera frame so ZLM can regist(). */
static void fill_black_i420(unsigned char *dst, int w, int h)
{
	int y_bytes = w * h;

	memset(dst, 0, (size_t)y_bytes);
	memset(dst + y_bytes, 128, (size_t)y_bytes / 2);
}

static void clear_latest_i420(void)
{
	pthread_mutex_lock(&s_frame_lock);
	free(s_i420);
	s_i420 = NULL;
	s_i420_bytes = 0;
	s_w = s_h = 0;
	s_frame_no = 0;
	s_gen = 0;
	pthread_mutex_unlock(&s_frame_lock);
}

static stream_info_t *primary_stream(void)
{
	int i;

	for (i = 0; i < g_config.stream_count; ++i) {
		if (g_config.streams[i].id == 101)
			return &g_config.streams[i];
	}
	if (g_config.stream_count > 0)
		return &g_config.streams[0];
	return NULL;
}

static uint64_t monotonic_us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static int clamp_fps(int fps)
{
	if (fps < 1)
		return 1;
	if (fps > 30)
		return 30;
	return fps;
}

static int target_fps(const stream_info_t *stream)
{
	int fps = ZLM_TARGET_FPS;

	if (stream && stream->max_frame_rate > fps)
		fps = stream->max_frame_rate;
	return clamp_fps(fps);
}

static int ensure_encoder(h264_encoder_t **encoder, int enc_w, int enc_h,
			  int fps, int bitrate, int gop)
{
	h264_encoder_destroy(*encoder);
	*encoder = h264_encoder_create(enc_w, enc_h, fps, bitrate,
				       gop > 0 ? gop : fps);
	if (!*encoder) {
		LOGW("[ZLM] h264 encoder create failed %dx%d", enc_w, enc_h);
		return -1;
	}
	return 0;
}

static void release_push(h264_encoder_t **encoder, int *pub_w, int *pub_h)
{
	if (zlm_server_is_published()) {
		zlm_server_unpublish();
		LOGI("[ZLM] idle → stop encode/push");
	}
	h264_encoder_destroy(*encoder);
	*encoder = NULL;
	*pub_w = 0;
	*pub_h = 0;
}

static void *encode_thread(void *arg)
{
	stream_info_t *stream = primary_stream();
	h264_encoder_t *encoder = NULL;
	unsigned char *i420 = NULL;
	int i420_cap = 0;
	int published_w = 0;
	int published_h = 0;
	int published_fps = 0;
	int need_key = 1;
	int stream_id = stream ? stream->id : 101;
	int fps = target_fps(stream);
	uint64_t period_us = 1000000ULL / (uint64_t)fps;
	uint64_t next_us = monotonic_us();
	uint64_t dts_origin_us = next_us;
	int have_encoded_frame = 0;
	int prev_readers = 0;
	uint64_t last_prime_us = 0;

	(void)arg;
	LOGI("[ZLM] encode thread started (stream_id=%d fps=%d)", stream_id,
	     fps);

	while (s_encode_running) {
		int readers;
		int demand;
		int enc_w = 0;
		int enc_h = 0;
		int force_key;
		const unsigned char *nalu = NULL;
		int nalu_size = 0;
		int bitrate;
		int gop;
		uint64_t now_us;
		size_t need;

		now_us = monotonic_us();
		if (next_us > now_us)
			usleep((useconds_t)(next_us - now_us));
		next_us += period_us;
		if (monotonic_us() > next_us + period_us)
			next_us = monotonic_us() + period_us;

		if (!zlm_server_is_ready())
			continue;

		/*
		 * Proactively register the ZLM media track so
		 * RTSP/WebRTC clients never get "stream not found".
		 * A placeholder 320x240 track is created; it is
		 * replaced with real dimensions once frames arrive.
		 */
		if (!zlm_server_is_published()) {
			int pre_br = stream && stream->bit_rate > 0
					     ? stream->bit_rate
					     : 400000;
			if (zlm_server_publish(stream_id, 320, 240, fps,
					       pre_br) == 0) {
				published_w = 320;
				published_h = 240;
				published_fps = fps;
				need_key = 1;
				have_encoded_frame = 0;
				dts_origin_us = monotonic_us();
				LOGI("[ZLM] proactive publish 320x240 at startup");
			}
		}

		stream = primary_stream();
		if (stream)
			stream_id = stream->id;
		fps = target_fps(stream);
		period_us = 1000000ULL / (uint64_t)fps;

		readers = zlm_server_reader_count(stream_id);
		demand = zlm_server_has_viewer_demand(stream_id);
		if (stream) {
			stream->connection_count = readers;
			stream->max_frame_rate = fps;
		}
		g_system_status.streaming = readers > 0 ? 1 : 0;
		if (readers > prev_readers)
			need_key = 1;
		prev_readers = readers;

		if (!demand) {
			/*
			 * No viewers — encode at 1 fps to keep the ZLM media
			 * source alive. ZLM requires actual H.264 data (SPS/
			 * PPS + IDR) before findAsync considers a stream
			 * "found"; an empty mk_media placeholder is not enough.
			 * 1 fps priming costs negligible CPU at 320x240.
			 */
			uint64_t idle_now = monotonic_us();
			if (have_encoded_frame &&
			    idle_now - last_prime_us < 1000000ULL) {
				usleep((useconds_t)ZLM_IDLE_SLEEP_US);
				continue;
			}
			last_prime_us = idle_now;
			need_key = !have_encoded_frame;
			/* fall through to encode one priming frame */
		}

		/*
		 * Size the buffer from the real frame. A fixed 320x240 slot
		 * rejects taller frames (for example 320x256 from a 640x512
		 * sensor): copy fails, enc_w stays 0, and no H.264 is ever
		 * pushed, so WebRTC play of stream 101 is "stream not found".
		 */
		need = (size_t)latest_i420_bytes(&enc_w, &enc_h);
		if (need == 0) {
			static int logged_black;

			enc_w = 320;
			enc_h = 240;
			need = (size_t)enc_w * enc_h * 3 / 2;
			if (ensure_buf(&i420, &i420_cap, (int)need) != 0)
				continue;
			fill_black_i420(i420, enc_w, enc_h);
			if (!logged_black) {
				logged_black = 1;
				LOGW("[ZLM] no camera frame yet, priming black "
				     "%dx%d so stream %d can register",
				     enc_w, enc_h, stream_id);
			}
		} else {
			if (ensure_buf(&i420, &i420_cap, (int)need) != 0)
				continue;
			if (copy_latest_i420(i420, i420_cap, &enc_w, &enc_h,
					     NULL, NULL) != 0)
				continue;
		}

		if (stream)
			stream->height = enc_h;

		bitrate = stream && stream->bit_rate > 0 ? stream->bit_rate : 400000;
		gop = ZLM_LOW_LATENCY_GOP;
		if (stream && stream->gop_size > 0 && stream->gop_size < gop)
			gop = stream->gop_size;
		if (gop < 1)
			gop = 1;

		if (!zlm_server_is_published() || published_w != enc_w ||
		    published_h != enc_h || published_fps != fps) {
			if (zlm_server_publish(stream_id, enc_w, enc_h, fps,
					       bitrate) != 0) {
				LOGW("[ZLM] publish failed id=%d %dx%d", stream_id,
				     enc_w, enc_h);
				continue;
			}
			if (ensure_encoder(&encoder, enc_w, enc_h, fps, bitrate,
					   gop) != 0)
				continue;
			LOGI("[ZLM] encode/push start %dx%d %dfps gop=%d", enc_w,
			     enc_h, fps, gop);
			published_w = enc_w;
			published_h = enc_h;
			published_fps = fps;
			need_key = 1;
			have_encoded_frame = 0;
			dts_origin_us = monotonic_us();
		}

		force_key = need_key || !have_encoded_frame;
		need_key = 0;

		if (!encoder &&
		    ensure_encoder(&encoder, enc_w, enc_h, fps, bitrate, gop) != 0)
			continue;

		if (h264_encoder_encode(encoder, i420, force_key, &nalu,
					&nalu_size) != 0)
			continue;

		now_us = monotonic_us();
		if (nalu && nalu_size > 0)
			zlm_server_input_h264(nalu, nalu_size,
					     (now_us - dts_origin_us) / 1000ULL);

		have_encoded_frame = 1;
	}

	release_push(&encoder, &published_w, &published_h);
	free(i420);
	LOGI("[ZLM] encode thread stopped");
	return NULL;
}

int zlm_encode_start(void)
{
	if (s_encode_started)
		return 0;
	s_encode_running = 1;
	if (pthread_create(&s_encode_thread, NULL, encode_thread, NULL) != 0) {
		s_encode_running = 0;
		LOGE("[ZLM] failed to start encode thread");
		return -1;
	}
	s_encode_started = 1;
	LOGI("[ZLM] encode thread created, waiting for viewer demand");
	return 0;
}

void zlm_encode_stop(void)
{
	if (!s_encode_started)
		return;
	s_encode_running = 0;
	pthread_join(s_encode_thread, NULL);
	clear_latest_i420();
	s_encode_started = 0;
}
