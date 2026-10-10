#ifndef BLE_APP_PROVISION_H
#define BLE_APP_PROVISION_H

#include <stdbool.h>

/*
 * main_app provisioning state machine.
 * Sits between the BLE GATT command handler and the Wi-Fi connect layer.
 */

typedef enum ble_prov_run_state {
	BLE_PROV_RUN_IDLE = 0,
	BLE_PROV_RUN_CONNECTING = 1,
	BLE_PROV_RUN_SUCCESS = 2,
	BLE_PROV_RUN_FAILED = 3
} ble_prov_run_state_t;

typedef struct ble_prov_run_status {
	ble_prov_run_state_t state;
	int progress;
	char stage[32];
	char ssid[64];
	char ip[48];
	int error_code;
	char error_reason[64];
} ble_prov_run_status_t;

void ble_provision_init(void);
void ble_provision_poll(void);

int ble_provision_request_wifi(const char *ssid, const char *password);
int ble_provision_request_ap(const char *ssid, const char *password,
			     int channel);
void ble_provision_get_run_status(ble_prov_run_status_t *out);

bool ble_provision_is_commissioning_open(void);
void ble_provision_open_commissioning_window(unsigned int duration_sec);
void ble_provision_close_commissioning_window(void);

int ble_provision_scan_wifi(void);

int ble_provision_reset(void);

bool ble_provision_should_restart_app(void);

#endif /* BLE_APP_PROVISION_H */
