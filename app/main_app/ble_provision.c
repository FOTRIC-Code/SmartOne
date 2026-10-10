#include "ble_provision.h"

#include "ble/ble.h"
#include "ble_config.h"
#include "ble_wifi.h"
#include "led/ble_led.h"
#include "libs/log/log.h"
#include "provision_shared.h"
#include "libs/time/time_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PROVISION_REQUEST_DELAY_SEC 2
#define PROVISION_STA_ERROR_CODE    300101

static ble_prov_run_status_t s_run;
static char s_run_ssid[64];
static char s_run_password[128];
static bool s_run_pending;
static time_t s_run_start_at;
static bool s_restart_app;

static bool s_commissioning_window_forced;
static time_t s_commissioning_window_until;

static void set_str(char *dst, size_t size, const char *src)
{
	if (dst && size > 0)
		snprintf(dst, size, "%s", src ? src : "");
}

static void set_stage(const char *stage, int progress)
{
	set_str(s_run.stage, sizeof(s_run.stage), stage);
	s_run.progress = progress;
}

bool ble_provision_is_commissioning_open(void)
{
	time_t now;

	if (g_ble_config.provision.provision_state != PROVISION_STATE_PROVISIONED)
		return true;
	if (!s_commissioning_window_forced)
		return false;
	if (s_commissioning_window_until == 0)
		return true;

	now = time_now_sec();
	if (now <= s_commissioning_window_until)
		return true;

	s_commissioning_window_forced = false;
	s_commissioning_window_until = 0;
	return false;
}

void ble_provision_open_commissioning_window(unsigned int duration_sec)
{
	if (s_commissioning_window_forced &&
	    s_commissioning_window_until == 0 && duration_sec == 0)
		return;

	s_commissioning_window_forced = true;
	s_commissioning_window_until =
		duration_sec == 0 ? 0 : time_now_sec() + duration_sec;
}

void ble_provision_close_commissioning_window(void)
{
	s_commissioning_window_forced = false;
	s_commissioning_window_until = 0;
}

static void write_trigger_file(const char *ip)
{
	FILE *fp = fopen(PROVISION_TRIGGER_PATH, "w");

	if (!fp) {
		LOGW("ble_provision: failed to write trigger file %s",
		     PROVISION_TRIGGER_PATH);
		return;
	}
	fprintf(fp, "%s\n", ip ? ip : "");
	fclose(fp);
	LOGI("ble_provision: wrote trigger file %s", PROVISION_TRIGGER_PATH);
}

void ble_provision_init(void)
{
	memset(&s_run, 0, sizeof(s_run));
	s_run_pending = false;
	s_restart_app = false;
}

int ble_provision_request_wifi(const char *ssid, const char *password)
{
	if (!ssid || ssid[0] == '\0')
		return -EINVAL;

	if (s_run_pending || s_run.state == BLE_PROV_RUN_CONNECTING)
		return -EBUSY;

	set_str(s_run_ssid, sizeof(s_run_ssid), ssid);
	set_str(s_run_password, sizeof(s_run_password), password);
	memset(&s_run, 0, sizeof(s_run));
	s_run.state = BLE_PROV_RUN_CONNECTING;
	set_str(s_run.ssid, sizeof(s_run.ssid), ssid);
	set_stage("queued", 5);
	s_run_pending = true;
	s_run_start_at = time_now_sec() + PROVISION_REQUEST_DELAY_SEC;

	ble_config_set_provision_state(PROVISION_STATE_PROVISIONING,
				       NETWORK_STATE_OFFLINE,
				       g_ble_config.provision.ssid, NULL, "");
	LOGI("ble_provision: request accepted ssid=%s (start in %ds)",
	     ssid, PROVISION_REQUEST_DELAY_SEC);
	return 0;
}

void ble_provision_get_run_status(ble_prov_run_status_t *out)
{
	if (out)
		*out = s_run;
}

static void run_wifi_provision(void)
{
	char ip[48] = {0};

	LOGI("ble_provision: starting Wi-Fi provision ssid=%s", s_run_ssid);
	ble_led_set_state(BLE_LED_PROVISIONING);

	set_stage("preparing", 15);
	if (ble_wifi_ensure_ready() != 0) {
		set_str(s_run.error_reason, sizeof(s_run.error_reason),
			"wifi_not_ready");
		goto fail;
	}

	set_stage("connecting", 40);
	if (ble_wifi_connect(s_run_ssid, s_run_password, ip, sizeof(ip)) != 0) {
		set_str(s_run.error_reason, sizeof(s_run.error_reason),
			"wifi_connect_failed");
		goto fail;
	}

	g_ble_config.provision.network_mode = NETWORK_MODE_STA;
	ble_config_set_provision_state(PROVISION_STATE_PROVISIONED,
				       NETWORK_STATE_ONLINE,
				       s_run_ssid, s_run_password, ip);
	ble_config_clear_provision_error();

	s_run.state = BLE_PROV_RUN_SUCCESS;
	set_str(s_run.ip, sizeof(s_run.ip), ip);
	set_stage("connected", 100);

	write_trigger_file(ip);
	s_restart_app = true;

	LOGI("ble_provision: success ssid=%s ip=%s", s_run_ssid, ip);
	return;

fail:
	s_run.state = BLE_PROV_RUN_FAILED;
	s_run.error_code = PROVISION_STA_ERROR_CODE;
	set_str(s_run.stage, sizeof(s_run.stage), "failed");
	s_run.progress = 100;

	/* Roll back provision_state so the device is ready for retry */
	g_ble_config.provision.provision_state = PROVISION_STATE_AP_READY;
	g_ble_config.provision.network_state = NETWORK_STATE_OFFLINE;

	ble_config_persist_provision_error(PROVISION_STA_ERROR_CODE,
					   "connecting_wifi",
					   s_run.error_reason);
	ble_led_set_state(BLE_LED_WAIT_PROVISION);
	LOGW("ble_provision: failed ssid=%s reason=%s",
	     s_run_ssid, s_run.error_reason);
}

void ble_provision_poll(void)
{
	if (!s_run_pending)
		return;
	if (time_now_sec() < s_run_start_at)
		return;

	s_run_pending = false;
	run_wifi_provision();
}

bool ble_provision_should_restart_app(void)
{
	if (s_restart_app) {
		s_restart_app = false;
		return true;
	}
	return false;
}

int ble_provision_request_ap(const char *ssid, const char *password,
			    int channel)
{
	int rc;

	if (!ssid || ssid[0] == '\0')
		return -EINVAL;

	LOGI("ble_provision: starting AP mode ssid=%s", ssid);
	ble_led_set_state(BLE_LED_PROVISIONING);

	if (ble_wifi_ensure_ready() != 0) {
		LOGE("ble_provision: Wi-Fi not ready for AP mode");
		ble_config_persist_provision_error(300102,
						   "start_ap",
						   "wifi_not_ready");
		ble_led_set_state(BLE_LED_WAIT_PROVISION);
		return -1;
	}

	rc = ble_wifi_start_ap(ssid, password, channel);
	if (rc != 0) {
		LOGE("ble_provision: AP start failed");
		ble_config_persist_provision_error(300102,
						   "start_ap",
						   "ap_start_failed");
		ble_led_set_state(BLE_LED_WAIT_PROVISION);
		return -1;
	}

	ble_config_set_network_mode(PROVISION_STATE_PROVISIONED,
				    NETWORK_STATE_ONLINE,
				    NETWORK_MODE_AP);
	ble_config_clear_provision_error();
	s_restart_app = true;

	ble_led_set_state(BLE_LED_ONLINE);
	LOGI("ble_provision: AP mode started ssid=%s", ssid);
	return 0;
}

int ble_provision_reset(void)
{
	if (s_run_pending || s_run.state == BLE_PROV_RUN_CONNECTING)
		return -EBUSY;

	memset(&s_run, 0, sizeof(s_run));
	s_run_pending = false;
	ble_provision_close_commissioning_window();

	if (ble_config_reset_provision() != 0)
		return -1;

	unlink(PROVISION_TRIGGER_PATH);
	LOGI("ble_provision: reset complete");
	return 0;
}
