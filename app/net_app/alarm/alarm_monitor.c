#include "alarm_monitor.h"
#include "body_temp_history.h"
#include "intrusion_motion.h"
#include "photo_album.h"
#include "thermal/thermal_data_provider.h"

#include "libs/log/log.h"
#include "libs/time/time_util.h"
#include "main_process/main_process.h"
#include "net_common/net_common.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_t s_thread;
static volatile int s_running;
static pthread_mutex_t s_wait_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_wait_cond = PTHREAD_COND_INITIALIZER;

static time_t s_last_intrusion;
static time_t s_last_high_temp;

static body_temp_history_t s_body_temp_history;
static body_temp_minute_t s_body_temp_minute;

static int cooldown_ready(time_t last, int no_repeat_minutes)
{
	time_t now = time_now_sec();

	if (no_repeat_minutes <= 0)
		return 1;
	return last == 0 || (now - last) >= no_repeat_minutes * 60;
}

static int publish_event(alarm_event_t *event)
{
	if (!event)
		return -1;
	return main_process_submit_alarm_event(event);
}

/* --- Alarm check routines --- */

static void check_intrusion(void)
{
	const intrusion_alarm_t *cfg = &g_config.intrusion_alarm;
	alarm_event_t event;
	int motion = 0;
	int threshold;

	if (!cfg->enabled) {
		intrusion_motion_reset();
		return;
	}
	switch (intrusion_motion_sample(&motion)) {
	case INTRUSION_MOTION_UNAVAILABLE:
	case INTRUSION_MOTION_BASELINE:
		return;
	default:
		break;
	}

	switch (cfg->sensitivity) {
	case NET_ALARM_SENSITIVITY_LOW:
		threshold = 70;
		break;
	case NET_ALARM_SENSITIVITY_HIGH:
		threshold = 30;
		break;
	default:
		threshold = 50;
		break;
	}

	if (motion < threshold)
		return;
	if (!cooldown_ready(s_last_intrusion, cfg->no_repeat_minutes))
		return;

	memset(&event, 0, sizeof(event));
	event.type = NET_ALARM_INTRUSION;
	event.timestamp_us = time_now_us();
	event.motion_level = motion;
	event.record_duration_sec = cfg->record_duration_sec;
	snprintf(event.message, sizeof(event.message), "intrusion alarm");
	if (publish_event(&event) == 0)
		s_last_intrusion = time_now_sec();
	LOGI("Intrusion alarm triggered (motion=%d)", motion);
}

static void check_high_temp(void)
{
	const high_temp_alarm_t *cfg = &g_config.high_temp_alarm;
	alarm_event_t event;
	float temp = 0;

	if (!cfg->enabled)
		return;
	if (thermal_get_global_max_temp(&temp) != 0)
		return;
	if (temp < cfg->alarm_temp)
		return;
	if (!cooldown_ready(s_last_high_temp, cfg->no_repeat_minutes))
		return;

	memset(&event, 0, sizeof(event));
	event.type = NET_ALARM_HIGH_TEMP;
	event.timestamp_us = time_now_us();
	event.temperature = temp;
	event.threshold = cfg->alarm_temp;
	event.record_duration_sec = cfg->record_duration_sec;
	snprintf(event.message, sizeof(event.message), "high temperature alarm");
	if (publish_event(&event) == 0)
		s_last_high_temp = time_now_sec();
	LOGI("High temp alarm: %.1f >= %.1f", temp, cfg->alarm_temp);
}

/*
 * Contract (Volga api.md): sensitivity 0/1/2 maps to slope thresholds
 * 0.10/0.05/0.02 °C/min; the alarm fires on |slope| reaching the threshold,
 * so both rising and falling temperature trigger.
 */
static float body_temp_slope_threshold(int sensitivity)
{
	switch (sensitivity) {
	case NET_ALARM_SENSITIVITY_LOW:
		return 0.10f;
	case NET_ALARM_SENSITIVITY_HIGH:
		return 0.02f;
	default:
		return 0.05f;
	}
}

/* One photo per minute while monitoring; the album keeps the latest 10. */
static void body_temp_take_photo(void)
{
	static unsigned char *s_photo_buf;
	static size_t s_photo_cap;
	thermal_preview_frame_t preview = { 0 };
	size_t need;

	if (thermal_provider_copy_preview_bgr(&preview) != 0 ||
	    preview.width <= 0 || preview.height <= 0)
		return;
	need = (size_t)preview.width * (size_t)preview.height * 3U;
	if (s_photo_cap < need) {
		unsigned char *buf = realloc(s_photo_buf, need);

		if (!buf)
			return;
		s_photo_buf = buf;
		s_photo_cap = need;
	}
	preview.data = s_photo_buf;
	preview.capacity = s_photo_cap;
	if (thermal_provider_copy_preview_bgr(&preview) == 0)
		photo_album_add(preview.data, preview.width, preview.height);
}

/* Runs every second: one frame-max reading into the current minute. */
static void sample_body_temp(void)
{
	static int s_was_enabled;
	float temp = 0;

	if (!g_config.body_temp_alarm.enabled) {
		if (s_was_enabled) {
			body_temp_minute_reset(&s_body_temp_minute);
			body_temp_history_reset(&s_body_temp_history);
			photo_album_reset();
			s_was_enabled = 0;
		}
		return;
	}
	s_was_enabled = 1;
	if (thermal_get_global_max_temp(&temp) == 0)
		body_temp_minute_add(&s_body_temp_minute, temp);
}

static void check_body_temp(void)
{
	const body_temp_alarm_t *cfg = &g_config.body_temp_alarm;
	alarm_event_t event;
	float temp = 0;
	float baseline = 0;
	float slope = 0;
	float threshold;
	int readings;
	uint64_t ts;

	if (!cfg->enabled)
		return;
	readings = s_body_temp_minute.count;
	if (body_temp_minute_median(&s_body_temp_minute, &temp) != 0) {
		body_temp_minute_reset(&s_body_temp_minute);
		LOGW("Body temp minute skipped: %d valid readings (< %d)",
		     readings, BODY_TEMP_MINUTE_MIN_VALID);
		return;
	}
	body_temp_minute_reset(&s_body_temp_minute);

	body_temp_take_photo();
	ts = time_now_us();
	body_temp_history_add(&s_body_temp_history, temp, ts);
	main_process_submit_body_temp_sample(temp, ts);

	if (s_body_temp_history.count < BODY_TEMP_HISTORY_CAPACITY)
		return;

	/*
	 * One round (10 minutes) complete: seal the photo round before
	 * evaluation so the linkage worker composites exactly these photos,
	 * then reset the sample window — the next minute starts a fresh round.
	 */
	photo_album_finish_round();
	if (body_temp_history_trend(&s_body_temp_history, &baseline, &slope) == 0) {
		threshold = body_temp_slope_threshold(cfg->sensitivity);
		if (fabsf(slope) >= threshold) {
			memset(&event, 0, sizeof(event));
			event.type = NET_ALARM_BODY_TEMP;
			event.timestamp_us = time_now_us();
			event.temperature = temp;
			event.baseline = baseline;
			event.slope = slope;
			snprintf(event.message, sizeof(event.message),
				 "body temperature alarm");
			publish_event(&event);
			LOGI("Body temp alarm: temp=%.2f (%d readings) baseline=%.2f slope=%.4f (|slope| >= %.2f)",
			     temp, readings, baseline, slope, threshold);
		} else {
			LOGI("Body temp round done: temp=%.2f (%d readings) baseline=%.2f slope=%.4f (below %.2f)",
			     temp, readings, baseline, slope, threshold);
		}
	}
	body_temp_history_reset(&s_body_temp_history);
}

static void *alarm_thread(void *arg)
{
	int body_temp_counter = 0;

	(void)arg;
	LOGI("Alarm monitor thread started");

	while (s_running) {
		struct timespec wake;

		check_intrusion();
		check_high_temp();
		sample_body_temp();

		/*
		 * Body temp reads every second and evaluates the minute median
		 * once per minute (60 iterations x 1s sleep).
		 */
		body_temp_counter++;
		if (body_temp_counter >= 60) {
			check_body_temp();
			body_temp_counter = 0;
		}

		{
			uint64_t wake_us = time_now_us() + 1000000ULL;

			wake.tv_sec = (time_t)(wake_us / 1000000ULL);
			wake.tv_nsec = (long)(wake_us % 1000000ULL) * 1000L;
		}
		pthread_mutex_lock(&s_wait_lock);
		if (s_running)
			pthread_cond_timedwait(&s_wait_cond, &s_wait_lock, &wake);
		pthread_mutex_unlock(&s_wait_lock);
	}

	LOGI("Alarm monitor thread stopped");
	return NULL;
}

int alarm_monitor_start(void)
{
	if (s_running)
		return 0;

	s_running = 1;
	s_last_intrusion = 0;
	s_last_high_temp = 0;
	intrusion_motion_reset();
	body_temp_minute_reset(&s_body_temp_minute);
	body_temp_history_reset(&s_body_temp_history);
	photo_album_reset();

	if (pthread_create(&s_thread, NULL, alarm_thread, NULL) != 0) {
		LOGE("Failed to create alarm monitor thread");
		s_running = 0;
		return -1;
	}
	LOGI("Alarm monitor started");
	return 0;
}

void alarm_monitor_stop(void)
{
	if (!s_running)
		return;
	pthread_mutex_lock(&s_wait_lock);
	s_running = 0;
	pthread_cond_signal(&s_wait_cond);
	pthread_mutex_unlock(&s_wait_lock);
	pthread_join(s_thread, NULL);
	LOGI("Alarm monitor stopped");
}
