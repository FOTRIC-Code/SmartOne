#include "ble_config.h"

#include "file/file_util.h"
#include "json/json_bind.h"
#include "json/json_util.h"
#include "libs/log/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ble_app_config_t g_ble_config;


static const cfg_field_t hsi_fields[] = {
	CFG_STRING(ble_hsi_info_t, name, "name"),
	CFG_STRING(ble_hsi_info_t, model, "model"),
	CFG_STRING(ble_hsi_info_t, serial_no, "serial_no"),
	CFG_STRING(ble_hsi_info_t, id, "id"),
	CFG_STRING(ble_hsi_info_t, firmware_version, "firmware_version"),
};

static const cfg_field_t app_settings_fields[] = {
	CFG_STRING(ble_app_settings_t, app_bin, "app_bin"),
};

static const cfg_field_t provision_fields[] = {
	CFG_INT(ble_provision_info_t, provision_state, "provision_state"),
	CFG_INT(ble_provision_info_t, network_state, "network_state"),
	CFG_INT(ble_provision_info_t, network_mode, "network_mode"),
	CFG_STRING(ble_provision_info_t, hotspot_ssid, "hotspot_ssid"),
	CFG_STRING(ble_provision_info_t, hotspot_password, "hotspot_password"),
	CFG_INT(ble_provision_info_t, hotspot_channel, "hotspot_channel"),
	CFG_STRING(ble_provision_info_t, http_host, "http_host"),
	CFG_INT(ble_provision_info_t, http_port_no, "http_port_no"),
	CFG_STRING(ble_provision_info_t, setup_code, "setup_code"),
	CFG_STRING(ble_provision_info_t, ssid, "ssid"),
	CFG_STRING(ble_provision_info_t, password, "password"),
	CFG_INT(ble_provision_info_t, last_error_code, "last_error_code"),
	CFG_STRING(ble_provision_info_t, last_error_stage, "last_error_stage"),
	CFG_STRING(ble_provision_info_t, last_error_reason, "last_error_reason"),
};

static void set_str(char *dst, size_t size, const char *src)
{
	if (!dst || size == 0)
		return;
	if (src == dst)
		return; /* already the same buffer — skip to avoid snprintf UB */
	snprintf(dst, size, "%s", src ? src : "");
}

void ble_config_build_default_hotspot_ssid(char *dst, size_t size,
					   const char *serial_no)
{
	const char *suffix;
	size_t sn_len;

	if (!dst || size == 0)
		return;

	if (!serial_no || serial_no[0] == '\0') {
		snprintf(dst, size, "%s", BLE_DEFAULT_DEVICE_NAME);
		return;
	}

	sn_len = strlen(serial_no);
	suffix = serial_no + (sn_len > 4 ? sn_len - 4 : 0);
	snprintf(dst, size, "%s-%s", BLE_DEFAULT_DEVICE_NAME, suffix);
}

static void load_defaults(void)
{
	memset(&g_ble_config, 0, sizeof(g_ble_config));
	set_str(g_ble_config.settings.app_bin,
		sizeof(g_ble_config.settings.app_bin),
		BLE_APP_BIN_DEFAULT);
	g_ble_config.provision.provision_state = PROVISION_STATE_AP_READY;
	g_ble_config.provision.network_state = NETWORK_STATE_OFFLINE;
	g_ble_config.provision.network_mode = NETWORK_MODE_STA;
	set_str(g_ble_config.provision.http_host,
		sizeof(g_ble_config.provision.http_host), "192.168.4.1");
	g_ble_config.provision.http_port_no = BLE_DEFAULT_HTTP_PORT;
	set_str(g_ble_config.provision.setup_code,
		sizeof(g_ble_config.provision.setup_code), "123456");
	set_str(g_ble_config.provision.last_error_stage,
		sizeof(g_ble_config.provision.last_error_stage), "idle");
	set_str(g_ble_config.provision.last_error_reason,
		sizeof(g_ble_config.provision.last_error_reason), "none");
}

static void load_recipe(void)
{
	char *data = json_read_file(BLE_CFG_RECIPE_PATH);
	cJSON *root = data ? cJSON_Parse(data) : NULL;

	if (root) {
		set_str(g_ble_config.hsi.brand, sizeof(g_ble_config.hsi.brand),
			json_get_string(root, "Product_Brand",
					g_ble_config.hsi.brand));
		cJSON_Delete(root);
	}
	free(data);
}

int ble_config_init(void)
{
	load_defaults();
	file_mkdir_p(BLE_CFG_PROVISION_DIR);
	file_mkdir_p(BLE_CFG_APP_DIR);

	cfg_load_file(BLE_CFG_APP_PATH, &g_ble_config.settings,
		      app_settings_fields, CFG_FIELD_COUNT(app_settings_fields));
	cfg_load_file(BLE_CFG_HSI_PATH, &g_ble_config.hsi,
		      hsi_fields, CFG_FIELD_COUNT(hsi_fields));
	load_recipe();
	cfg_load_file(BLE_CFG_PROVISION_PATH, &g_ble_config.provision,
		      provision_fields, CFG_FIELD_COUNT(provision_fields));

	if (g_ble_config.settings.app_bin[0] == '\0')
		set_str(g_ble_config.settings.app_bin,
			sizeof(g_ble_config.settings.app_bin),
			BLE_APP_BIN_DEFAULT);

	if (g_ble_config.provision.hotspot_ssid[0] == '\0')
		ble_config_build_default_hotspot_ssid(
			g_ble_config.provision.hotspot_ssid,
			sizeof(g_ble_config.provision.hotspot_ssid),
			g_ble_config.hsi.serial_no);

	LOGI("ble_config_init done, provision_state=%d ssid=%s",
	     g_ble_config.provision.provision_state,
	     g_ble_config.provision.ssid);
	return 0;
}

int ble_config_save_provision(void)
{
	if (g_ble_config.provision.provision_state ==
		    PROVISION_STATE_PROVISIONED &&
	    g_ble_config.provision.ssid[0] == '\0') {
		LOGW("BLE CONFIG WARNING: saving provision with EMPTY ssid "
		     "while state=PROVISIONED!");
	}
	return cfg_save_file(BLE_CFG_PROVISION_PATH, &g_ble_config.provision,
			     provision_fields,
			     CFG_FIELD_COUNT(provision_fields));
}

int ble_config_set_provision_state(int provision_state, int network_state,
				   const char *ssid, const char *password,
				   const char *ip)
{
	const char *old_ssid = g_ble_config.provision.ssid;

	if (old_ssid[0] != '\0' && (!ssid || ssid[0] == '\0')) {
		LOGW("BLE CONFIG: ssid changing from '%s' to EMPTY! "
		     "state=%d->%d", old_ssid, 
		     g_ble_config.provision.provision_state, provision_state);
	}
	g_ble_config.provision.provision_state = provision_state;
	g_ble_config.provision.network_state = network_state;
	set_str(g_ble_config.provision.ssid,
		sizeof(g_ble_config.provision.ssid), ssid);
	if (password)
		set_str(g_ble_config.provision.password,
			sizeof(g_ble_config.provision.password), password);
	(void)ip;
	LOGI("BLE CONFIG: provision saved state=%d net=%d ssid='%s'",
	     provision_state, network_state,
	     g_ble_config.provision.ssid);
	return ble_config_save_provision();
}

int ble_config_set_network_mode(int provision_state, int network_state,
				int network_mode)
{
	g_ble_config.provision.provision_state = provision_state;
	g_ble_config.provision.network_state = network_state;
	g_ble_config.provision.network_mode = network_mode;
	return ble_config_save_provision();
}

int ble_config_set_hotspot(const char *ssid, const char *password, int channel)
{
	if (ssid && ssid[0])
		set_str(g_ble_config.provision.hotspot_ssid,
			sizeof(g_ble_config.provision.hotspot_ssid), ssid);
	if (password)
		set_str(g_ble_config.provision.hotspot_password,
			sizeof(g_ble_config.provision.hotspot_password),
			password);
	g_ble_config.provision.hotspot_channel = channel > 0 ? channel : 0;
	return ble_config_save_provision();
}

int ble_config_reset_provision(void)
{
	g_ble_config.provision.provision_state = PROVISION_STATE_AP_READY;
	g_ble_config.provision.network_state = NETWORK_STATE_OFFLINE;
	g_ble_config.provision.network_mode = NETWORK_MODE_STA;
	g_ble_config.provision.ssid[0] = '\0';
	g_ble_config.provision.password[0] = '\0';
	ble_config_build_default_hotspot_ssid(
		g_ble_config.provision.hotspot_ssid,
		sizeof(g_ble_config.provision.hotspot_ssid),
		g_ble_config.hsi.serial_no);
	g_ble_config.provision.hotspot_password[0] = '\0';
	g_ble_config.provision.hotspot_channel = 0;
	g_ble_config.provision.last_error_code = 0;
	set_str(g_ble_config.provision.last_error_stage,
		sizeof(g_ble_config.provision.last_error_stage), "idle");
	set_str(g_ble_config.provision.last_error_reason,
		sizeof(g_ble_config.provision.last_error_reason), "none");
	return ble_config_save_provision();
}

int ble_config_clear_provision_error(void)
{
	g_ble_config.provision.last_error_code = 0;
	set_str(g_ble_config.provision.last_error_stage,
		sizeof(g_ble_config.provision.last_error_stage), "idle");
	set_str(g_ble_config.provision.last_error_reason,
		sizeof(g_ble_config.provision.last_error_reason), "none");
	return ble_config_save_provision();
}

int ble_config_persist_provision_error(int error_code, const char *stage,
				       const char *reason)
{
	g_ble_config.provision.last_error_code = error_code;
	set_str(g_ble_config.provision.last_error_stage,
		sizeof(g_ble_config.provision.last_error_stage),
		stage && stage[0] ? stage : "idle");
	set_str(g_ble_config.provision.last_error_reason,
		sizeof(g_ble_config.provision.last_error_reason),
		reason && reason[0] ? reason : "none");
	return ble_config_save_provision();
}

bool ble_config_is_provisioned(void)
{
	return g_ble_config.provision.provision_state ==
		       PROVISION_STATE_PROVISIONED &&
	       g_ble_config.provision.ssid[0] != '\0';
}

bool ble_config_is_ap_mode(void)
{
	return g_ble_config.provision.provision_state ==
		       PROVISION_STATE_PROVISIONED &&
	       g_ble_config.provision.network_mode == NETWORK_MODE_AP;
}
