/* Internal to ble/ — include only from ble.c and ble_advertiser.c. */
#ifndef NET_APP_BLE_BLE_ADVERTISER_H
#define NET_APP_BLE_BLE_ADVERTISER_H

#include <stdbool.h>

#define BLE_SERVICE_UUID "00001234-0000-1000-8000-00805F9B34FB"
#define BLE_CHR_WRITE_UUID "00001235-0000-1000-8000-00805F9B34FB"
#define BLE_CHR_NOTIFY_UUID "00001236-0000-1000-8000-00805F9B34FB"

typedef struct net_ble_advertisement {
	const char *device_id;
	const char *product_id;
	const char *model;
	const char *serial_no;
	const char *hotspot_ssid;
	const char *http_host;
	int http_port_no;
} net_ble_advertisement_t;

int net_ble_advertiser_start(const net_ble_advertisement_t *adv);
int net_ble_advertiser_stop(void);
/*
 * True while BLE is in play: advertising, connected to a central, or holding
 * the stack open. Restarting the shared Wi-Fi/BT radio takes hci0 with it, so
 * callers that touch the combo must check this, not the advertising state.
 */
bool net_ble_is_running(void);

/**
 * Poll for deferred BLE provision requests.
 * Must be called from main loop context (not from BLE callback).
 * Returns true if a provision request was processed.
 */
bool net_ble_poll(void);

/**
 * Lightweight poll that only drains the BLE command queue and the
 * async wifi_sta_apply status poller.  Skips ble_poll_advertising_status()
 * which runs `bluetoothctl show` and triggers HCI activity that starves
 * the AIC8800DC Wi-Fi data plane via the coex timer.
 *
 * Call this from the main loop when Wi-Fi is connected to keep BLE
 * commands responsive without interfering with Wi-Fi traffic.
 */
bool net_ble_poll_commands(void);

/**
 * Send provision result back to phone via BLE notify.
 * Call after WiFi connect attempt completes.
 */
void net_ble_notify_provision_result(bool success, const char *ip);

#endif
