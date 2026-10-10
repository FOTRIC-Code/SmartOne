#ifndef BLE_APP_WIFI_H
#define BLE_APP_WIFI_H

#include <stdbool.h>
#include <stddef.h>


int ble_wifi_connect(const char *ssid, const char *password,
		     char *ip_out, size_t ip_out_size);
int ble_wifi_ensure_ready(void);
bool ble_wifi_is_connected(char *ip_out, size_t ip_out_size);
int ble_wifi_start_ap(const char *ssid, const char *password, int channel);
bool ble_wifi_try_recover(void);
void ble_wifi_ensure_dhcp_daemon(void);

#endif /* BLE_APP_WIFI_H */
