#ifndef BLE_APP_LED_BLE_LED_H
#define BLE_APP_LED_BLE_LED_H

typedef enum ble_led_state {
	BLE_LED_WAIT_PROVISION,
	BLE_LED_PROVISIONING,
	BLE_LED_ONLINE,
	BLE_LED_OFFLINE,
	BLE_LED_FAILED
} ble_led_state_t;

int ble_led_init(void);
void ble_led_deinit(void);
int ble_led_set_state(ble_led_state_t state);

#endif
