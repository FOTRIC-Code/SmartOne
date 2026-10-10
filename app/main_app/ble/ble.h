#ifndef BLE_APP_BLE_H
#define BLE_APP_BLE_H

#include <stdbool.h>

void ble_init(void);
void ble_poll(void);
void ble_poll_commands(void);
bool ble_is_running(void);
void ble_notify_provision_result(bool success, const char *ip);

/*
 * AIC8800DC combo-chip coexistence: BLE and Wi-Fi share the same radio.
 * ble_stop()    — tear down BLE stack + kill bluetoothd so Wi-Fi can work.
 * ble_restart() — bring bluetoothd and BLE advertising back up.
 */
void ble_stop(void);
void ble_restart(void);

#endif
