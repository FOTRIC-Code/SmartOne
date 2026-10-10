#include "http_api.h"

#include "http_api_common.h"
#include "http_api_routes.h"
#include "http_auth.h"
#include "libs/file/file_util.h"
#include "libs/json/json_util.h"
#include "libs/log/log.h"
#include "libs/mongoose/mongoose.h"
#include "net_common/net_common.h"
#include "video_process/video_process.h"

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *static_page_name(const char *path)
{
	const char *slash;

	if (!path || !path[0])
		return "index.html";
	slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static int is_legacy_setup_page(const char *page)
{
	return page && (strcmp(page, "provision.html") == 0 ||
			strcmp(page, "admin.html") == 0 ||
			strcmp(page, "upgrade.html") == 0);
}

/* Returns 1 if request was handled (redirect/forbidden). */
static int http_web_access_gate(struct mg_connection *c,
				struct mg_http_message *hm,
				const char *path)
{
	const char *page = static_page_name(path);

	(void)hm;

	/*
	 * SoftAP is a normal hotspot for local use — SPA is always available.
	 * Wi-Fi station provisioning is a settings module, not a separate site.
	 * Legacy static pages redirect into the SPA.
	 */
	if (is_legacy_setup_page(page)) {
		mg_http_reply(c, 302,
			      "Location: /#/home\r\n"
			      "Cache-Control: no-store\r\n",
			      "");
		return 1;
	}

	return 0;
}

static struct mg_mgr s_mgr;
static int s_started;
static int s_polling;
static pthread_t s_thread;
static int s_thread_started;

static void strip_leading_slash(char *path)
{
	if (path[0] == '/')
		memmove(path, path + 1, strlen(path));
}

static int parse_trailing_id(const char *path, const char *prefix, int *id)
{
	size_t plen = strlen(prefix);
	char extra[64];

	if (strncmp(path, prefix, plen) != 0)
		return -1;
	if (path[plen] == '\0')
		return 1; /* prefix only */
	if (path[plen] != '/')
		return -1;
	if (sscanf(path + plen + 1, "%d%63s", id, extra) == 1)
		return 0;
	if (sscanf(path + plen + 1, "%d/%63s", id, extra) == 2)
		return 0;
	return -1;
}

static int parse_trailing_segment(const char *path, const char *prefix,
				  char *segment, size_t segment_size)
{
	size_t plen = strlen(prefix);
	const char *value;
	size_t len;

	if (!path || !prefix || !segment || segment_size == 0)
		return -1;
	if (strncmp(path, prefix, plen) != 0)
		return -1;
	if (path[plen] == '\0')
		return 1;
	if (path[plen] != '/')
		return -1;

	value = path + plen + 1;
	if (!value[0])
		return 1;
	len = strcspn(value, "/");
	if (value[len] != '\0' || len >= segment_size)
		return -1;
	memcpy(segment, value, len);
	segment[len] = '\0';
	return 0;
}

static int parse_alarm_type_token(const char *token, int *type)
{
	int id;
	char extra[8];

	if (!token || !token[0] || !type)
		return -1;
	if (sscanf(token, "%d%7s", &id, extra) == 1) {
		if (id >= NET_PARKIA_ALARM_TYPE_FIRST &&
		    id <= NET_PARKIA_ALARM_TYPE_LAST) {
			*type = id;
			return 0;
		}
		return -1;
	}
	return -1;
}

static const char *s_cors_headers =
	"Content-Type: application/json\r\n"
	"Access-Control-Allow-Origin: *\r\n"
	"Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
	"Access-Control-Allow-Headers: Content-Type, Authorization\r\n";

static void reply_json(struct mg_connection *c, char *response)
{
	mg_http_reply(c, 200, s_cors_headers, "%s",
		      response ? response : "{\"code\":-1,\"message\":\"empty\"}");
	free(response);
}

static int method_is(struct mg_http_message *hm, const char *m)
{
	return mg_strcmp(hm->method, mg_str(m)) == 0;
}

/* Holly-style: empty path or short file extension → static under /var/www. */
static int looks_like_api(const char *path)
{
	if (!path || !path[0])
		return 0;
	if (strncmp(path, "system/", 7) == 0 ||
	    strncmp(path, "thermal/", 8) == 0 ||
	    strncmp(path, "alarm/", 6) == 0 ||
	    strncmp(path, "image/", 6) == 0 ||
	    strncmp(path, "streaming/", 10) == 0)
		return 1;
	return 0;
}

static int is_static_request(const char *path)
{
	size_t len;
	size_t i;

	if (!path || path[0] == '\0')
		return 1;
	if (looks_like_api(path))
		return 0;

	len = strlen(path);
	for (i = len; i > 0; i--) {
		if (path[i - 1] == '.') {
			size_t ext_len = len - i;

			return ext_len > 0 && ext_len <= 5;
		}
		if (path[i - 1] == '/')
			break;
	}
	return 0;
}

static void serve_www(struct mg_connection *c, struct mg_http_message *hm)
{
	struct mg_http_serve_opts opts;

	memset(&opts, 0, sizeof(opts));
	opts.root_dir = NET_WEB_ROOT;
	/*
	 * no-cache, not no-store: the bundle is unhashed so it must be
	 * revalidated, but no-store made every SoftAP page refresh re-download
	 * 1.6 MB. Revalidation hits the Etag and returns 304.
	 */
	opts.extra_headers = "Cache-Control: no-cache\r\n"
			     "Access-Control-Allow-Origin: *\r\n";
	mg_http_serve_dir(c, hm, &opts);
}

/* image/channels/{id}/{sub} → 0 and fills id/sub. */
static int parse_image_channel(const char *path, int *img_id, char *sub,
			       size_t sub_sz)
{
	if (!path || !img_id || !sub || sub_sz == 0)
		return -1;
	if (strncmp(path, ROUTE_IMAGE_CHANNELS "/",
		    strlen(ROUTE_IMAGE_CHANNELS "/")) != 0)
		return -1;
	if (sscanf(path + strlen(ROUTE_IMAGE_CHANNELS "/"), "%d/%31s", img_id,
		   sub) != 2)
		return -1;
	if (strlen(sub) >= sub_sz)
		return -1;
	return 0;
}

static int parse_alarm_route(const char *path, int *type)
{
	char alarm_type[32];

	if (parse_trailing_segment(path, ROUTE_ALARM_TRIGGERS, alarm_type,
				   sizeof(alarm_type)) != 0)
		return -1;
	return parse_alarm_type_token(alarm_type, type);
}

/*
 * Holly-style: one route table per method. The listener picks the table
 * once; handlers do not re-check the verb.
 */
static char *http_get_dispatch(const char *path)
{
	char *response = NULL;
	char sub[32] = { 0 };
	int id = 0;

	if (strcmp(path, ROUTE_SYSTEM_DEVICE_INFO) == 0)
		response = handle_system_device_info_get();
	else if (strcmp(path, ROUTE_SYSTEM_TIME) == 0)
		response = handle_system_time_get();
	else if (strcmp(path, ROUTE_SYSTEM_NTP_SERVER) == 0)
		response = handle_system_ntp_server_get();
	else if (strcmp(path, ROUTE_SYSTEM_NETWORKS) == 0)
		response = handle_system_networks_get();
	else if (parse_trailing_id(path, ROUTE_SYSTEM_NETWORKS, &id) == 0)
		response = handle_system_network_get(id);
	else if (strcmp(path, ROUTE_SYSTEM_STATUS) == 0)
		response = handle_system_status_get();
	else if (strcmp(path, ROUTE_SYSTEM_PORT) == 0)
		response = handle_system_port_get();
	else if (strcmp(path, ROUTE_SYSTEM_FTP) == 0)
		response = handle_system_ftp_get();
	else if (strcmp(path, ROUTE_SYSTEM_WEBHOOK) == 0)
		response = handle_system_webhook_get();
	else if (strcmp(path, ROUTE_THERMAL_INTRUSION) == 0)
		response = handle_intrusion_alarm_get();
	else if (strcmp(path, ROUTE_THERMAL_HIGH_TEMP) == 0)
		response = handle_high_temp_alarm_get();
	else if (strcmp(path, ROUTE_THERMAL_BODY_TEMP) == 0)
		response = handle_body_temp_alarm_get();
	else if (strcmp(path, ROUTE_THERMAL_SHUTTER_MODE) == 0)
		response = handle_thermal_shutter_mode_get();
	else if (strcmp(path, ROUTE_THERMAL_SHUTTER_TIME) == 0)
		response = handle_thermal_shutter_time_get();
	else if (strcmp(path, ROUTE_THERMAL_RANGE) == 0)
		response = handle_thermal_range_get();
	else if (strcmp(path, ROUTE_THERMAL_PARAMS) == 0)
		response = handle_thermal_params_get();
	else if (strcmp(path, ROUTE_THERMAL_UNIT) == 0)
		response = handle_thermal_unit_get();
	else if (strcmp(path, ROUTE_THERMAL_LENS_INFO) == 0)
		response = handle_thermal_lens_info_get();
	else if (strcmp(path, ROUTE_THERMAL_COLOR_DIST) == 0)
		response = handle_thermal_color_dist_get();
	else if (strcmp(path, ROUTE_THERMAL_PALETTE) == 0)
		response = handle_thermal_palette_get();
	else if (strcmp(path, ROUTE_THERMAL_PRESET_PALETTES) == 0)
		response = handle_thermal_palettes_get();
	else if (strcmp(path, ROUTE_THERMAL_GAIN) == 0)
		response = handle_thermal_gain_get();
	else if (parse_image_channel(path, &id, sub, sizeof(sub)) == 0) {
		if (strcmp(sub, "image_flip") == 0)
			response = handle_image_flip_get(id);
		else if (strcmp(sub, "image_params") == 0)
			response = handle_image_params_get(id);
	} else if (parse_alarm_route(path, &id) == 0)
		response = handle_alarm_trigger_get(id);
	else if (strcmp(path, ROUTE_STREAMING_CHANNELS) == 0)
		response = handle_streaming_channels_get();
	else if (parse_trailing_id(path, ROUTE_STREAMING_CHANNELS, &id) == 0) {
		if (strstr(path, "/status") != NULL)
			response = handle_streaming_channel_status_get(id);
		else
			response = handle_streaming_channel_get(id);
	}

	if (!response)
		response = create_error(EC_FAIL, "Not found");
	return response;
}

static char *http_put_dispatch(const char *path, const char *body, int len)
{
	char *response = NULL;
	char sub[32] = { 0 };
	int id = 0;

	if (strcmp(path, ROUTE_SYSTEM_DEVICE_INFO) == 0)
		response = handle_system_device_info_put(body, len);
	else if (strcmp(path, ROUTE_SYSTEM_DEVICE_RESTART) == 0)
		response = handle_system_restart();
	else if (strcmp(path, ROUTE_SYSTEM_DEVICE_RESET) == 0)
		response = handle_system_reset();
	else if (strcmp(path, ROUTE_SYSTEM_TIME) == 0)
		response = handle_system_time_put(body, len);
	else if (strcmp(path, ROUTE_SYSTEM_NTP_SERVER) == 0)
		response = handle_system_ntp_server_put(body, len);
	else if (strcmp(path, ROUTE_SYSTEM_NETWORKS) == 0)
		response = handle_system_networks_put(body, len);
	else if (parse_trailing_id(path, ROUTE_SYSTEM_NETWORKS, &id) == 0)
		response = handle_system_network_put(id, body, len);
	else if (strcmp(path, ROUTE_SYSTEM_PORT) == 0)
		response = handle_system_port_put(body, len);
	else if (strcmp(path, ROUTE_SYSTEM_FTP) == 0)
		response = handle_system_ftp_put(body, len);
	else if (strcmp(path, ROUTE_SYSTEM_WEBHOOK) == 0)
		response = handle_system_webhook_put(body, len);
	else if (strcmp(path, ROUTE_DISCOVERY_RECOVER_DHCP) == 0)
		response = handle_discovery_recover_dhcp();
	else if (strcmp(path, ROUTE_THERMAL_INTRUSION) == 0)
		response = handle_intrusion_alarm_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_HIGH_TEMP) == 0)
		response = handle_high_temp_alarm_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_BODY_TEMP) == 0)
		response = handle_body_temp_alarm_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_SHUTTER_MODE) == 0)
		response = handle_thermal_shutter_mode_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_SHUTTER_TIME) == 0)
		response = handle_thermal_shutter_time_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_CALIBRATE) == 0)
		response = handle_thermal_calibrate_put();
	else if (strcmp(path, ROUTE_THERMAL_RANGE) == 0)
		response = handle_thermal_range_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_PARAMS) == 0)
		response = handle_thermal_params_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_UNIT) == 0)
		response = handle_thermal_unit_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_COLOR_DIST) == 0)
		response = handle_thermal_color_dist_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_PALETTE) == 0)
		response = handle_thermal_palette_put(body, len);
	else if (strcmp(path, ROUTE_THERMAL_GAIN) == 0)
		response = handle_thermal_gain_put(body, len);
	else if (parse_image_channel(path, &id, sub, sizeof(sub)) == 0) {
		if (strcmp(sub, "image_flip") == 0)
			response = handle_image_flip_put(id, body, len);
		else if (strcmp(sub, "image_params") == 0)
			response = handle_image_params_put(id, body, len);
	} else if (parse_alarm_route(path, &id) == 0)
		response = handle_alarm_trigger_put(id, body, len);

	if (!response)
		response = create_error(EC_FAIL, "Not found");
	return response;
}

static char *http_post_dispatch(const char *path, const char *body, int len)
{
	char *response = NULL;

	if (strcmp(path, ROUTE_SYSTEM_FTP_TEST) == 0)
		response = handle_system_ftp_test(body, len);

	if (!response)
		response = create_error(EC_FAIL, "Not found");
	return response;
}

#define UPGRADE_UPLOAD_MAX (128 * 1024 * 1024)

/* basename of multipart filename; reject empty / path traversal. */
static int upgrade_safe_name(char *out, size_t out_sz, struct mg_str filename)
{
	const char *p;
	size_t i, n;

	if (!out || out_sz < 2)
		return -1;
	out[0] = '\0';
	if (!filename.buf || filename.len == 0)
		return -1;

	p = filename.buf;
	n = filename.len;
	for (i = 0; i < n; i++) {
		if (p[i] == '/' || p[i] == '\\') {
			p += i + 1;
			n -= i + 1;
			i = (size_t)-1;
		}
	}
	if (n == 0 || n >= out_sz)
		return -1;
	for (i = 0; i < n; i++) {
		char ch = p[i];
		if (ch == '\0' || ch == '"' || ch == '\r' || ch == '\n')
			return -1;
	}
	memcpy(out, p, n);
	out[n] = '\0';
	if (strcmp(out, ".") == 0 || strcmp(out, "..") == 0)
		return -1;
	return 0;
}

static int write_bin_file(const char *path, const void *data, size_t len)
{
	FILE *fp;
	size_t wrote;

	if (!path || (!data && len > 0))
		return -1;
	file_ensure_dir(path);
	fp = fopen(path, "wb");
	if (!fp)
		return -1;
	wrote = len ? fwrite(data, 1, len, fp) : 0;
	fclose(fp);
	return wrote == len ? 0 : -1;
}

/*
 * Holly-compatible: multipart/form-data with a file field (name=file).
 * mg_http_upload is not used — it expects ?file=&offset= and a raw body.
 */
static void handle_upgrade_upload(struct mg_connection *c,
				  struct mg_http_message *hm)
{
	char filepath[256];
	char name[128] = { 0 };
	char *response;
	struct mg_http_part part;
	size_t ofs = 0;
	int found = 0;

	if (hm->body.len == 0) {
		reply_json(c, create_error(EC_INVALID_PARAM, "Empty upload body"));
		return;
	}
	if (hm->body.len > UPGRADE_UPLOAD_MAX) {
		reply_json(c, create_error(EC_FAIL, "Upload exceeds max size"));
		return;
	}

	file_mkdir_p(NET_UPGRADE_DIR);

	while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) != 0) {
		int is_file_field;

		if (part.filename.len == 0)
			continue;
		is_file_field = (part.name.len == 4 &&
				 strncasecmp(part.name.buf, "file", 4) == 0);
		/* Prefer field name "file"; otherwise take first file part. */
		if (!is_file_field && found)
			continue;
		if (upgrade_safe_name(name, sizeof(name), part.filename) != 0)
			snprintf(name, sizeof(name), "firmware.img");
		snprintf(filepath, sizeof(filepath), "%s/%s", NET_UPGRADE_DIR,
			 name);
		if (write_bin_file(filepath, part.body.buf, part.body.len) !=
		    0) {
			reply_json(c,
				   create_error(EC_FAIL, "Failed to save upload"));
			return;
		}
		found = 1;
		if (is_file_field)
			break;
	}

	if (!found) {
		reply_json(c, create_error(EC_INVALID_PARAM,
					   "Missing upgrade file part"));
		return;
	}

	LOGI("Upgrade upload complete: %s (%zu bytes)", filepath,
	     (size_t)file_size(filepath));
	response = handle_system_upgrade(filepath);
	reply_json(c, response);
}

static const char *s_sdp_headers =
	"Content-Type: application/sdp\r\n"
	"Access-Control-Allow-Origin: *\r\n"
	"Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
	"Access-Control-Allow-Headers: Content-Type, Authorization\r\n";

/*
 * Same-origin WebRTC signaling proxy (SoftAP-safe).
 * Browser posts SDP to :80/media/webrtc; we call ZLM in-process.
 */
static void handle_webrtc_signaling(struct mg_connection *c,
				    struct mg_http_message *hm)
{
	char type[16] = { 0 };
	char app[32] = { 0 };
	char stream[32] = { 0 };
	char host[128] = { 0 };
	char err[256] = { 0 };
	char *offer = NULL;
	char *answer = NULL;
	struct mg_str *host_hdr;

	mg_http_get_var(&hm->query, "type", type, sizeof(type));
	mg_http_get_var(&hm->query, "app", app, sizeof(app));
	mg_http_get_var(&hm->query, "stream", stream, sizeof(stream));
	if (!type[0])
		snprintf(type, sizeof(type), "play");
	if (!app[0])
		snprintf(app, sizeof(app), "video");
	if (!stream[0])
		snprintf(stream, sizeof(stream), "101");

	host_hdr = mg_http_get_header(hm, "Host");
	if (host_hdr && host_hdr->buf && host_hdr->len > 0 &&
	    host_hdr->len < sizeof(host)) {
		memcpy(host, host_hdr->buf, host_hdr->len);
		host[host_hdr->len] = '\0';
	}

	if (hm->body.len == 0) {
		mg_http_reply(c, 400, s_cors_headers, "empty offer");
		return;
	}
	offer = (char *)malloc(hm->body.len + 1);
	if (!offer) {
		mg_http_reply(c, 500, s_cors_headers, "oom");
		return;
	}
	memcpy(offer, hm->body.buf, hm->body.len);
	offer[hm->body.len] = '\0';

	LOGI("[HTTP] webrtc proxy type=%s app=%s stream=%s host=%s offer_len=%zu",
	     type, app, stream, host[0] ? host : "-", hm->body.len);

	if (video_process_webrtc_answer(type, offer, app, stream,
					host[0] ? host : NULL, &answer, err,
					sizeof(err)) != 0 ||
	    !answer) {
		LOGE("[HTTP] webrtc proxy failed: %s", err[0] ? err : "unknown");
		mg_http_reply(c, 503, s_cors_headers, "%s",
			      err[0] ? err : "webrtc unavailable");
		free(offer);
		free(answer);
		return;
	}

	LOGI("[HTTP] webrtc proxy ok answer_len=%zu", strlen(answer));
	mg_http_reply(c, 200, s_sdp_headers, "%s", answer);
	free(offer);
	free(answer);
}

static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
	struct mg_http_message *hm;
	char path[256];

	if (ev != MG_EV_HTTP_MSG)
		return;

	hm = (struct mg_http_message *)ev_data;
	snprintf(path, sizeof(path), "%.*s", (int)hm->uri.len, hm->uri.buf);
	strip_leading_slash(path);

	/* Handle CORS preflight */
	if (method_is(hm, "OPTIONS")) {
		mg_http_reply(c, 204, s_cors_headers, "");
		return;
	}

	/* Static pages from sibling module app/www → /var/www. */
	if ((method_is(hm, "GET") || method_is(hm, "HEAD")) &&
	    is_static_request(path)) {
		if (http_web_access_gate(c, hm, path))
			return;
		serve_www(c, hm);
		return;
	}

	if (http_auth_gate(c, hm, path) != 0)
		return;

	if (strcmp(path, ROUTE_SYSTEM_DEVICE_UPGRADE) == 0 &&
	    method_is(hm, "POST")) {
		handle_upgrade_upload(c, hm);
		return;
	}

	if (strcmp(path, ROUTE_MEDIA_WEBRTC) == 0 && method_is(hm, "POST")) {
		handle_webrtc_signaling(c, hm);
		return;
	}

	if (method_is(hm, "GET"))
		reply_json(c, http_get_dispatch(path));
	else if (method_is(hm, "PUT"))
		reply_json(c, http_put_dispatch(path, hm->body.buf,
						(int)hm->body.len));
	else if (method_is(hm, "POST"))
		reply_json(c, http_post_dispatch(path, hm->body.buf,
						 (int)hm->body.len));
	else
		reply_json(c, create_error(EC_FAIL, "Not found"));
}

#define HTTP_API_POLL_MS 100

/*
 * mg_mgr_poll must never be re-entered from inside an event handler: it can
 * close and free the very connection the outer handler is still using.
 */
static int http_api_poll_tick(void)
{
	if (!s_started || s_polling)
		return 0;

	s_polling = 1;
	mg_mgr_poll(&s_mgr, HTTP_API_POLL_MS);
	s_polling = 0;
	return HTTP_API_POLL_MS;
}

static void *http_api_thread(void *opaque)
{
	(void)opaque;
	while (s_started)
		http_api_poll_tick();
	return NULL;
}

int http_api_start(int port)
{
	char url[64];

	if (s_started)
		return 0;

	mg_mgr_init(&s_mgr);
	snprintf(url, sizeof(url), "http://0.0.0.0:%d",
		 port > 0 ? port : NET_DEFAULT_HTTP_PORT);
	if (!mg_http_listen(&s_mgr, url, ev_handler, NULL)) {
		LOGE("http_api_start failed on %s", url);
		mg_mgr_free(&s_mgr);
		return -1;
	}
	s_started = 1;
	if (pthread_create(&s_thread, NULL, http_api_thread, NULL) != 0) {
		LOGE("http_api thread failed");
		s_started = 0;
		mg_mgr_free(&s_mgr);
		return -1;
	}
	s_thread_started = 1;
	LOGI("HTTP API listening on %s", url);
	return 0;
}

void http_api_stop(void)
{
	if (!s_started && !s_thread_started)
		return;
	s_started = 0;
	if (s_thread_started) {
		pthread_join(s_thread, NULL);
		s_thread_started = 0;
	}
	mg_mgr_free(&s_mgr);
	LOGI("HTTP API stopped");
}
