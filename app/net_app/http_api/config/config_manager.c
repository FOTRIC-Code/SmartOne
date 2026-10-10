#include "config_manager.h"

#include "config_defaults.h"
#include "libs/file/file_util.h"
#include "libs/json/json_bind.h"
#include "libs/json/json_util.h"
#include "libs/log/log.h"
#include "libs/rtc/rtc_util.h"
#include "libs/time/time_util.h"
#include "main_process/main_process.h"
#include "thermal/thermal_data_provider.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
	hsi_info_cfg hsi_info;
	recipe_info_cfg recipe_info;
	time_info_cfg time_info;
	ntp_info_cfg ntp_server;
	port_info_cfg port_info;
	network_info_cfg networks[NET_MAX_NETWORKS];
	int network_count;
	provision_info_cfg provision;
	seal_param_cfg seal_param;
	intrusion_alarm_cfg intrusion_alarm;
	high_temp_alarm_cfg high_temp_alarm;
	body_temp_alarm_cfg body_temp_alarm;
	alarm_trigger_cfg alarm_triggers[NET_PARKIA_ALARM_TYPE_COUNT];
	ftp_info_cfg ftp_info;
	webhook_info_cfg webhook;
	thermal_settings_cfg thermal;
	image_settings_cfg image;
	log_cfg log_config;
} app_config_t;

static app_config_t config_app;
static pthread_mutex_t config_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	int record_duration_sec;
} legacy_record_config_cfg;

static const cfg_field_t stor_seal_fields[] = {
	CFG_BOOL(seal_param_cfg, sealed, "sealed"),
};

static const cfg_field_t stor_port_fields[] = {
	CFG_INT(port_info_cfg, http_port_no, "http_port_no"),
	CFG_INT(port_info_cfg, rtsp_port_no, "rtsp_port_no"),
	CFG_INT(port_info_cfg, rtp_port_no, "rtp_port_no"),
};

static const cfg_field_t stor_time_fields[] = {
	CFG_INT(time_info_cfg, time_mode, "time_mode"),
	CFG_STRING(time_info_cfg, local_time, "local_time"),
	CFG_STRING(time_info_cfg, time_zone, "time_zone"),
};

static const cfg_field_t stor_ntp_fields[] = {
	CFG_STRING(ntp_info_cfg, host_name, "host_name"),
	CFG_INT(ntp_info_cfg, port_no, "port_no"),
	CFG_INT(ntp_info_cfg, syn_interval, "syn_interval"),
};

static const cfg_field_t stor_hsi_fields[] = {
	CFG_STRING(hsi_info_cfg, name, "name"),
	CFG_STRING(hsi_info_cfg, model, "model"),
	CFG_STRING(hsi_info_cfg, spec, "spec"),
	CFG_STRING(hsi_info_cfg, serial_no, "serial_no"),
	CFG_STRING(hsi_info_cfg, id, "id"),
	CFG_STRING(hsi_info_cfg, firmware_name, "firmware_name"),
	CFG_STRING(hsi_info_cfg, firmware_version, "firmware_version"),
	CFG_STRING(hsi_info_cfg, manufacture_date, "manufacture_date"),
};

static const cfg_field_t stor_recipe_info_fields[] = {
	CFG_STRING(recipe_info_cfg, company, "Product_Company"),
	CFG_STRING(recipe_info_cfg, brand, "Product_Brand"),
	CFG_STRING(recipe_info_cfg, language, "Language_DefaultLanguage"),
};

static const cfg_field_t stor_network_fields[] = {
	CFG_INT(network_info_cfg, id, "id"),
	CFG_STRING(network_info_cfg, name, "name"),
	CFG_STRING(network_info_cfg, ip, "ip"),
	CFG_STRING(network_info_cfg, netmask, "netmask"),
	CFG_BOOL(network_info_cfg, dhcp, "dhcp"),
	CFG_STR_ARRAY(network_info_cfg, gateway, gateway_count, "gateway",
		      NET_MAX_ARRAY),
	CFG_STR_ARRAY(network_info_cfg, dns, dns_count, "dns", NET_MAX_ARRAY),
};

static const cfg_field_t stor_provision_fields[] = {
	CFG_INT(provision_info_cfg, provision_state, "provision_state"),
	CFG_INT(provision_info_cfg, network_state, "network_state"),
	CFG_INT(provision_info_cfg, network_mode, "network_mode"),
	CFG_STRING(provision_info_cfg, hotspot_ssid, "hotspot_ssid"),
	CFG_STRING(provision_info_cfg, hotspot_password, "hotspot_password"),
	CFG_INT(provision_info_cfg, hotspot_channel, "hotspot_channel"),
	CFG_STRING(provision_info_cfg, http_host, "http_host"),
	CFG_INT(provision_info_cfg, http_port_no, "http_port_no"),
	CFG_STRING(provision_info_cfg, setup_code, "setup_code"),
	CFG_STRING(provision_info_cfg, ssid, "ssid"),
	CFG_STRING(provision_info_cfg, password, "password"),
	CFG_INT(provision_info_cfg, last_error_code, "last_error_code"),
	CFG_STRING(provision_info_cfg, last_error_stage, "last_error_stage"),
	CFG_STRING(provision_info_cfg, last_error_reason, "last_error_reason"),
};

static const cfg_field_t stor_intrusion_alarm_fields[] = {
	CFG_BOOL(intrusion_alarm_cfg, enabled, "enabled"),
	CFG_INT(intrusion_alarm_cfg, no_repeat_minutes, "no_repeat_minutes"),
	CFG_INT(intrusion_alarm_cfg, sensitivity, "sensitivity"),
	CFG_INT(intrusion_alarm_cfg, record_duration_sec, "record_duration_sec"),
};

static const cfg_field_t stor_high_temp_alarm_fields[] = {
	CFG_BOOL(high_temp_alarm_cfg, enabled, "enabled"),
	CFG_INT(high_temp_alarm_cfg, no_repeat_minutes, "no_repeat_minutes"),
	CFG_FLOAT(high_temp_alarm_cfg, alarm_temp, "alarm_temp"),
	CFG_INT(high_temp_alarm_cfg, record_duration_sec, "record_duration_sec"),
};

static const cfg_field_t stor_body_temp_alarm_fields[] = {
	CFG_BOOL(body_temp_alarm_cfg, enabled, "enabled"),
	CFG_INT(body_temp_alarm_cfg, sensitivity, "sensitivity"),
};

static const cfg_field_t stor_alarm_trigger_fields[] = {
	CFG_BOOL(alarm_trigger_cfg, trigger_ftp_enabled, "trigger_ftp_enabled"),
	CFG_BOOL(alarm_trigger_cfg, trigger_webhook_enabled,
		 "trigger_webhook_enabled"),
	CFG_BOOL(alarm_trigger_cfg, trigger_record_enabled,
		 "trigger_record_enabled"),
};

static const cfg_field_t stor_ftp_info_fields[] = {
	CFG_BOOL(ftp_info_cfg, use_ssl, "use_ssl"),
	CFG_STRING(ftp_info_cfg, ip, "ip"),
	CFG_INT(ftp_info_cfg, port_no, "port_no"),
	CFG_STRING(ftp_info_cfg, user_name, "user_name"),
	CFG_STRING(ftp_info_cfg, pwd, "pwd"),
	CFG_BOOL(ftp_info_cfg, enable_annoy, "enable_annoy"),
	CFG_STRING(ftp_info_cfg, path_template, "path_template"),
};

static const cfg_field_t stor_webhook_fields[] = {
	CFG_STRING(webhook_info_cfg, url, "url"),
};

static const cfg_field_t stor_thermal_fields[] = {
	CFG_INT(thermal_settings_cfg, shutter_mode, "shutter_mode"),
	CFG_INT(thermal_settings_cfg, shutter_interval_min,
		"shutter_interval_min"),
	CFG_INT(thermal_settings_cfg, range_id, "range_id"),
	CFG_FLOAT(thermal_settings_cfg, emissivity, "emissivity"),
	CFG_FLOAT(thermal_settings_cfg, distance, "distance"),
	CFG_FLOAT(thermal_settings_cfg, refl_temp, "refl_temp"),
	CFG_FLOAT(thermal_settings_cfg, atm_temp, "atm_temp"),
	CFG_FLOAT(thermal_settings_cfg, rel_humidity, "rel_humidity"),
	CFG_INT(thermal_settings_cfg, temp_unit, "temp_unit"),
	CFG_INT(thermal_settings_cfg, distance_unit, "distance_unit"),
	CFG_INT(thermal_settings_cfg, color_dist, "color_dist"),
	CFG_INT(thermal_settings_cfg, palette_ref_no, "palette_ref_no"),
	CFG_BOOL(thermal_settings_cfg, palette_inverted, "palette_inverted"),
	CFG_INT_ARRAY(thermal_settings_cfg, palette_colors, palette_color_count,
		      "palette_colors", NET_MAX_PALETTE_COLORS),
	CFG_INT(thermal_settings_cfg, gain_mode, "gain_mode"),
	CFG_FLOAT(thermal_settings_cfg, gain_max, "gain_max"),
	CFG_FLOAT(thermal_settings_cfg, gain_min, "gain_min"),
	CFG_BOOL(thermal_settings_cfg, gain_fixed_span, "gain_fixed_span"),
	CFG_FLOAT(thermal_settings_cfg, gain_span, "gain_span"),
};

static const cfg_field_t stor_image_fields[] = {
	CFG_INT(image_settings_cfg, flip_mode, "flip_mode"),
	CFG_INT(image_settings_cfg, rotation, "rotation"),
	CFG_INT(image_settings_cfg, contrast, "contrast"),
	CFG_INT(image_settings_cfg, brightness, "brightness"),
	CFG_BOOL(image_settings_cfg, dde_enable, "dde_enable"),
	CFG_INT(image_settings_cfg, dde_value, "dde_value"),
};

static const cfg_field_t stor_legacy_record_fields[] = {
	CFG_INT(legacy_record_config_cfg, record_duration_sec,
		"record_duration_sec"),
};

static const cfg_field_t stor_log_config_fields[] = {
	CFG_BOOL(log_cfg, enabled, "enabled"),
	CFG_INT(log_cfg, level, "level"),
	CFG_INT(log_cfg, max_file_size, "max_file_size"),
	CFG_STRING(log_cfg, file_path, "file_path"),
};

static void load_alarms(void);
static void config_set_defaults(void);
static void config_sync_runtime(void);
static void config_apply_system_time(int ntp_mode, const char *time_zone,
				     const char *local_time, int restore_saved);
int config_set_hsi_info(const hsi_info_cfg *info);

static const char *const factory_reset_cfg_paths[] = {
	NET_CFG_TIME_PATH,
	NET_CFG_NTP_PATH,
	NET_CFG_NETWORK_PATH,
	NET_CFG_PORT_PATH,
	NET_CFG_LOG_PATH,
	NET_CFG_PROVISION_PATH,
	NET_CFG_ALARM_INTRUSION_PATH,
	NET_CFG_ALARM_HIGH_TEMP_PATH,
	NET_CFG_ALARM_BODY_TEMP_PATH,
	NET_CFG_ALARM_TRIGGER_PATH,
	NET_CFG_ALARM_TRIGGER_INTRUSION_PATH,
	NET_CFG_ALARM_TRIGGER_HIGH_TEMP_PATH,
	NET_CFG_ALARM_TRIGGER_BODY_TEMP_PATH,
	NET_CFG_RECORD_CONFIG_PATH,
	NET_CFG_FTP_PATH,
	NET_CFG_WEBHOOK_PATH,
	NET_CFG_THERMAL_PATH,
	NET_CFG_IMAGE_PATH,
	NULL,
};

static void set_str(char *dst, size_t size, const char *src)
{
	if (!dst || size == 0)
		return;
	snprintf(dst, size, "%s", src ? src : "");
}

static int config_save_and_sync(const char *path, const void *data,
				const cfg_field_t *fields, int count)
{
	if (cfg_save_file(path, data, fields, count) != 0)
		return -1;
	config_sync_runtime();
	return 0;
}

static int save_array_cfg(const char *path, const void *array, int count,
			  size_t item_size, const cfg_field_t *fields, int fc)
{
	json_t arr = cfg_array_to_json(array, count, item_size, fields, fc);

	if (!arr)
		return -1;
	int ret = json_save_file(path, arr);
	json_free(arr);
	return ret;
}

static int load_array_cfg(const char *path, void *array, int max_count,
			  size_t item_size, const cfg_field_t *fields, int fc,
			  int *out_count)
{
	json_t json = NULL;
	int ret;

	if (json_load_file(path, &json) != 0) {
		LOGW("Config not found or invalid: %s", path);
		return -1;
	}
	ret = cfg_array_from_json(json, array, max_count, item_size, fields,
				  fc, out_count);
	json_free(json);
	return ret;
}

static void config_set_defaults(void)
{
	int i;

	memset(&config_app, 0, sizeof(config_app));
	config_app.recipe_info = (recipe_info_cfg)DEFAULT_RECIPE_INFO;
	config_app.time_info = (time_info_cfg)DEFAULT_TIME_INFO;
	config_app.ntp_server = (ntp_info_cfg)DEFAULT_NTP_INFO;
	config_app.port_info = (port_info_cfg)DEFAULT_PORT_INFO;
	config_app.seal_param = (seal_param_cfg)DEFAULT_SEAL_PARAM;
	config_app.intrusion_alarm = (intrusion_alarm_cfg)DEFAULT_INTRUSION_ALARM;
	config_app.high_temp_alarm = (high_temp_alarm_cfg)DEFAULT_HIGH_TEMP_ALARM;
	config_app.body_temp_alarm = (body_temp_alarm_cfg)DEFAULT_BODY_TEMP_ALARM;
	for (i = 0; i < NET_PARKIA_ALARM_TYPE_COUNT; ++i)
		config_app.alarm_triggers[i] =
			(alarm_trigger_cfg)DEFAULT_ALARM_TRIGGER;
	config_app.ftp_info = (ftp_info_cfg)DEFAULT_FTP_INFO;
	config_app.webhook = (webhook_info_cfg)DEFAULT_WEBHOOK_INFO;
	config_app.thermal = (thermal_settings_cfg)DEFAULT_THERMAL_SETTINGS;
	config_app.image = (image_settings_cfg)DEFAULT_IMAGE_SETTINGS;
	config_app.log_config = (log_cfg)DEFAULT_LOG_CONFIG;
	config_app.networks[0] = (network_info_cfg)DEFAULT_NETWORK_ITEM;
	config_app.network_count = 1;
	config_app.provision = (provision_info_cfg)DEFAULT_PROVISION_INFO;
	net_common_set_default_hotspot_ssid(&config_app.provision,
					    config_app.hsi_info.serial_no);
}

static void config_apply_system_time(int ntp_mode, const char *time_zone,
				     const char *local_time, int restore_saved)
{
	time_t utc;

	time_tz_apply(time_zone);
	if (restore_saved && rtc_sync_system_time() == 0)
		return;
	if (ntp_mode)
		return;
	if (time_set_datetime(local_time, &utc) == 0)
		rtc_write(utc);
}

static void config_sync_runtime(void)
{
	pthread_mutex_lock(&config_lock);
	g_config.hsi_info = config_app.hsi_info;
	g_config.time_info = config_app.time_info;
	g_config.ntp_server = config_app.ntp_server;
	g_config.port_info = config_app.port_info;
	memcpy(g_config.networks, config_app.networks,
	       sizeof(config_app.networks[0]) * (size_t)config_app.network_count);
	g_config.network_count = config_app.network_count;
	g_config.provision = config_app.provision;
	g_config.intrusion_alarm = config_app.intrusion_alarm;
	g_config.high_temp_alarm = config_app.high_temp_alarm;
	g_config.body_temp_alarm = config_app.body_temp_alarm;
	memcpy(g_config.alarm_triggers, config_app.alarm_triggers,
	       sizeof(g_config.alarm_triggers));
	g_config.ftp_info = config_app.ftp_info;
	g_config.webhook = config_app.webhook;
	g_config.thermal = config_app.thermal;
	g_config.image = config_app.image;
	pthread_mutex_unlock(&config_lock);
}

static void load_hsi(void)
{
	cfg_load_file(NET_CFG_HSI_PATH, &config_app.hsi_info, stor_hsi_fields,
		      CFG_FIELD_COUNT(stor_hsi_fields));
}

static void load_recipe(void)
{
	cfg_load_file(NET_CFG_RECIPE_PATH, &config_app.recipe_info,
		      stor_recipe_info_fields,
		      CFG_FIELD_COUNT(stor_recipe_info_fields));
	/* Mirror into HSI for BLE / runtime callers that read g_config.hsi_info. */
	set_str(config_app.hsi_info.company,
		sizeof(config_app.hsi_info.company),
		config_app.recipe_info.company);
	set_str(config_app.hsi_info.brand, sizeof(config_app.hsi_info.brand),
		config_app.recipe_info.brand);
}

static void load_time(void)
{
	cfg_load_file(NET_CFG_TIME_PATH, &config_app.time_info, stor_time_fields,
		      CFG_FIELD_COUNT(stor_time_fields));
	cfg_load_file(NET_CFG_NTP_PATH, &config_app.ntp_server, stor_ntp_fields,
		      CFG_FIELD_COUNT(stor_ntp_fields));
}

static void load_network(void)
{
	load_array_cfg(NET_CFG_NETWORK_PATH, config_app.networks,
		       NET_MAX_NETWORKS, sizeof(network_info_cfg),
		       stor_network_fields,
		       CFG_FIELD_COUNT(stor_network_fields),
		       &config_app.network_count);
}

static void load_provision(void)
{
	cfg_load_file(NET_CFG_PROVISION_PATH, &config_app.provision,
		      stor_provision_fields,
		      CFG_FIELD_COUNT(stor_provision_fields));
}

static void load_port(void)
{
	cfg_load_file(NET_CFG_PORT_PATH, &config_app.port_info, stor_port_fields,
		      CFG_FIELD_COUNT(stor_port_fields));
}

static void load_seal(void)
{
	cfg_load_file(NET_CFG_SEAL_PATH, &config_app.seal_param, stor_seal_fields,
		      CFG_FIELD_COUNT(stor_seal_fields));
}

static void load_log(void)
{
	cfg_load_file(NET_CFG_LOG_PATH, &config_app.log_config, stor_log_config_fields,
		      CFG_FIELD_COUNT(stor_log_config_fields));
}

static void load_thermal(void)
{
	cfg_load_file(NET_CFG_THERMAL_PATH, &config_app.thermal,
		      stor_thermal_fields, CFG_FIELD_COUNT(stor_thermal_fields));
}

static void load_image(void)
{
	cfg_load_file(NET_CFG_IMAGE_PATH, &config_app.image,
		      stor_image_fields, CFG_FIELD_COUNT(stor_image_fields));
}

static void apply_log_config(void)
{
	log_apply_config(config_app.log_config.enabled, config_app.log_config.level,
			 config_app.log_config.max_file_size,
			 config_app.log_config.file_path);
}

int config_init(void)
{
	net_common_set_runtime_defaults();
	config_set_defaults();
	file_ensure_dir(NET_CFG_TIME_PATH);
	file_ensure_dir(NET_CFG_NTP_PATH);
	file_ensure_dir(NET_CFG_NETWORK_PATH);
	file_ensure_dir(NET_CFG_HSI_PATH);
	file_ensure_dir(NET_CFG_PROVISION_PATH);
	file_ensure_dir(NET_CFG_SEAL_PATH);
	file_ensure_dir(NET_CFG_RECIPE_PATH);
	file_ensure_dir(NET_CFG_PORT_PATH);
	file_ensure_dir(NET_CFG_LOG_PATH);
	file_ensure_dir(NET_CFG_ALARM_INTRUSION_PATH);
	file_ensure_dir(NET_CFG_FTP_PATH);
	file_ensure_dir(NET_CFG_WEBHOOK_PATH);
	file_ensure_dir(NET_CFG_THERMAL_PATH);
	file_ensure_dir(NET_CFG_IMAGE_PATH);
	file_mkdir_p(NET_TMP_DIR);
	file_mkdir_p(NET_UPGRADE_DIR);
	load_hsi();
	load_recipe();
	load_time();
	load_network();
	load_provision();
	load_port();
	load_alarms();
	load_seal();
	load_log();
	load_thermal();
	load_image();
	apply_log_config();
	config_apply_system_time(
		config_app.time_info.time_mode == NET_TIME_MODE_NTP,
		config_app.time_info.time_zone,
		config_app.time_info.local_time, 1);
	config_sync_runtime();
	LOGI("config_init done, provision_state=%d",
	     g_config.provision.provision_state);
	return 0;
}

void config_uninit(void)
{
}

hsi_info_cfg *config_get_hsi_info(void)
{
	return &config_app.hsi_info;
}

recipe_info_cfg *config_get_recipe_info(void)
{
	return &config_app.recipe_info;
}


int config_set_hsi_info(const hsi_info_cfg *info)
{
	if (!info)
		return -1;
	config_app.hsi_info = *info;
	return config_save_and_sync(NET_CFG_HSI_PATH, &config_app.hsi_info,
				    stor_hsi_fields,
				    CFG_FIELD_COUNT(stor_hsi_fields));
}

time_info_cfg *config_get_time_info(void)
{
	time_format_now(TIME_FMT_DATETIME, config_app.time_info.local_time,
			sizeof(config_app.time_info.local_time));
	return &config_app.time_info;
}

int config_set_time_info(const time_info_cfg *info)
{
	net_enum_time_mode_t prev_mode;
	time_info_cfg next;

	if (!info)
		return -1;

	prev_mode = (net_enum_time_mode_t)config_app.time_info.time_mode;
	next = *info;
	if (next.time_mode == NET_TIME_MODE_LOCAL && next.local_time[0] == '\0')
		time_format_now(TIME_FMT_DATETIME, next.local_time,
				sizeof(next.local_time));

	config_app.time_info = next;
	config_apply_system_time(next.time_mode == NET_TIME_MODE_NTP,
				 next.time_zone, next.local_time, 0);

	if (config_save_and_sync(NET_CFG_TIME_PATH, &config_app.time_info,
				 stor_time_fields,
				 CFG_FIELD_COUNT(stor_time_fields)) != 0)
		return -1;

	if (prev_mode != NET_TIME_MODE_NTP &&
	    next.time_mode == NET_TIME_MODE_NTP)
		main_process_request_task(NET_TASK_NTP_SYNC);
	return 0;
}

ntp_info_cfg *config_get_ntp_server(void)
{
	return &config_app.ntp_server;
}

int config_set_ntp_server(const ntp_info_cfg *info)
{
	if (!info)
		return -1;
	config_app.ntp_server = *info;
	if (config_save_and_sync(NET_CFG_NTP_PATH, &config_app.ntp_server,
				 stor_ntp_fields,
				 CFG_FIELD_COUNT(stor_ntp_fields)) != 0)
		return -1;
	if (g_config.time_info.time_mode == NET_TIME_MODE_NTP)
		main_process_request_task(NET_TASK_NTP_SYNC);
	return 0;
}

int config_get_networks(network_info_cfg *networks, int *count)
{
	int i;

	if (!networks || !count)
		return -1;
	for (i = 0; i < config_app.network_count; ++i)
		networks[i] = config_app.networks[i];
	*count = config_app.network_count;
	return 0;
}

network_info_cfg *config_get_network(int id)
{
	int i;

	for (i = 0; i < config_app.network_count; ++i) {
		if (config_app.networks[i].id == id)
			return &config_app.networks[i];
	}
	return NULL;
}

int config_set_network(int id, const network_info_cfg *network)
{
	network_info_cfg target;
	int i;

	if (!network)
		return -1;

	target = *network;
	target.id = id;

	for (i = 0; i < config_app.network_count; i++) {
		if (config_app.networks[i].id == id) {
			network_info_cfg old = config_app.networks[i];
			config_app.networks[i] = target;
			if (save_array_cfg(
				NET_CFG_NETWORK_PATH, config_app.networks,
				config_app.network_count,
				sizeof(network_info_cfg), stor_network_fields,
				CFG_FIELD_COUNT(stor_network_fields)) != 0) {
				config_app.networks[i] = old;
				return -1;
			}
			config_sync_runtime();
			return 0;
		}
	}

	if (config_app.network_count >= NET_MAX_NETWORKS)
		return -1;

	config_app.networks[config_app.network_count] = target;
	config_app.network_count++;
	if (save_array_cfg(NET_CFG_NETWORK_PATH, config_app.networks,
			   config_app.network_count, sizeof(network_info_cfg),
			   stor_network_fields,
			   CFG_FIELD_COUNT(stor_network_fields)) != 0) {
		config_app.network_count--;
		return -1;
	}
	config_sync_runtime();
	return 0;
}

int config_set_networks_batch(const network_info_cfg *networks, int count)
{
	if (!networks || count <= 0 || count > NET_MAX_NETWORKS)
		return -1;

	memcpy(config_app.networks, networks,
	       (size_t)count * sizeof(network_info_cfg));
	config_app.network_count = count;
	if (save_array_cfg(NET_CFG_NETWORK_PATH, config_app.networks,
			   config_app.network_count, sizeof(network_info_cfg),
			   stor_network_fields,
			   CFG_FIELD_COUNT(stor_network_fields)) != 0)
		return -1;
	config_sync_runtime();
	return 0;
}

provision_info_cfg *config_get_provision(void)
{
	return &config_app.provision;
}

int config_reset_factory_defaults(void)
{
	size_t i;
	int type_index;

	for (i = 0; factory_reset_cfg_paths[i] != NULL; i++) {
		if (remove(factory_reset_cfg_paths[i]) != 0 && errno != ENOENT)
			LOGW("factory reset: failed to remove %s",
			     factory_reset_cfg_paths[i]);
		else
			LOGI("factory reset: removed %s",
			     factory_reset_cfg_paths[i]);
	}

	config_app.time_info = (time_info_cfg)DEFAULT_TIME_INFO;
	config_app.ntp_server = (ntp_info_cfg)DEFAULT_NTP_INFO;
	config_app.port_info = (port_info_cfg)DEFAULT_PORT_INFO;
	config_app.intrusion_alarm = (intrusion_alarm_cfg)DEFAULT_INTRUSION_ALARM;
	config_app.high_temp_alarm = (high_temp_alarm_cfg)DEFAULT_HIGH_TEMP_ALARM;
	config_app.body_temp_alarm = (body_temp_alarm_cfg)DEFAULT_BODY_TEMP_ALARM;
	for (type_index = 0; type_index < NET_PARKIA_ALARM_TYPE_COUNT;
	     ++type_index)
		config_app.alarm_triggers[type_index] =
			(alarm_trigger_cfg)DEFAULT_ALARM_TRIGGER;
	config_app.ftp_info = (ftp_info_cfg)DEFAULT_FTP_INFO;
	config_app.webhook = (webhook_info_cfg)DEFAULT_WEBHOOK_INFO;
	config_app.thermal = (thermal_settings_cfg)DEFAULT_THERMAL_SETTINGS;
	config_app.image = (image_settings_cfg)DEFAULT_IMAGE_SETTINGS;
	config_app.log_config = (log_cfg)DEFAULT_LOG_CONFIG;
	config_app.networks[0] = (network_info_cfg)DEFAULT_NETWORK_ITEM;
	config_app.network_count = 1;
	config_app.provision = (provision_info_cfg)DEFAULT_PROVISION_INFO;
	net_common_set_default_hotspot_ssid(&config_app.provision,
					    config_app.hsi_info.serial_no);

	if (config_set_time_info(&config_app.time_info) != 0)
		return -1;
	if (config_set_ntp_server(&config_app.ntp_server) != 0)
		return -1;
	if (config_set_port_info(&config_app.port_info) != 0)
		return -1;
	if (config_set_network(config_app.networks[0].id,
			       &config_app.networks[0]) != 0)
		return -1;
	if (config_set_intrusion_alarm(&config_app.intrusion_alarm) != 0)
		return -1;
	if (config_set_high_temp_alarm(&config_app.high_temp_alarm) != 0)
		return -1;
	if (config_set_body_temp_alarm(&config_app.body_temp_alarm) != 0)
		return -1;
	for (type_index = 0; type_index < NET_PARKIA_ALARM_TYPE_COUNT;
	     ++type_index) {
		if (config_set_alarm_trigger(
			    type_index + NET_PARKIA_ALARM_TYPE_FIRST,
			    &config_app.alarm_triggers[type_index]) != 0)
			return -1;
	}
	if (config_set_ftp_info(&config_app.ftp_info) != 0 ||
	    config_set_webhook(&config_app.webhook) != 0)
		return -1;
	if (config_set_log(&config_app.log_config) != 0)
		return -1;

	/* Provision file already removed; keep runtime at factory AP defaults
	 * without rewriting credentials to disk. */
	config_sync_runtime();
	LOGI("factory reset complete (seal/hsi/recipe preserved; provision cleared)");
	return 0;
}

seal_param_cfg *config_get_seal_param(void)
{
	return &config_app.seal_param;
}

int config_set_seal_param(const seal_param_cfg *seal)
{
	if (!seal)
		return -1;
	config_app.seal_param = *seal;
	return config_save_and_sync(NET_CFG_SEAL_PATH, &config_app.seal_param,
				    stor_seal_fields,
				    CFG_FIELD_COUNT(stor_seal_fields));
}

port_info_cfg *config_get_port_info(void)
{
	return &config_app.port_info;
}

int config_set_port_info(const port_info_cfg *info)
{
	if (!info)
		return -1;
	config_app.port_info = *info;
	if (config_save_and_sync(NET_CFG_PORT_PATH, &config_app.port_info,
				 stor_port_fields,
				 CFG_FIELD_COUNT(stor_port_fields)) != 0)
		return -1;
	main_process_request_task(NET_TASK_HTTP_REBIND);
	return 0;
}

/* ---------- Alarm persistence ---------- */

static void load_alarms(void)
{
	static const char *const trigger_paths[NET_PARKIA_ALARM_TYPE_COUNT] = {
		NET_CFG_ALARM_TRIGGER_INTRUSION_PATH,
		NET_CFG_ALARM_TRIGGER_HIGH_TEMP_PATH,
		NET_CFG_ALARM_TRIGGER_BODY_TEMP_PATH,
	};
	alarm_trigger_cfg legacy_trigger = (alarm_trigger_cfg)DEFAULT_ALARM_TRIGGER;
	legacy_record_config_cfg legacy_record = { 30 };
	int legacy_trigger_loaded = 0;
	int legacy_record_loaded = 0;
	int i;

	if (cfg_load_file(NET_CFG_RECORD_CONFIG_PATH, &legacy_record,
			  stor_legacy_record_fields,
			  CFG_FIELD_COUNT(stor_legacy_record_fields)) == 0 &&
	    (legacy_record.record_duration_sec == 10 ||
	     legacy_record.record_duration_sec == 30 ||
	     legacy_record.record_duration_sec == 60)) {
		legacy_record_loaded = 1;
		config_app.intrusion_alarm.record_duration_sec =
			legacy_record.record_duration_sec;
		config_app.high_temp_alarm.record_duration_sec =
			legacy_record.record_duration_sec;
	}

	cfg_load_file(NET_CFG_ALARM_INTRUSION_PATH, &config_app.intrusion_alarm,
		      stor_intrusion_alarm_fields,
		      CFG_FIELD_COUNT(stor_intrusion_alarm_fields));
	cfg_load_file(NET_CFG_ALARM_HIGH_TEMP_PATH, &config_app.high_temp_alarm,
		      stor_high_temp_alarm_fields,
		      CFG_FIELD_COUNT(stor_high_temp_alarm_fields));
	cfg_load_file(NET_CFG_ALARM_BODY_TEMP_PATH, &config_app.body_temp_alarm,
		      stor_body_temp_alarm_fields,
		      CFG_FIELD_COUNT(stor_body_temp_alarm_fields));
	if (config_app.intrusion_alarm.record_duration_sec != 30 &&
	    config_app.intrusion_alarm.record_duration_sec != 60)
		config_app.intrusion_alarm.record_duration_sec = 30;
	if (config_app.high_temp_alarm.record_duration_sec != 30 &&
	    config_app.high_temp_alarm.record_duration_sec != 60)
		config_app.high_temp_alarm.record_duration_sec = 30;
	if (config_app.body_temp_alarm.sensitivity < NET_ALARM_SENSITIVITY_LOW ||
	    config_app.body_temp_alarm.sensitivity > NET_ALARM_SENSITIVITY_HIGH)
		config_app.body_temp_alarm.sensitivity = NET_ALARM_SENSITIVITY_MID;
	if (legacy_record_loaded &&
	    cfg_save_file(NET_CFG_ALARM_INTRUSION_PATH,
			  &config_app.intrusion_alarm,
			  stor_intrusion_alarm_fields,
			  CFG_FIELD_COUNT(stor_intrusion_alarm_fields)) == 0 &&
	    cfg_save_file(NET_CFG_ALARM_HIGH_TEMP_PATH,
			  &config_app.high_temp_alarm,
			  stor_high_temp_alarm_fields,
			  CFG_FIELD_COUNT(stor_high_temp_alarm_fields)) == 0)
		remove(NET_CFG_RECORD_CONFIG_PATH);

	if (cfg_load_file(NET_CFG_ALARM_TRIGGER_PATH, &legacy_trigger,
			  stor_alarm_trigger_fields,
			  CFG_FIELD_COUNT(stor_alarm_trigger_fields)) == 0) {
		legacy_trigger_loaded = 1;
		for (i = 0; i < NET_PARKIA_ALARM_TYPE_COUNT; ++i)
			config_app.alarm_triggers[i] = legacy_trigger;
	}
	for (i = 0; i < NET_PARKIA_ALARM_TYPE_COUNT; ++i)
		cfg_load_file(trigger_paths[i], &config_app.alarm_triggers[i],
			      stor_alarm_trigger_fields,
			      CFG_FIELD_COUNT(stor_alarm_trigger_fields));
	if (legacy_trigger_loaded) {
		int migrated = 1;

		for (i = 0; i < NET_PARKIA_ALARM_TYPE_COUNT; ++i) {
			if (cfg_save_file(
				    trigger_paths[i],
				    &config_app.alarm_triggers[i],
				    stor_alarm_trigger_fields,
				    CFG_FIELD_COUNT(stor_alarm_trigger_fields)) != 0)
				migrated = 0;
		}
		if (migrated)
			remove(NET_CFG_ALARM_TRIGGER_PATH);
	}

	cfg_load_file(NET_CFG_FTP_PATH, &config_app.ftp_info,
		      stor_ftp_info_fields, CFG_FIELD_COUNT(stor_ftp_info_fields));
	cfg_load_file(NET_CFG_WEBHOOK_PATH, &config_app.webhook,
		      stor_webhook_fields, CFG_FIELD_COUNT(stor_webhook_fields));
}

intrusion_alarm_cfg *config_get_intrusion_alarm(void)
{
	return &config_app.intrusion_alarm;
}

int config_set_intrusion_alarm(const intrusion_alarm_cfg *cfg)
{
	if (!cfg)
		return -1;
	config_app.intrusion_alarm = *cfg;
	return config_save_and_sync(NET_CFG_ALARM_INTRUSION_PATH,
				    &config_app.intrusion_alarm,
				    stor_intrusion_alarm_fields,
				    CFG_FIELD_COUNT(stor_intrusion_alarm_fields));
}

high_temp_alarm_cfg *config_get_high_temp_alarm(void)
{
	return &config_app.high_temp_alarm;
}

int config_set_high_temp_alarm(const high_temp_alarm_cfg *cfg)
{
	if (!cfg)
		return -1;
	config_app.high_temp_alarm = *cfg;
	return config_save_and_sync(NET_CFG_ALARM_HIGH_TEMP_PATH,
				    &config_app.high_temp_alarm,
				    stor_high_temp_alarm_fields,
				    CFG_FIELD_COUNT(stor_high_temp_alarm_fields));
}

body_temp_alarm_cfg *config_get_body_temp_alarm(void)
{
	return &config_app.body_temp_alarm;
}

int config_set_body_temp_alarm(const body_temp_alarm_cfg *cfg)
{
	if (!cfg)
		return -1;
	config_app.body_temp_alarm = *cfg;
	return config_save_and_sync(NET_CFG_ALARM_BODY_TEMP_PATH,
				    &config_app.body_temp_alarm,
				    stor_body_temp_alarm_fields,
				    CFG_FIELD_COUNT(stor_body_temp_alarm_fields));
}

alarm_trigger_cfg *config_get_alarm_trigger(int type)
{
	int index = net_alarm_type_index(type);

	return index >= 0 ? &config_app.alarm_triggers[index] : NULL;
}

int config_set_alarm_trigger(int type, const alarm_trigger_cfg *cfg)
{
	static const char *const paths[NET_PARKIA_ALARM_TYPE_COUNT] = {
		NET_CFG_ALARM_TRIGGER_INTRUSION_PATH,
		NET_CFG_ALARM_TRIGGER_HIGH_TEMP_PATH,
		NET_CFG_ALARM_TRIGGER_BODY_TEMP_PATH,
	};
	int index = net_alarm_type_index(type);

	if (!cfg || index < 0)
		return -1;
	config_app.alarm_triggers[index] = *cfg;
	return config_save_and_sync(paths[index],
				    &config_app.alarm_triggers[index],
				    stor_alarm_trigger_fields,
				    CFG_FIELD_COUNT(stor_alarm_trigger_fields));
}

ftp_info_cfg *config_get_ftp_info(void)
{
	return &config_app.ftp_info;
}

int config_set_ftp_info(const ftp_info_cfg *cfg)
{
	if (!cfg)
		return -1;
	config_app.ftp_info = *cfg;
	return config_save_and_sync(NET_CFG_FTP_PATH, &config_app.ftp_info,
				    stor_ftp_info_fields,
				    CFG_FIELD_COUNT(stor_ftp_info_fields));
}

webhook_info_cfg *config_get_webhook(void)
{
	return &config_app.webhook;
}

int config_set_webhook(const webhook_info_cfg *cfg)
{
	if (!cfg)
		return -1;
	config_app.webhook = *cfg;
	return config_save_and_sync(NET_CFG_WEBHOOK_PATH, &config_app.webhook,
				    stor_webhook_fields,
				    CFG_FIELD_COUNT(stor_webhook_fields));
}

thermal_settings_cfg *config_get_thermal_settings(void)
{
	return &config_app.thermal;
}

int config_set_thermal_settings(const thermal_settings_cfg *cfg)
{
	int ret;

	if (!cfg)
		return -1;
	config_app.thermal = *cfg;
	ret = config_save_and_sync(NET_CFG_THERMAL_PATH, &config_app.thermal,
				   stor_thermal_fields,
				   CFG_FIELD_COUNT(stor_thermal_fields));
	if (ret == 0)
		thermal_provider_apply_thermal_settings();
	return ret;
}

image_settings_cfg *config_get_image_settings(void)
{
	return &config_app.image;
}

int config_set_image_settings(const image_settings_cfg *cfg)
{
	int ret;

	if (!cfg)
		return -1;
	config_app.image = *cfg;
	ret = config_save_and_sync(NET_CFG_IMAGE_PATH, &config_app.image,
				   stor_image_fields,
				   CFG_FIELD_COUNT(stor_image_fields));
	if (ret == 0)
		thermal_provider_apply_image_settings();
	return ret;
}

int config_copy_alarm_linkage(int type, alarm_trigger_cfg *trigger,
			      ftp_info_cfg *ftp, webhook_info_cfg *webhook)
{
	int index = net_alarm_type_index(type);

	if (index < 0 || !trigger || !ftp || !webhook)
		return -1;
	pthread_mutex_lock(&config_lock);
	*trigger = g_config.alarm_triggers[index];
	*ftp = g_config.ftp_info;
	*webhook = g_config.webhook;
	pthread_mutex_unlock(&config_lock);
	return 0;
}

log_cfg *config_get_log(void)
{
	return &config_app.log_config;
}

int config_set_log(const log_cfg *cfg)
{
	if (!cfg)
		return -1;
	config_app.log_config = *cfg;
	if (cfg_save_file(NET_CFG_LOG_PATH, &config_app.log_config,
			  stor_log_config_fields,
			  CFG_FIELD_COUNT(stor_log_config_fields)) != 0)
		return -1;
	apply_log_config();
	return 0;
}

