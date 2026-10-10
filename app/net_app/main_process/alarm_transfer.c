#include "alarm_transfer.h"

#include "thermal/thermal_data_provider.h"
#include "libs/ftp/ftp_client.h"
#include "libs/log/log.h"
#include "libs/time/time_util.h"
#include "net_common/net_common.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int alarm_transfer_init(void)
{
	return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1;
}

void alarm_transfer_uninit(void)
{
	curl_global_cleanup();
}

static void make_ftp_config(const ftp_info_t *source,
			    ftp_conn_cfg_t *target)
{
	memset(target, 0, sizeof(*target));
	snprintf(target->ip, sizeof(target->ip), "%s", source->ip);
	target->port = source->port_no;
	snprintf(target->user, sizeof(target->user), "%s",
		 source->user_name);
	snprintf(target->pwd, sizeof(target->pwd), "%s", source->pwd);
	target->use_ssl = source->use_ssl;
	target->enable_annoy = source->enable_annoy;
}

static void expand_remote_path(const ftp_info_t *config,
			       const alarm_event_t *event, char *out,
			       size_t out_size)
{
	char stamp[32];
	char stamp_ms[32];
	char template_path[NET_LEN_128];
	char *token;
	uint64_t us = event && event->timestamp_us ? event->timestamp_us :
						     time_now_us();

	if (time_format_local_us(us, TIME_FMT_COMPACT, stamp,
				 sizeof(stamp)) < 0)
		stamp[0] = '\0';
	snprintf(stamp_ms, sizeof(stamp_ms), "%s%03u", stamp,
		 time_millis_of_us(us));
	snprintf(template_path, sizeof(template_path), "%s",
		 config->path_template[0] ? config->path_template : "alarm.jpg");
	token = strstr(template_path, "{yyyyMMddHHmmssfff}");
	if (!token)
		token = strstr(template_path, "{yyyyMMddHHmmss}");
	if (token) {
		char suffix[NET_LEN_128];
		size_t token_len =
			strncmp(token, "{yyyyMMddHHmmssfff}", 19) == 0 ? 19 :
									 16;

		snprintf(suffix, sizeof(suffix), "%s", token + token_len);
		*token = '\0';
		snprintf(out, out_size, "%s%s%s", template_path,
			 token_len == 19 ? stamp_ms : stamp, suffix);
	} else {
		snprintf(out, out_size, "%s", template_path);
	}
}

int alarm_ftp_test(const ftp_info_t *config)
{
	ftp_conn_cfg_t ftp;

	if (!config || !config->ip[0])
		return -1;
	make_ftp_config(config, &ftp);
	return ftp_test_login(&ftp);
}

int alarm_ftp_upload(const ftp_info_t *config, const alarm_event_t *event)
{
	ftp_conn_cfg_t ftp;
	thermal_snapshot_t snapshot = { 0 };
	char remote_path[NET_LEN_128];
	int result = -1;

	if (!config || !event || !config->ip[0])
		return -1;
	if (thermal_provider_capture_jpeg(&snapshot) != 0) {
		LOGE("alarm FTP snapshot unavailable");
		return -1;
	}
	expand_remote_path(config, event, remote_path, sizeof(remote_path));
	make_ftp_config(config, &ftp);
	result = ftp_upload_buffer(&ftp, snapshot.data, snapshot.size,
				   remote_path);
	thermal_provider_release_snapshot(&snapshot);
	return result;
}

static int webhook_post_json(const char *url, const char *body)
{
	CURL *curl;
	CURLcode code;
	struct curl_slist *headers = NULL;
	long status = 0;

	curl = curl_easy_init();
	if (!curl)
		return -1;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
	code = curl_easy_perform(curl);
	if (code == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return code == CURLE_OK && status >= 200 && status < 300 ? 0 : -1;
}

int alarm_webhook_post(const webhook_info_t *config,
		       const alarm_event_t *event)
{
	char body[512];
	const char *is_alarm_field;

	if (!config || !event || !config->url[0])
		return -1;

	is_alarm_field = event->type == NET_ALARM_BODY_TEMP ?
				 ",\"is_alarm\":true" : "";
	snprintf(body, sizeof(body),
		 "{\"type\":%d,\"alarm_info\":{\"timestamp_us\":%llu,"
		 "\"motion_level\":%d,\"temperature\":%.3f,"
		 "\"threshold\":%.3f,\"baseline\":%.3f,\"slope\":%.6f,"
		 "\"message\":\"%s\",\"video_path\":\"%s\"%s}}",
		 event->type, (unsigned long long)event->timestamp_us,
		 event->motion_level, event->temperature, event->threshold,
		 event->baseline, event->slope, event->message,
		 event->video_path, is_alarm_field);
	return webhook_post_json(config->url, body);
}

int body_temp_sample_webhook_post(const webhook_info_t *config,
				  float temperature, uint64_t timestamp_us)
{
	char body[256];

	if (!config || !config->url[0])
		return -1;
	snprintf(body, sizeof(body),
		 "{\"type\":%d,\"alarm_info\":{\"timestamp_us\":%llu,"
		 "\"temperature\":%.3f,\"is_alarm\":false}}",
		 NET_ALARM_BODY_TEMP,
		 (unsigned long long)timestamp_us, temperature);
	return webhook_post_json(config->url, body);
}
