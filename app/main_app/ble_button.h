#ifndef BLE_APP_BUTTON_H
#define BLE_APP_BUTTON_H

#include <stdbool.h>

/*
 * RECOVERY button monitor (SARADC IN1 long-press).
 *
 * A long-press (>= 3 s) clears the provisioning state, restarts BLE
 * advertising, and sets the LED to WAIT_PROVISION so the device can
 * be re-provisioned.
 *
 * If the SARADC IIO device is not available, init returns -1 and
 * poll is a silent no-op.
 */

/* Find the correct IIO device and prepare for polling. */
int ble_button_init(void);

/*
 * Poll the button state; call from the main loop.
 * Returns true once when a long-press reset is triggered.
 */
bool ble_button_poll(void);

#endif /* BLE_APP_BUTTON_H */
