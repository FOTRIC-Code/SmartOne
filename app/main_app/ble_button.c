/*
 * ble_button.c — RECOVERY button (SARADC IN1) long-press handler.
 *
 * Hardware: The RECOVERY button is connected to SARADC channel 1.
 * Vref = 1.8 V, 10-bit ADC (0–1023).
 *   Button pressed  → ADC ≈ 0   (shorted to GND)
 *   Button released → ADC ≈ 1023 (pulled up to Vref)
 *
 * A continuous press of >= LONG_PRESS_SEC seconds triggers a
 * factory-reset sequence: clear provisioning, restart BLE, and
 * set the LED to WAIT_PROVISION.
 */
#include "ble_button.h"

#include "ble/ble.h"
#include "ble_config.h"
#include "ble_provision.h"
#include "ble_wifi.h"
#include "led/ble_led.h"
#include "libs/log/log.h"
#include "libs/time/time_util.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ── tunables ────────────────────────────────────────────────────── */

#define SARADC_CHANNEL        1
#define ADC_PRESSED_THRESHOLD 100   /* raw < this → button pressed    */
#define LONG_PRESS_SEC        3     /* seconds held to trigger reset  */

/* ── state ───────────────────────────────────────────────────────── */

static char s_adc_path[256];       /* sysfs path to in_voltageN_raw  */
static bool s_inited;              /* true if IIO device was found    */

static bool s_pressed;             /* debounced: button is held       */
static time_t s_press_start;       /* monotonic time when press began */
static bool s_triggered;           /* latch: reset already fired      */

/* ── helpers ─────────────────────────────────────────────────────── */

static int read_adc_raw(void)
{
	FILE *fp;
	int val = -1;

	fp = fopen(s_adc_path, "r");
	if (!fp)
		return -1;
	if (fscanf(fp, "%d", &val) != 1)
		val = -1;
	fclose(fp);
	return val;
}

/*
 * Scan /sys/bus/iio/devices/iio:device* for a device that has the
 * in_voltage<CHANNEL>_raw file.  The RK3506 SARADC driver registers
 * as "ff4e8000.adc" (platform device address), not the generic
 * "rockchip_saradc" name, so match by channel file presence instead.
 */
static int find_iio_device(void)
{
	static const char iio_base[] = "/sys/bus/iio/devices";
	DIR *dir;
	struct dirent *ent;
	char path[256];

	dir = opendir(iio_base);
	if (!dir) {
		LOGW("ble_button: cannot open %s", iio_base);
		return -1;
	}

	while ((ent = readdir(dir)) != NULL) {
		if (strncmp(ent->d_name, "iio:device", 10) != 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s/in_voltage%d_raw",
			 iio_base, ent->d_name, SARADC_CHANNEL);
		if (access(path, R_OK) == 0) {
			snprintf(s_adc_path, sizeof(s_adc_path),
				 "%s", path);
			closedir(dir);
			return 0;
		}
	}

	closedir(dir);
	return -1;
}

static void do_factory_reset(void)
{
	LOGI("ble_button: === LONG-PRESS FACTORY RESET ===");

	/* 1. Disconnect Wi-Fi (best-effort) */
	LOGI("ble_button: disconnecting Wi-Fi");
	(void)system("wpa_cli -i wlan0 disconnect >/dev/null 2>&1");

	/* 2. Clear provisioning state */
	if (ble_provision_reset() != 0)
		LOGW("ble_button: ble_provision_reset failed (ignored)");

	/* 3. LED → waiting for provision */
	ble_led_set_state(BLE_LED_WAIT_PROVISION);

	/* 4. Restart BLE stack so advertising resumes */
	ble_restart();

	LOGI("ble_button: factory reset complete, awaiting BLE provisioning");
}

/* ── public API ──────────────────────────────────────────────────── */

int ble_button_init(void)
{
	if (find_iio_device() != 0) {
		LOGW("ble_button: SARADC IIO device not found, "
		     "button disabled");
		s_inited = false;
		return -1;
	}

	/* Quick sanity read */
	int raw = read_adc_raw();

	if (raw < 0) {
		LOGW("ble_button: cannot read %s, button disabled",
		     s_adc_path);
		s_inited = false;
		return -1;
	}

	s_pressed = false;
	s_press_start = 0;
	s_triggered = false;
	s_inited = true;

	LOGI("ble_button: init OK, adc=%s (raw=%d)", s_adc_path, raw);
	return 0;
}

bool ble_button_poll(void)
{
	int raw;
	bool is_down;
	time_t now;

	if (!s_inited)
		return false;

	raw = read_adc_raw();
	if (raw < 0)
		return false;

	is_down = (raw < ADC_PRESSED_THRESHOLD);
	now = time_now_sec();

	if (is_down && !s_pressed) {
		/* Rising edge: button just pressed */
		s_pressed = true;
		s_press_start = now;
		s_triggered = false;
		LOGD("ble_button: pressed (raw=%d)", raw);
	} else if (!is_down && s_pressed) {
		/* Falling edge: button released */
		s_pressed = false;
		s_triggered = false;
		LOGD("ble_button: released (raw=%d)", raw);
	}

	if (s_pressed && !s_triggered &&
	    (now - s_press_start) >= LONG_PRESS_SEC) {
		s_triggered = true;
		LOGI("ble_button: long-press detected (%d s)",
		     (int)(now - s_press_start));
		do_factory_reset();
		return true;
	}

	return false;
}
