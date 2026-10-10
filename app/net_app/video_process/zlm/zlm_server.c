#include "zlm_server.h"

#include "libs/log/log.h"
#include "libs/time/time_util.h"
#include "net_common/net_common.h"
#include "zlm_encode.h"

#include "mk_mediakit.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* Demand window so pipeline can publish SPS/IDR before ICE/reader attaches. */
#define ZLM_VIEWER_DEMAND_SEC 20
#define NET_SOFTAP_ADVERTISE_IP "192.168.4.1"

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_ready;
static int s_stream_id;
static mk_media s_media;
static volatile time_t s_viewer_demand_until;

int zlm_server_start_mp4_record_task(const char *path, unsigned int back_ms,
				     unsigned int forward_ms)
{
	char stream[16];

	if (!path || !path[0] || !forward_ms)
		return -1;
	zlm_server_bump_viewer_demand((int)((back_ms + forward_ms) / 1000U) + 5);
	snprintf(stream, sizeof(stream), "%d",
		 s_stream_id > 0 ? s_stream_id : 101);
	return mk_recorder_start_task("__defaultVhost__", "video", stream,
				      path, back_ms, forward_ms) ?
		       0 :
		       -1;
}

void zlm_server_bump_viewer_demand(int sec)
{
	if (sec <= 0)
		sec = ZLM_VIEWER_DEMAND_SEC;
	s_viewer_demand_until = time_now_sec() + sec;
}
/*
 * HTTP Host from :80/media/webrtc (USB / SoftAP / STA). Pinned for the
 * duration of mk_webrtc_get_answer_sdp so SoftAP-local sockets cannot
 * overwrite ICE advertise IP mid-exchange.
 */
static char s_path_ip[64];

static void API_CALL on_zlm_flow_report(const mk_media_info url_info,
					size_t total_bytes, size_t total_seconds,
					int is_player, const mk_sock_info sender)
{
	const char *schema;

	(void)total_bytes;
	(void)total_seconds;
	(void)sender;

	schema = mk_media_info_get_schema(url_info);
	if (is_player && schema && strcmp(schema, "rtc") == 0)
		LOGI("[ZLM] rtc_session_released");
}

static void API_CALL on_zlm_media_play(const mk_media_info url_info,
				       const mk_auth_invoker invoker,
				       const mk_sock_info sender)
{
	const char *schema;
	const char *app;
	const char *stream;

	schema = mk_media_info_get_schema(url_info);
	app = mk_media_info_get_app(url_info);
	stream = mk_media_info_get_stream(url_info);
	LOGI("[ZLM] media_play schema=%s app=%s stream=%s published=%d",
	     schema ? schema : "?", app ? app : "?", stream ? stream : "?",
	     s_media ? 1 : 0);

	/* Any external play (RTSP/WebRTC) → start pushing until readers attach. */
	zlm_server_bump_viewer_demand(ZLM_VIEWER_DEMAND_SEC);

	if (schema && strcmp(schema, "rtc") == 0) {
		char local_ip[64] = { 0 };

		/*
		 * ICE must advertise the interface the browser actually uses
		 * (USB 192.168.123.x vs SoftAP 192.168.4.x). In-process answer
		 * often sees SoftAP as local_ip even when Host is USB — do not
		 * clobber a Host-pinned path.
		 */
		if (!s_path_ip[0] && sender) {
			mk_sock_info_local_ip(sender, local_ip);
			if (local_ip[0] && strcmp(local_ip, "127.0.0.1") != 0 &&
			    strcmp(local_ip, "0.0.0.0") != 0)
				zlm_server_apply_rtc_extern_ip(local_ip);
		}
	}
	mk_auth_invoker_do(invoker, NULL);
}

/*
 * No media yet: ask ZLM to wait while video_process publishes on demand.
 * Return 0 = wait for registration; 1 = close immediately.
 */
static int API_CALL on_zlm_media_not_found(const mk_media_info url_info,
					   const mk_sock_info sender)
{
	const char *app;
	const char *stream;

	(void)sender;
	app = mk_media_info_get_app(url_info);
	stream = mk_media_info_get_stream(url_info);
	if (!app || strcmp(app, "video") != 0)
		return 1;

	zlm_server_bump_viewer_demand(ZLM_VIEWER_DEMAND_SEC);
	LOGI("[ZLM] media_not_found app=%s stream=%s → wait publish",
	     app, stream ? stream : "?");
	return 0;
}

/* Last viewer left (after ZLM none-reader delay): stop encode demand. */
static void API_CALL on_zlm_media_no_reader(const mk_media_source sender)
{
	(void)sender;
	s_viewer_demand_until = 0;
	LOGI("[ZLM] media_no_reader → clear demand (stop push)");
}

static void copy_host_ip(const char *host, char *ip, size_t ip_sz)
{
	size_t n;

	if (!ip || ip_sz == 0)
		return;
	ip[0] = '\0';
	if (!host || !host[0])
		return;

	n = strcspn(host, ":");
	if (n >= ip_sz)
		n = ip_sz - 1;
	memcpy(ip, host, n);
	ip[n] = '\0';
}

static void apply_rtc_advertise_ip(void)
{
	/*
	 * SoftAP clients are on 192.168.4.0/24. Without rtc.externIP, ZLM uses
	 * SockUtil::get_local_ip() which may pick the wrong iface and ICE fails.
	 * USB (192.168.123.100) is selected per-request from HTTP Host — see
	 * zlm_server_apply_rtc_extern_ip() — because SoftAP is often still up.
	 */
	if (g_config.provision.network_mode == NET_NETWORK_MODE_AP) {
		mk_ini_set_option(mk_ini_default(), "rtc.externIP",
				  NET_SOFTAP_ADVERTISE_IP);
		LOGI("[ZLM] rtc.externIP=%s (SoftAP default)",
		     NET_SOFTAP_ADVERTISE_IP);
	} else {
		mk_ini_set_option(mk_ini_default(), "rtc.externIP", "");
		LOGI("[ZLM] rtc.externIP=auto (STA)");
	}
}

void zlm_server_apply_rtc_extern_ip(const char *host_or_ip)
{
	char ip[64];

	copy_host_ip(host_or_ip, ip, sizeof(ip));
	if (ip[0] && strcmp(ip, "127.0.0.1") != 0 &&
	    strcmp(ip, "localhost") != 0 && strcmp(ip, "0.0.0.0") != 0) {
		mk_ini_set_option(mk_ini_default(), "rtc.externIP", ip);
		snprintf(s_path_ip, sizeof(s_path_ip), "%s", ip);
		LOGI("[ZLM] rtc.externIP=%s (client path)", ip);
		return;
	}
	if (s_path_ip[0]) {
		mk_ini_set_option(mk_ini_default(), "rtc.externIP", s_path_ip);
		LOGI("[ZLM] rtc.externIP=%s (keep pinned path)", s_path_ip);
		return;
	}
	apply_rtc_advertise_ip();
}

void zlm_server_update_advertise_ip(void)
{
	if (!zlm_server_is_ready())
		return;
	apply_rtc_advertise_ip();
}

static void apply_protocol_options(void)
{
	char port_str[16];

	snprintf(port_str, sizeof(port_str), "%u", (unsigned)ZLM_RTC_PORT);
	mk_ini_set_option(mk_ini_default(), "rtc.port", port_str);
	mk_ini_set_option(mk_ini_default(), "rtc.timeoutSec", "15");
	apply_rtc_advertise_ip();
	mk_ini_set_option(mk_ini_default(), "protocol.enable_hls", "0");
	mk_ini_set_option(mk_ini_default(), "protocol.enable_hls_fmp4", "0");
	mk_ini_set_option(mk_ini_default(), "protocol.enable_mp4", "1");
	mk_ini_set_option(mk_ini_default(), "protocol.enable_ts", "0");
	mk_ini_set_option(mk_ini_default(), "protocol.enable_fmp4", "0");
	mk_ini_set_option(mk_ini_default(), "protocol.enable_rtmp", "0");
	mk_ini_set_option(mk_ini_default(), "protocol.enable_rtsp", "1");
	mk_ini_set_option(mk_ini_default(), "protocol.enable_audio", "0");
	mk_ini_set_option(mk_ini_default(), "protocol.add_mute_audio", "0");
	mk_ini_set_option(mk_ini_default(), "protocol.auto_close", "0");
	/*
	 * GOP cache is shared with the muxer: event recording (pre-roll) reads
	 * its back window from this cache. At 25fps with GOP=6, 64 GOPs cover
	 * ~15s, comfortably above the 10s alarm pre-roll.
	 */
	mk_ini_set_option(mk_ini_default(), "rtp_proxy.gop_cache", "64");
	/*
	 * Do NOT set wait_track_ready_ms=0. With 0, ZLM discards the H264 track
	 * before SPS/PPS arrive, RTSP never regist()'s, and WebRTC findAsync
	 * hangs until our answer timeout (Holly keeps the 10s default).
	 */
	/* Live thermal: flush RTP ASAP; mergeWrite adds coalescing delay. */
	mk_ini_set_option(mk_ini_default(), "general.mergeWriteMS", "0");
	mk_ini_set_option(mk_ini_default(), "rtsp.lowLatency", "1");
	mk_ini_set_option(mk_ini_default(), "rtp.lowLatency", "1");
	/* Use input DTS as-is; relative/system stamp rewrite can add lag. */
	mk_ini_set_option(mk_ini_default(), "protocol.modify_stamp", "0");
	mk_ini_set_option(mk_ini_default(), "general.maxStreamWaitMS", "5000");
}

int zlm_server_start(void)
{
	mk_config config = {
		.thread_num = 2, /* media input + RTSP/WebRTC serve */
		.log_level = 1, /* Debug: surface record history / signaling details */
		.log_mask = LOG_CONSOLE | LOG_FILE,
		.log_file_path = "/tmp/zlm_log",
		.log_file_days = 1,
		.ini_is_path = 0,
		.ini = NULL,
		.ssl_is_path = 0,
		.ssl = NULL,
		.ssl_pwd = NULL,
	};
	mk_events events;
	uint16_t port;

	pthread_mutex_lock(&s_lock);
	if (s_ready) {
		pthread_mutex_unlock(&s_lock);
		return 0;
	}

	mk_env_init(&config);
	apply_protocol_options();

	port = mk_rtsp_server_start((uint16_t)g_config.port_info.rtsp_port_no, 0);
	if (port == 0) {
		pthread_mutex_unlock(&s_lock);
		LOGE("[ZLM] RTSP listen failed on port %d",
		     g_config.port_info.rtsp_port_no);
		return -1;
	}
	LOGI("[ZLM] RTSP on port %u", port);

	port = mk_http_server_start(ZLM_HTTP_PORT, 0);
	if (port == 0) {
		pthread_mutex_unlock(&s_lock);
		LOGE("[ZLM] HTTP listen failed on port %d (WebRTC signaling dead)",
		     ZLM_HTTP_PORT);
		return -1;
	}
	LOGI("[ZLM] HTTP on port %u (WebRTC signaling)", port);

	port = mk_rtc_server_start(ZLM_RTC_PORT);
	if (port == 0) {
		pthread_mutex_unlock(&s_lock);
		LOGE("[ZLM] RTC listen failed on port %d", ZLM_RTC_PORT);
		return -1;
	}
	LOGI("[ZLM] RTC  on port %u (UDP media)", port);

	memset(&events, 0, sizeof(events));
	events.on_mk_media_play = on_zlm_media_play;
	events.on_mk_media_not_found = on_zlm_media_not_found;
	events.on_mk_media_no_reader = on_zlm_media_no_reader;
	events.on_mk_flow_report = on_zlm_flow_report;
	mk_events_listen(&events);

	s_ready = 1;
	g_system_status.zlm_ready = 1;
	pthread_mutex_unlock(&s_lock);

	if (zlm_encode_start() != 0)
		LOGW("[ZLM] encode thread start failed");
	return 0;
}

void zlm_server_unpublish(void)
{
	pthread_mutex_lock(&s_lock);
	if (s_media) {
		mk_media_release(s_media);
		s_media = NULL;
		LOGI("[ZLM] unpublished stream_id=%d", s_stream_id);
	}
	s_stream_id = 0;
	pthread_mutex_unlock(&s_lock);
}

int zlm_server_is_published(void)
{
	int published;

	pthread_mutex_lock(&s_lock);
	published = s_media != NULL;
	pthread_mutex_unlock(&s_lock);
	return published;
}

void zlm_server_stop(void)
{
	zlm_encode_stop();
	pthread_mutex_lock(&s_lock);
	if (s_media) {
		mk_media_release(s_media);
		s_media = NULL;
	}
	s_stream_id = 0;
	if (s_ready) {
		mk_stop_all_server();
		s_ready = 0;
	}
	g_system_status.zlm_ready = 0;
	g_system_status.streaming = 0;
	{
		int i;

		for (i = 0; i < g_config.stream_count; ++i)
			g_config.streams[i].connection_count = 0;
	}
	pthread_mutex_unlock(&s_lock);
	LOGI("[ZLM] server stopped");
}

int zlm_server_publish(int stream_id, int width, int height, int fps,
		       int bit_rate)
{
	char stream_name[16];
	codec_args v_args;
	mk_track v_track;
	mk_media media;

	(void)bit_rate;
	if (stream_id <= 0 || width <= 0 || height <= 0 || fps <= 0)
		return -1;

	snprintf(stream_name, sizeof(stream_name), "%d", stream_id);
	media = mk_media_create("__defaultVhost__", "video", stream_name, 0, 0, 0);
	if (!media) {
		LOGE("[ZLM] mk_media_create failed for stream %d", stream_id);
		return -1;
	}

	memset(&v_args, 0, sizeof(v_args));
	v_args.video.width = width;
	v_args.video.height = height;
	v_args.video.fps = fps;
	v_track = mk_track_create(MKCodecH264, &v_args);
	if (!v_track) {
		LOGE("[ZLM] mk_track_create failed for stream %d", stream_id);
		mk_media_release(media);
		return -1;
	}
	mk_media_init_track(media, v_track);
	mk_media_init_complete(media);
	mk_track_unref(v_track);

	pthread_mutex_lock(&s_lock);
	if (s_media)
		mk_media_release(s_media);
	s_media = media;
	s_stream_id = stream_id;
	pthread_mutex_unlock(&s_lock);

	LOGI("[ZLM] stream_id=%d media ready codec=H264 %dx%d@%d", stream_id,
	     width, height, fps);
	LOGI("[ZLM]   RTSP  : rtsp://device_ip:%d/video/%d",
	     g_config.port_info.rtsp_port_no, stream_id);
	LOGI("[ZLM]   WebRTC: http://device_ip:%d/media/webrtc?app=video&stream=%d&type=play",
	     ZLM_HTTP_PORT, stream_id);
	return 0;
}

int zlm_server_input_h264(const void *data, int len, unsigned long long dts_ms)
{
	mk_media media;
	int ret;

	if (!data || len <= 0)
		return -1;

	pthread_mutex_lock(&s_lock);
	media = s_media;
	pthread_mutex_unlock(&s_lock);
	if (!media)
		return -1;

	ret = mk_media_input_h264(media, data, len, dts_ms, dts_ms);
	if (!ret)
		LOGW("[ZLM] input_h264 failed len=%d dts=%llu", len,
		     (unsigned long long)dts_ms);
	return ret ? 0 : -1;
}

int zlm_server_reader_count(int stream_id)
{
	mk_media media;
	int count;

	pthread_mutex_lock(&s_lock);
	if (!s_media || (stream_id != 0 && stream_id != s_stream_id)) {
		pthread_mutex_unlock(&s_lock);
		return 0;
	}
	media = s_media;
	pthread_mutex_unlock(&s_lock);
	count = mk_media_total_reader_count(media);
	return count > 0 ? count : 0;
}

int zlm_server_has_viewer_demand(int stream_id)
{
	time_t now;
	time_t until;

	if (zlm_server_reader_count(stream_id) > 0)
		return 1;
	now = time_now_sec();
	until = s_viewer_demand_until;
	return (until != 0 && now <= until) ? 1 : 0;
}

int zlm_server_is_ready(void)
{
	int ready;

	pthread_mutex_lock(&s_lock);
	ready = s_ready;
	pthread_mutex_unlock(&s_lock);
	return ready;
}

typedef struct {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int done;
	/* Waiter timed out; late ZLM callback must free this heap object. */
	int abandoned;
	char *answer;
	char *err;
} webrtc_wait_t;

static void webrtc_wait_free(webrtc_wait_t *w)
{
	if (!w)
		return;
	free(w->answer);
	free(w->err);
	pthread_mutex_destroy(&w->lock);
	pthread_cond_destroy(&w->cond);
	free(w);
}

static void API_CALL on_webrtc_answer(void *user_data, const char *answer,
				      const char *err)
{
	webrtc_wait_t *w = (webrtc_wait_t *)user_data;
	int abandoned;

	pthread_mutex_lock(&w->lock);
	if (answer && answer[0])
		w->answer = strdup(answer);
	if (err && err[0])
		w->err = strdup(err);
	w->done = 1;
	abandoned = w->abandoned;
	pthread_cond_signal(&w->cond);
	pthread_mutex_unlock(&w->lock);

	/* Timeout path already returned; we own lifetime now. */
	if (abandoned)
		webrtc_wait_free(w);
}

int zlm_server_webrtc_answer(const char *type, const char *offer,
			     const char *app, const char *stream,
			     const char *host, char **out_answer,
			     char *errbuf, size_t errbuf_sz)
{
	char rtc_url[512];
	webrtc_wait_t *wait;
	pthread_condattr_t cattr;
	struct timespec ts;
	int rc;
	int use_mono = 0;

	if (out_answer)
		*out_answer = NULL;
	if (errbuf && errbuf_sz)
		errbuf[0] = '\0';

	if (!type || !type[0] || !offer || !offer[0] || !app || !app[0] ||
	    !stream || !stream[0] || !out_answer) {
		if (errbuf && errbuf_sz)
			snprintf(errbuf, errbuf_sz, "invalid webrtc args");
		return -1;
	}
	if (!zlm_server_is_ready()) {
		if (errbuf && errbuf_sz)
			snprintf(errbuf, errbuf_sz, "media server not ready");
		return -1;
	}

	/*
	 * Kick video_process to publish before findAsync times out
	 * (maxStreamWaitMS), even when media was unpublished while idle.
	 */
	zlm_server_bump_viewer_demand(ZLM_VIEWER_DEMAND_SEC);
	zlm_server_apply_rtc_extern_ip(host);

	{
		char rtc_host[128];

		copy_host_ip((host && host[0]) ? host : "127.0.0.1", rtc_host,
			     sizeof(rtc_host));
		snprintf(rtc_url, sizeof(rtc_url),
			 "rtc://%s/%s/%s?app=%s&stream=%s&type=%s", rtc_host,
			 app, stream, app, stream, type);
	}

	LOGI("[ZLM] webrtc_answer begin type=%s app=%s stream=%s host=%s offer_len=%zu url=%s",
	     type, app, stream, (host && host[0]) ? host : "127.0.0.1",
	     strlen(offer), rtc_url);

	wait = (webrtc_wait_t *)calloc(1, sizeof(*wait));
	if (!wait) {
		if (errbuf && errbuf_sz)
			snprintf(errbuf, errbuf_sz, "oom");
		return -1;
	}

	pthread_mutex_init(&wait->lock, NULL);
	pthread_condattr_init(&cattr);
#if defined(CLOCK_MONOTONIC)
	if (pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC) == 0)
		use_mono = 1;
#endif
	pthread_cond_init(&wait->cond, &cattr);
	pthread_condattr_destroy(&cattr);

	mk_webrtc_get_answer_sdp(wait, on_webrtc_answer, type, offer, rtc_url);

	/*
	 * Prefer MONOTONIC so settimeofday / NTP step cannot expire the wait
	 * immediately (CLOCK_REALTIME absolute deadlines jump with wall clock).
	 */
	if (use_mono)
		clock_gettime(CLOCK_MONOTONIC, &ts);
	else {
		uint64_t now_us = time_now_us();

		ts.tv_sec = (time_t)(now_us / 1000000ULL);
		ts.tv_nsec = (long)(now_us % 1000000ULL) * 1000L;
	}
	/* > maxStreamWaitMS so late findAsync errors reach the callback. */
	ts.tv_sec += 12;

	pthread_mutex_lock(&wait->lock);
	while (!wait->done) {
		rc = pthread_cond_timedwait(&wait->cond, &wait->lock, &ts);
		if (rc == ETIMEDOUT)
			break;
	}

	/* Unpin so SoftAP/STA role changes can update the default again. */
	s_path_ip[0] = '\0';

	if (!wait->done) {
		/* Late ZLM callback (findAsync can wait ~15s) must free wait. */
		wait->abandoned = 1;
		pthread_mutex_unlock(&wait->lock);
		LOGE("[ZLM] webrtc_answer timeout type=%s stream=%s", type, stream);
		if (errbuf && errbuf_sz)
			snprintf(errbuf, errbuf_sz, "webrtc answer timeout");
		return -1;
	}

	rc = -1;
	if (wait->answer && wait->answer[0] == 'v') {
		*out_answer = wait->answer;
		wait->answer = NULL;
		rc = 0;
		LOGI("[ZLM] webrtc_answer ok type=%s stream=%s answer_len=%zu",
		     type, stream, strlen(*out_answer));
	} else {
		LOGE("[ZLM] webrtc_answer fail type=%s stream=%s err=%s", type, stream,
		     wait->err && wait->err[0] ? wait->err : "empty answer");
		if (errbuf && errbuf_sz) {
			snprintf(errbuf, errbuf_sz, "%s",
				 wait->err && wait->err[0] ? wait->err :
							     "webrtc answer failed");
		}
	}

	pthread_mutex_unlock(&wait->lock);
	webrtc_wait_free(wait);
	return rc;
}
