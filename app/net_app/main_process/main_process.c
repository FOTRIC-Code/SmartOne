#include "main_process.h"

#include "alarm_transfer.h"
#include "main_process_tasks.h"
#include "http_api/config/config_manager.h"
#include "libs/log/log.h"
#include "libs/time/time_util.h"
#include "net_common/net_common.h"
#include "rtp/rtp_server.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ALARM_EVENT_QUEUE_SIZE 16
#define ALARM_TRANSFER_JOB_LIMIT 16

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_alarm_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t s_job_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t s_task_cond = PTHREAD_COND_INITIALIZER;
static unsigned s_pending;
static time_t s_last_ntp;
static int s_inited;
static pthread_t s_alarm_worker;
static pthread_t s_task_thread;
static int s_task_thread_started;
static alarm_event_t s_alarm_queue[ALARM_EVENT_QUEUE_SIZE];
static int s_alarm_head;
static int s_alarm_count;
static int s_transfer_jobs;

typedef struct linkage_job {
	alarm_event_t event;
	ftp_info_cfg ftp;
	webhook_info_cfg webhook;
	int send_ftp;
	int send_webhook;
} linkage_job_t;

static void *linkage_job_main(void *opaque)
{
	linkage_job_t *job = opaque;

	if (job->send_ftp)
		alarm_ftp_upload(&job->ftp, &job->event);
	if (job->send_webhook)
		alarm_webhook_post(&job->webhook, &job->event);
	free(job);

	pthread_mutex_lock(&s_lock);
	s_transfer_jobs--;
	pthread_cond_broadcast(&s_job_cond);
	pthread_mutex_unlock(&s_lock);
	return NULL;
}

static void submit_linkage_job(const alarm_event_t *event,
			       const alarm_trigger_cfg *trigger,
			       const ftp_info_cfg *ftp,
			       const webhook_info_cfg *webhook)
{
	linkage_job_t *job;
	pthread_t thread;

	if (!event || !trigger || !ftp || !webhook)
		return;
	if (!trigger->trigger_ftp_enabled &&
	    !trigger->trigger_webhook_enabled)
		return;
	job = calloc(1, sizeof(*job));
	if (!job)
		return;
	job->event = *event;
	job->ftp = *ftp;
	job->webhook = *webhook;
	job->send_ftp = trigger->trigger_ftp_enabled;
	job->send_webhook = trigger->trigger_webhook_enabled;

	pthread_mutex_lock(&s_lock);
	if (s_transfer_jobs >= ALARM_TRANSFER_JOB_LIMIT) {
		pthread_mutex_unlock(&s_lock);
		LOGW("Alarm transfer job limit reached");
		free(job);
		return;
	}
	s_transfer_jobs++;
	pthread_mutex_unlock(&s_lock);

	if (pthread_create(&thread, NULL, linkage_job_main, job) != 0) {
		pthread_mutex_lock(&s_lock);
		s_transfer_jobs--;
		pthread_cond_broadcast(&s_job_cond);
		pthread_mutex_unlock(&s_lock);
		free(job);
		return;
	}
	pthread_detach(thread);
}

static void *alarm_worker_main(void *unused)
{
	(void)unused;
	for (;;) {
		alarm_event_t event;
		alarm_trigger_cfg trigger;
		ftp_info_cfg ftp;
		webhook_info_cfg webhook;

		pthread_mutex_lock(&s_lock);
		while (s_inited && s_alarm_count == 0)
			pthread_cond_wait(&s_alarm_cond, &s_lock);
		if (!s_inited && s_alarm_count == 0) {
			pthread_mutex_unlock(&s_lock);
			break;
		}
		event = s_alarm_queue[s_alarm_head];
		s_alarm_head = (s_alarm_head + 1) % ALARM_EVENT_QUEUE_SIZE;
		s_alarm_count--;
		pthread_mutex_unlock(&s_lock);

		if (config_copy_alarm_linkage(event.type, &trigger, &ftp,
					      &webhook) != 0)
			continue;
		if (trigger.trigger_record_enabled && event.video_path[0]) {
			if (event.type == NET_ALARM_BODY_TEMP)
				main_task_body_temp_compose(&event);
			else
				main_task_alarm_record(&event);
		}
		submit_linkage_job(&event, &trigger, &ftp, &webhook);
	}
	return NULL;
}

static const char *alarm_type_name(int type)
{
	switch (type) {
	case NET_ALARM_INTRUSION:
		return "intrusion";
	case NET_ALARM_HIGH_TEMP:
		return "high_temp";
	case NET_ALARM_BODY_TEMP:
		return "body_temp";
	default:
		return "alarm";
	}
}

/*
 * The record path must be fixed before the event is published: RTP/webhook
 * receivers read video_path immediately, while the recording itself runs
 * later in the linkage worker.
 */
static void alarm_plan_record_path(alarm_event_t *event)
{
	const alarm_trigger_cfg *trigger;
	char stamp[32];
	uint64_t us;

	trigger = config_get_alarm_trigger(event->type);
	if (!trigger || !trigger->trigger_record_enabled)
		return;
	if (!net_record_dir_ready()) {
		LOGW("alarm record skipped: %s not backed by a usable SD card",
		     NET_ALARM_RECORD_DIR);
		return;
	}
	us = event->timestamp_us ? event->timestamp_us : time_now_us();
	if (time_format_local_us(us, TIME_FMT_FILE, stamp, sizeof(stamp)) < 0)
		return;
	snprintf(event->video_path, sizeof(event->video_path),
		 "%s/%s_%s.mp4", NET_ALARM_RECORD_DIR,
		 alarm_type_name(event->type), stamp);
}

int main_process_submit_alarm_event(const alarm_event_t *event)
{
	alarm_event_t enriched;
	int tail;

	if (!event)
		return -1;
	enriched = *event;
	alarm_plan_record_path(&enriched);
	pthread_mutex_lock(&s_lock);
	if (!s_inited || s_alarm_count == ALARM_EVENT_QUEUE_SIZE) {
		pthread_mutex_unlock(&s_lock);
		LOGW("Alarm linkage queue unavailable or full");
		return -1;
	}
	tail = (s_alarm_head + s_alarm_count) % ALARM_EVENT_QUEUE_SIZE;
	s_alarm_queue[tail] = enriched;
	s_alarm_count++;
	pthread_cond_signal(&s_alarm_cond);
	pthread_mutex_unlock(&s_lock);
	rtp_push_alarm_event(&enriched);
	return 0;
}

typedef struct sample_job {
	webhook_info_cfg webhook;
	float temperature;
	uint64_t timestamp_us;
} sample_job_t;

static void *sample_job_main(void *opaque)
{
	sample_job_t *job = opaque;

	body_temp_sample_webhook_post(&job->webhook, job->temperature,
				      job->timestamp_us);
	free(job);

	pthread_mutex_lock(&s_lock);
	s_transfer_jobs--;
	pthread_cond_broadcast(&s_job_cond);
	pthread_mutex_unlock(&s_lock);
	return NULL;
}

void main_process_submit_body_temp_sample(float temperature,
					  uint64_t timestamp_us)
{
	alarm_trigger_cfg trigger;
	ftp_info_cfg ftp;
	webhook_info_cfg webhook;
	sample_job_t *job;
	pthread_t thread;

	if (config_copy_alarm_linkage(NET_ALARM_BODY_TEMP, &trigger, &ftp,
				      &webhook) != 0)
		return;
	if (!trigger.trigger_webhook_enabled || !webhook.url[0])
		return;

	job = calloc(1, sizeof(*job));
	if (!job)
		return;
	job->webhook = webhook;
	job->temperature = temperature;
	job->timestamp_us = timestamp_us;

	pthread_mutex_lock(&s_lock);
	if (s_transfer_jobs >= ALARM_TRANSFER_JOB_LIMIT) {
		pthread_mutex_unlock(&s_lock);
		LOGW("Transfer job limit reached, dropping body temp sample");
		free(job);
		return;
	}
	s_transfer_jobs++;
	pthread_mutex_unlock(&s_lock);

	if (pthread_create(&thread, NULL, sample_job_main, job) != 0) {
		pthread_mutex_lock(&s_lock);
		s_transfer_jobs--;
		pthread_cond_broadcast(&s_job_cond);
		pthread_mutex_unlock(&s_lock);
		free(job);
		return;
	}
	pthread_detach(thread);
}

void main_process_request_task(net_enum_task_type_t task)
{
	pthread_mutex_lock(&s_lock);
	s_pending |= (unsigned)task;
	pthread_cond_signal(&s_task_cond);
	pthread_mutex_unlock(&s_lock);
}

static void main_process_run_pending(void)
{
	unsigned pending = 0;
	time_t now;
	int interval_sec;

	pthread_mutex_lock(&s_lock);
	pending = s_pending;
	s_pending = 0;
	pthread_mutex_unlock(&s_lock);

	if (pending & NET_TASK_NTP_SYNC)
		main_task_ntp_sync();
	if (pending & NET_TASK_DEVICE_RESTART)
		main_task_device_restart();
	if (pending & NET_TASK_HTTP_REBIND)
		main_task_http_rebind();

	now = time_now_sec();
	interval_sec = g_config.ntp_server.syn_interval > 0 ?
			       g_config.ntp_server.syn_interval * 60 :
			       86400;
	if (g_config.time_info.time_mode == NET_TIME_MODE_NTP &&
	    (s_last_ntp == 0 || now - s_last_ntp >= interval_sec)) {
		main_task_ntp_sync();
		s_last_ntp = now;
	}
}

static void *task_thread_main(void *opaque)
{
	(void)opaque;
	pthread_mutex_lock(&s_lock);
	while (s_inited) {
		if (s_pending == 0) {
			struct timespec wake;

			clock_gettime(CLOCK_REALTIME, &wake);
			wake.tv_sec += 1;
			pthread_cond_timedwait(&s_task_cond, &s_lock, &wake);
			if (!s_inited)
				break;
		}
		pthread_mutex_unlock(&s_lock);
		main_process_run_pending();
		pthread_mutex_lock(&s_lock);
	}
	pthread_mutex_unlock(&s_lock);
	return NULL;
}

int main_process_init(void)
{
	s_pending = 0;
	s_last_ntp = 0;
	s_alarm_head = 0;
	s_alarm_count = 0;
	s_transfer_jobs = 0;
	s_inited = 1;
	if (alarm_transfer_init() != 0) {
		s_inited = 0;
		LOGE("Failed to initialize alarm transfer client");
		return -1;
	}
	if (pthread_create(&s_alarm_worker, NULL, alarm_worker_main, NULL) != 0) {
		s_inited = 0;
		alarm_transfer_uninit();
		LOGE("Failed to start alarm linkage worker");
		return -1;
	}
	if (pthread_create(&s_task_thread, NULL, task_thread_main, NULL) != 0) {
		pthread_mutex_lock(&s_lock);
		s_inited = 0;
		pthread_cond_signal(&s_alarm_cond);
		pthread_mutex_unlock(&s_lock);
		pthread_join(s_alarm_worker, NULL);
		alarm_transfer_uninit();
		LOGE("Failed to start main process task thread");
		return -1;
	}
	s_task_thread_started = 1;
	LOGI("main_process_init");
	return 0;
}

void main_process_uninit(void)
{
	pthread_mutex_lock(&s_lock);
	s_inited = 0;
	pthread_cond_signal(&s_alarm_cond);
	pthread_cond_signal(&s_task_cond);
	pthread_mutex_unlock(&s_lock);
	if (s_task_thread_started) {
		pthread_join(s_task_thread, NULL);
		s_task_thread_started = 0;
	}
	pthread_join(s_alarm_worker, NULL);
	pthread_mutex_lock(&s_lock);
	while (s_transfer_jobs > 0)
		pthread_cond_wait(&s_job_cond, &s_lock);
	pthread_mutex_unlock(&s_lock);
	alarm_transfer_uninit();
}
