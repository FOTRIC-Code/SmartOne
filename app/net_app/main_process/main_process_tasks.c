#include "main_process_tasks.h"
#include "alarm_transfer.h"

#include "alarm/photo_album.h"
#include "http_api/config/config_manager.h"
#include "libs/log/log.h"
#include "libs/ntp/ntp_client.h"
#include "libs/platform/command.h"
#include "libs/rtc/rtc_util.h"
#include "libs/time/time_util.h"
#include "net_common/net_common.h"
#include "video_process/video_process.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *const ntp_fallback_servers[] = {
	"cn.ntp.org.cn",
	"ntp.tencent.com",
	"pool.ntp.org",
	"time.windows.com",
	"time.cloudflare.com",
	NULL
};

int main_task_ntp_sync(void)
{
	ntp_info_t *ntp_server = &g_config.ntp_server;
	const char *used_server = NULL;
	time_t ntp_utc;
	int port;

	if (g_config.time_info.time_mode != NET_TIME_MODE_NTP) {
		LOGI("NTP sync: skipped in manual time mode");
		return -1;
	}

	port = ntp_server->port_no > 0 ? ntp_server->port_no : NTP_DEFAULT_PORT;
	LOGI("NTP sync: querying %s:%d (with fallbacks)", ntp_server->host_name,
	     port);

	if (ntp_query_with_fallback(ntp_server->host_name, port,
				    ntp_fallback_servers, &ntp_utc,
				    &used_server) == 0) {
		if (time_set_epoch(ntp_utc) == 0) {
			LOGI("NTP sync: system time updated from %s",
			     used_server);
			rtc_write(ntp_utc);
			return 0;
		}
	}

	LOGW("NTP sync: all servers failed");
	return -1;
}

int main_task_device_restart(void)
{
	LOGI("Device restart scheduled");
	sleep(1);
	sync();
	net_run_command("reboot >/dev/null 2>&1 &");
	return 0;
}

int main_task_alarm_ftp_upload(const alarm_event_t *event)
{
	int result = alarm_ftp_upload(&g_config.ftp_info, event);

	if (result == 0)
		LOGI("alarm_ftp: snapshot uploaded for type=%d", event->type);
	else
		LOGE("alarm_ftp: snapshot upload failed for type=%d",
		     event ? event->type : -1);
	return result;
}

int main_task_alarm_webhook(const alarm_event_t *event)
{
	int result = alarm_webhook_post(&g_config.webhook, event);

	if (result == 0)
		LOGI("alarm_webhook: event posted for type=%d", event->type);
	else
		LOGE("alarm_webhook: POST failed for type=%d",
		     event ? event->type : -1);
	return result;
}

int main_task_alarm_record(const alarm_event_t *event)
{
	if (!event || !event->video_path[0])
		return -1;
	LOGI("alarm_record: %ds for type=%d → %s", event->record_duration_sec,
	     event->type, event->video_path);
	return video_process_record_alarm(event->video_path,
					  event->record_duration_sec);
}

int main_task_body_temp_compose(const alarm_event_t *event)
{
	photo_frame_t photos[PHOTO_ALBUM_CAPACITY];
	video_still_frame_t stills[PHOTO_FRAMES_MAX];
	int count;
	int i;
	int ret;

	if (!event || !event->video_path[0])
		return -1;
	count = photo_album_copy(photos, PHOTO_ALBUM_CAPACITY);
	if (count <= 0) {
		LOGW("body_temp compose: no photos collected yet");
		return -1;
	}
	for (i = 0; i < count; ++i) {
		stills[i].bgr = photos[i].bgr;
		stills[i].width = photos[i].width;
		stills[i].height = photos[i].height;
	}
	ret = video_process_compose_stills_mp4(stills, count,
					       event->video_path);
	photo_album_free_copy(photos, count);
	if (ret == 0)
		LOGI("body_temp compose: %d photos → %s", count,
		     event->video_path);
	else
		LOGE("body_temp compose failed: %s", event->video_path);
	return ret;
}

int main_task_http_rebind(void)
{
	LOGI("http_rebind: port cfg changed (http=%d rtsp=%d rtp=%d) — restart net_app to apply",
	     g_config.port_info.http_port_no, g_config.port_info.rtsp_port_no,
	     g_config.port_info.rtp_port_no);
	return 0;
}
