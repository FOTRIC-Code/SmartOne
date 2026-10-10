#ifndef BLE_APP_CONFIG_H
#define BLE_APP_CONFIG_H

#include "provision_shared.h"

#include <stdbool.h>
#include <stddef.h>

#define BLE_APP_NAME    "main_app"
#define BLE_APP_VERSION "1.0.0.0"

#define BLE_DEFAULT_DEVICE_NAME "SmartOne"
#define BLE_DEFAULT_HTTP_PORT   80

#define BLE_CFG_DIR          "/etc/cfg"
#define BLE_CFG_HSI_PATH     BLE_CFG_DIR "/device/hsi/config.json"
#define BLE_CFG_RECIPE_PATH  BLE_CFG_DIR "/sensor/recipe_file/oem/oem.json"
#define BLE_CFG_APP_DIR      BLE_CFG_DIR "/main_app"
#define BLE_CFG_APP_PATH     BLE_CFG_APP_DIR "/config.json"
#define BLE_CFG_PROVISION_DIR  PROVISION_SHARED_DIR
#define BLE_CFG_PROVISION_PATH PROVISION_SHARED_PATH

#define BLE_LEN_16   16
#define BLE_LEN_32   32
#define BLE_LEN_48   48
#define BLE_LEN_64   64
#define BLE_LEN_128  128

typedef struct ble_hsi_info {
	char name[BLE_LEN_128];
	char model[BLE_LEN_128];
	char serial_no[BLE_LEN_16];
	char id[BLE_LEN_32];
	char brand[BLE_LEN_32];
	char firmware_version[BLE_LEN_16];
} ble_hsi_info_t;

typedef struct ble_provision_info {
	int provision_state;
	int network_state;
	int network_mode;
	char hotspot_ssid[64];
	char hotspot_password[128];
	int hotspot_channel;
	char http_host[48];
	int http_port_no;
	char setup_code[32];
	char ssid[64];
	char password[128];
	int last_error_code;
	char last_error_stage[32];
	char last_error_reason[64];
} ble_provision_info_t;

#define BLE_APP_BIN_DEFAULT "/usr/bin/net_app"
#define BLE_APP_BIN_MAXLEN 256

typedef struct ble_app_settings {
	char app_bin[BLE_APP_BIN_MAXLEN];
} ble_app_settings_t;

typedef struct ble_app_config {
	ble_hsi_info_t hsi;
	ble_provision_info_t provision;
	ble_app_settings_t settings;
} ble_app_config_t;

extern ble_app_config_t g_ble_config;

int ble_config_init(void);
int ble_config_save_provision(void);
int ble_config_set_provision_state(int provision_state, int network_state,
				   const char *ssid, const char *password,
				   const char *ip);
int ble_config_set_network_mode(int provision_state, int network_state,
				int network_mode);
int ble_config_set_hotspot(const char *ssid, const char *password, int channel);
int ble_config_reset_provision(void);
int ble_config_clear_provision_error(void);
int ble_config_persist_provision_error(int error_code, const char *stage,
				       const char *reason);

bool ble_config_is_provisioned(void);
bool ble_config_is_ap_mode(void);

void ble_config_build_default_hotspot_ssid(char *dst, size_t size,
					   const char *serial_no);

#endif /* BLE_APP_CONFIG_H */
