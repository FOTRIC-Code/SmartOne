#ifndef BLE_APP_COMPAT_H
#define BLE_APP_COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble_config.h"
#include "ble_provision.h"
#include "ble_wifi.h"
#include "json/json_util.h"

/* ── Error codes (from http_api_common.h) ──────────────────────────── */
#ifndef EC_OK
#define EC_OK            0
#define EC_INVALID_PARAM (-4)
#endif

/* ── Device name ───────────────────────────────────────────────────── */
#define NET_DEFAULT_DEVICE_NAME BLE_DEFAULT_DEVICE_NAME
#define NET_DEFAULT_HTTP_PORT   BLE_DEFAULT_HTTP_PORT

/* ── Provision states ──────────────────────────────────────────────── */
#define NET_PROVISION_STATE_AP_READY     PROVISION_STATE_AP_READY
#define NET_PROVISION_STATE_PROVISIONING PROVISION_STATE_PROVISIONING
#define NET_PROVISION_STATE_PROVISIONED  PROVISION_STATE_PROVISIONED
#define NET_PROVISION_STATE_FAILED       PROVISION_STATE_FAILED

/* ── Network states ────────────────────────────────────────────────── */
#define NET_NETWORK_STATE_OFFLINE NETWORK_STATE_OFFLINE
#define NET_NETWORK_STATE_ONLINE  NETWORK_STATE_ONLINE

/* ── Network modes ─────────────────────────────────────────────────── */
#define NET_NETWORK_MODE_AP  NETWORK_MODE_AP
#define NET_NETWORK_MODE_STA NETWORK_MODE_STA

/* ── Enum-to-string helpers for BLE JSON responses ─────────────────── */

static inline const char *provision_state_str(int state)
{
	switch (state) {
	case PROVISION_STATE_AP_READY:     return "unprovisioned";
	case PROVISION_STATE_PROVISIONING: return "provisioning";
	case PROVISION_STATE_PROVISIONED:  return "provisioned";
	case PROVISION_STATE_FAILED:       return "failed";
	default:                           return "unknown";
	}
}

static inline const char *network_state_str(int state)
{
	switch (state) {
	case NETWORK_STATE_OFFLINE: return "offline";
	case NETWORK_STATE_ONLINE:  return "online";
	default:                    return "unknown";
	}
}

static inline const char *network_mode_str(int mode)
{
	switch (mode) {
	case NETWORK_MODE_AP:  return "ap";
	case NETWORK_MODE_STA: return "sta";
	default:               return "unknown";
	}
}

/* ── Provision run state mapping ───────────────────────────────────── */
typedef ble_prov_run_state_t provision_run_state_t;
typedef ble_prov_run_status_t provision_run_status_t;

#define PROVISION_RUN_IDLE       BLE_PROV_RUN_IDLE
#define PROVISION_RUN_CONNECTING BLE_PROV_RUN_CONNECTING
#define PROVISION_RUN_SUCCESS    BLE_PROV_RUN_SUCCESS
#define PROVISION_RUN_FAILED     BLE_PROV_RUN_FAILED

/* ── Function mappings ─────────────────────────────────────────────── */

#define provision_is_commissioning_open()    ble_provision_is_commissioning_open()
#define provision_open_commissioning_window(d) ble_provision_open_commissioning_window(d)
#define provision_close_commissioning_window() ble_provision_close_commissioning_window()
#define provision_get_run_status(out) \
	ble_provision_get_run_status(out)
#define provision_request_ap(ssid, pwd, ch) \
	ble_provision_request_ap((ssid), (pwd), (ch))
#define provision_reset_to_ble() ble_provision_reset()

/*
 * g_config field access redirected to g_ble_config.
 * The original code uses g_config.hsi_info.*, g_config.provision.*,
 * g_config.port_info.*, g_config.networks[0].*.
 */

/* Shallow config facade so g_config.provision.xyz expressions compile. */
typedef struct ble_compat_port_info {
	int http_port_no;
	int rtp_port_no;
} ble_compat_port_info_t;

typedef struct ble_compat_network_info {
	char ip[48];
} ble_compat_network_info_t;

typedef struct ble_compat_config {
	ble_hsi_info_t hsi_info;
	ble_provision_info_t provision;
	ble_compat_port_info_t port_info;
	ble_compat_network_info_t networks[1];
} ble_compat_config_t;

/*
 * The ble_advertiser.c accesses g_config directly. We provide a macro
 * that expands to the real ble config, plus a sync function.
 */
extern ble_compat_config_t g_ble_compat_config;

static inline void ble_compat_sync_config(void)
{
	g_ble_compat_config.hsi_info = g_ble_config.hsi;
	g_ble_compat_config.provision = g_ble_config.provision;
	g_ble_compat_config.port_info.http_port_no =
		g_ble_config.provision.http_port_no > 0 ?
			g_ble_config.provision.http_port_no :
			BLE_DEFAULT_HTTP_PORT;
	g_ble_compat_config.port_info.rtp_port_no = 0;
}

#define g_config g_ble_compat_config

/* ── JSON helpers for BLE command parsing (replaces http_dto functions) ─ */

static inline int ble_parse_wifi_sta_json(const char *payload, int len,
					  char *ssid, size_t ssid_size,
					  char *password, size_t pw_size)
{
	cJSON *root, *item;

	(void)len;
	root = cJSON_Parse(payload);
	if (!root)
		return -1;

	item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
	if (!cJSON_IsString(item) || !item->valuestring[0]) {
		cJSON_Delete(root);
		return -1;
	}
	snprintf(ssid, ssid_size, "%s", item->valuestring);

	item = cJSON_GetObjectItemCaseSensitive(root, "pwd");
	if (cJSON_IsString(item) && item->valuestring[0])
		snprintf(password, pw_size, "%s", item->valuestring);
	else
		password[0] = '\0';

	cJSON_Delete(root);
	return 0;
}

static inline int ble_parse_hotspot_json(const char *payload, int len,
					 char *ssid, size_t ssid_size,
					 char *password, size_t pw_size,
					 int *has_password, int *channel)
{
	cJSON *root, *item;

	(void)len;
	root = cJSON_Parse(payload);
	if (!root)
		return -1;

	item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
	if (!cJSON_IsString(item) || !item->valuestring[0]) {
		cJSON_Delete(root);
		return -1;
	}
	snprintf(ssid, ssid_size, "%s", item->valuestring);

	*has_password = 0;
	item = cJSON_GetObjectItemCaseSensitive(root, "pwd");
	if (cJSON_IsString(item)) {
		snprintf(password, pw_size, "%s", item->valuestring);
		*has_password = 1;
	} else {
		password[0] = '\0';
	}

	*channel = 0;
	item = cJSON_GetObjectItemCaseSensitive(root, "channel");
	if (cJSON_IsNumber(item))
		*channel = item->valueint;

	cJSON_Delete(root);
	return 0;
}

#endif /* BLE_APP_COMPAT_H */
