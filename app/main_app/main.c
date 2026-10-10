/*
 * main_app — sole entry-point daemon for Parkia.Cube.
 *
 * Boot sequence:
 *
 *   main_app 启动 (daemon)
 *   ├── 1. 无条件启动 net_app（即使未配网也先拉起）
 *   ├── 2. 如果已有配网信息 → 连接 Wi-Fi（不启动 BLE）
 *   ├── 3. 如果未配网 → 启动 BLE GATT 广播
 *   └── 4. 主循环：
 *       ├── 未联网 → ble_poll() + ble_provision_poll()
 *       ├── 已联网 → ble_poll_commands()（BLE 已停止时为 no-op）
 *       ├── 守护 app_bin（默认 net_app，自定义程序同一路径）：
 *       │     崩溃后按 1/2/4…30s 退避重启；60s 内退出超过 5 次则停止拉起
 *       └── 配网成功 → 立刻重启 app_bin → 10s 后关闭 BLE
 *
 * After successful provisioning the BLE stack is kept alive for 10s
 * so the App can receive the GATT success notification.  Then BLE is
 * fully stopped (bluetoothd killed, hci0 powered down) because the
 * AIC8800DC combo chip shares one radio — keeping BT alive stalls
 * the Wi-Fi data plane.
 * To re-provision, the user must long-press the reset button, which
 * clears the provisioned state and reboots into BLE mode.
 *
 * net_app is NEVER auto-started by an init script. It is always launched
 * and managed by main_app.
 */
#include "ble/ble.h"
#include "ble_button.h"
#include "ble_config.h"
#include "ble_provision.h"
#include "ble_wifi.h"
#include "led/ble_led.h"
#include "libs/log/log.h"
#include "provision_shared.h"
#include "libs/time/time_util.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NET_CHECK_INTERVAL_SEC   10
#define BLE_SHUTDOWN_DELAY_SEC   10
#define PING_FAIL_RECOVER_THRESHOLD 3  /* consecutive failures before recovery */

#define APP_BACKOFF_MIN_SEC  1
#define APP_BACKOFF_MAX_SEC  30
#define APP_STABLE_SEC       60
#define APP_CRASH_LIMIT      5
#define APP_CRASH_WINDOW_SEC 60

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signo)
{
	(void)signo;
	g_running = 0;
}

/* ── default app process management ──────────────────────────────── */

static const char *app_basename_from_path(const char *path)
{
	const char *name;

	if (!path || path[0] == '\0')
		return "net_app";

	name = strrchr(path, '/');
	return (name && name[1] != '\0') ? name + 1 : path;
}

static pid_t g_app_pid;
static int g_app_stop_requested;
static int g_app_give_up;
static int g_app_stable;
static int g_app_backoff_sec = APP_BACKOFF_MIN_SEC;
static int g_app_crash_count;
static time_t g_app_started_at;
static time_t g_app_restart_at;
static time_t g_app_crash_window_start;

static time_t mono_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec;
}

static const char *managed_app_bin(void)
{
	if (g_ble_config.settings.app_bin[0] == '\0')
		return BLE_APP_BIN_DEFAULT;
	return g_ble_config.settings.app_bin;
}

static void stop_app(void)
{
	int status;
	int i;

	if (g_app_pid <= 0)
		return;

	g_app_stop_requested = 1;
	if (kill(g_app_pid, SIGTERM) != 0 && errno == ESRCH) {
		waitpid(g_app_pid, &status, WNOHANG);
		g_app_pid = 0;
		g_app_stop_requested = 0;
		return;
	}

	for (i = 0; i < 40; i++) {
		if (waitpid(g_app_pid, &status, WNOHANG) == g_app_pid) {
			g_app_pid = 0;
			g_app_stop_requested = 0;
			return;
		}
		usleep(50 * 1000);
	}

	kill(g_app_pid, SIGKILL);
	waitpid(g_app_pid, &status, 0);
	g_app_pid = 0;
	g_app_stop_requested = 0;
}

static int spawn_app(void)
{
	const char *bin = managed_app_bin();
	const char *app_name = app_basename_from_path(bin);
	pid_t pid;

	if (access(bin, X_OK) != 0) {
		LOGE("main_app: cannot execute %s (%s), stop restarting",
		     bin, strerror(errno));
		g_app_give_up = 1;
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		LOGE("main_app: fork %s failed: %s", app_name, strerror(errno));
		return -1;
	}
	if (pid == 0) {
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		execl(bin, app_name, (char *)NULL);
		_exit(127);
	}

	g_app_pid = pid;
	g_app_started_at = mono_sec();
	g_app_stable = 0;
	g_app_stop_requested = 0;
	g_app_restart_at = 0;
	LOGI("main_app: launched %s (%s) pid=%d", app_name, bin, (int)pid);
	return 0;
}

/* Intentional start/restart: stop the known child and launch immediately. */
static void launch_app(void)
{
	const char *bin = managed_app_bin();

	if (g_app_pid > 0)
		LOGI("main_app: restarting %s", app_basename_from_path(bin));
	stop_app();
	g_app_give_up = 0;
	g_app_backoff_sec = APP_BACKOFF_MIN_SEC;
	g_app_crash_count = 0;
	g_app_crash_window_start = 0;
	g_app_restart_at = 0;
	(void)spawn_app();
}

static void note_app_exit(int status)
{
	const char *app_name = app_basename_from_path(managed_app_bin());
	time_t now = mono_sec();

	if (WIFSIGNALED(status)) {
		LOGW("main_app: %s pid exited signal=%d", app_name,
		     WTERMSIG(status));
	} else {
		LOGW("main_app: %s pid exited status=%d", app_name,
		     WIFEXITED(status) ? WEXITSTATUS(status) : status);
	}

	if (g_app_crash_window_start == 0 ||
	    now - g_app_crash_window_start > APP_CRASH_WINDOW_SEC) {
		g_app_crash_window_start = now;
		g_app_crash_count = 1;
	} else {
		g_app_crash_count++;
	}

	if (g_app_crash_count > APP_CRASH_LIMIT) {
		LOGE("main_app: %s exited %d times in %ds, stop restarting",
		     app_name, g_app_crash_count, APP_CRASH_WINDOW_SEC);
		g_app_give_up = 1;
		return;
	}

	g_app_restart_at = now + g_app_backoff_sec;
	LOGW("main_app: restart %s in %ds", app_name, g_app_backoff_sec);
	if (g_app_backoff_sec < APP_BACKOFF_MAX_SEC) {
		g_app_backoff_sec *= 2;
		if (g_app_backoff_sec > APP_BACKOFF_MAX_SEC)
			g_app_backoff_sec = APP_BACKOFF_MAX_SEC;
	}
}

/* Called every main-loop turn (~10ms). */
static void supervise_app(void)
{
	int status;
	pid_t pid;
	time_t now;

	if (g_app_pid > 0) {
		pid = waitpid(g_app_pid, &status, WNOHANG);
		if (pid == 0) {
			if (!g_app_stable &&
			    mono_sec() - g_app_started_at >= APP_STABLE_SEC) {
				g_app_stable = 1;
				g_app_backoff_sec = APP_BACKOFF_MIN_SEC;
				g_app_crash_count = 0;
				g_app_crash_window_start = 0;
			}
			return;
		}
		if (pid == g_app_pid) {
			g_app_pid = 0;
			if (g_app_stop_requested) {
				g_app_stop_requested = 0;
				return;
			}
			note_app_exit(status);
			return;
		}
		return;
	}

	if (g_app_give_up)
		return;

	now = mono_sec();
	if (g_app_restart_at != 0 && now < g_app_restart_at)
		return;

	if (spawn_app() != 0 && !g_app_give_up) {
		g_app_restart_at = now + g_app_backoff_sec;
		if (g_app_backoff_sec < APP_BACKOFF_MAX_SEC) {
			g_app_backoff_sec *= 2;
			if (g_app_backoff_sec > APP_BACKOFF_MAX_SEC)
				g_app_backoff_sec = APP_BACKOFF_MAX_SEC;
		}
	}
}

/* ── trigger file (for legacy compatibility) ───────────────────── */

static void write_trigger_file(const char *ip)
{
	FILE *fp = fopen(PROVISION_TRIGGER_PATH, "w");

	if (!fp)
		return;
	fprintf(fp, "%s\n", ip ? ip : "");
	fclose(fp);
}

/* ── boot-time Wi-Fi reconnect ──────────────────────────────────── */

static bool boot_wifi_reconnect(void)
{
	char ip[48] = {0};

	LOGI("main_app: device provisioned, ssid=%s",
	     g_ble_config.provision.ssid);

	if (ble_wifi_is_connected(ip, sizeof(ip))) {
		LOGI("main_app: already online ip=%s", ip);
		ble_led_set_state(BLE_LED_ONLINE);
		ble_wifi_ensure_dhcp_daemon();
		write_trigger_file(ip);
		launch_app();
		return true;
	}

	LOGI("main_app: reconnecting Wi-Fi ssid=%s pwd=%s",
	     g_ble_config.provision.ssid,
	     g_ble_config.provision.password[0] ? "***" : "(empty)");
	ble_led_set_state(BLE_LED_PROVISIONING);

	if (ble_wifi_ensure_ready() != 0) {
		LOGW("main_app: Wi-Fi not ready, staying in BLE listen mode");
		ble_config_persist_provision_error(
			300101, "boot_reconnect", "wifi_not_ready");
		ble_led_set_state(BLE_LED_OFFLINE);
		return false;
	}

	if (ble_wifi_connect(g_ble_config.provision.ssid,
			     g_ble_config.provision.password,
			     ip, sizeof(ip)) == 0) {
		LOGI("main_app: reconnected ip=%s", ip);
		ble_led_set_state(BLE_LED_ONLINE);

		ble_config_set_provision_state(
			PROVISION_STATE_PROVISIONED,
			NETWORK_STATE_ONLINE,
			g_ble_config.provision.ssid,
			NULL, ip);
		ble_config_clear_provision_error();
		write_trigger_file(ip);
		launch_app();
		return true;
	}

	LOGW("main_app: Wi-Fi reconnect failed, staying in BLE listen mode");
	ble_config_persist_provision_error(
		300101, "boot_reconnect", "wifi_connect_failed");
	ble_led_set_state(BLE_LED_OFFLINE);
	return false;
}

/* ── boot-time AP restore ───────────────────────────────────────── */

static bool boot_ap_restore(void)
{
	int rc;

	LOGI("main_app: restoring AP mode ssid=%s",
	     g_ble_config.provision.hotspot_ssid);
	ble_led_set_state(BLE_LED_PROVISIONING);

	rc = ble_wifi_start_ap(g_ble_config.provision.hotspot_ssid,
			       g_ble_config.provision.hotspot_password,
			       g_ble_config.provision.hotspot_channel);
	if (rc != 0) {
		LOGW("main_app: AP restore failed");
		ble_led_set_state(BLE_LED_WAIT_PROVISION);
		return false;
	}

	ble_led_set_state(BLE_LED_ONLINE);
	LOGI("main_app: AP mode restored");
	return true;
}

/* ── main ───────────────────────────────────────────────────────── */

int main(void)
{
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	log_init();
	LOGI("%s %s starting (system entry point)",
	     BLE_APP_NAME, BLE_APP_VERSION);

	if (ble_config_init() != 0) {
		LOGE("ble_config_init failed");
		return 1;
	}

	ble_led_init();
	ble_provision_init();
	ble_button_init();

	/*
	 * Step 1: Always launch net_app first.
	 */
	launch_app();

	/*
	 * Disable USB autosuspend for the AIC8800DC combo chip.
	 * The default timeout (2 s) puts the chip to sleep when idle,
	 * causing periodic Wi-Fi drops.  Setting power/control to "on"
	 * disables runtime PM entirely.  Done here (after launch_app)
	 * so USB devices are already enumerated.
	 */
	(void)system(
		"for d in /sys/bus/usb/devices/[0-9]*/power; do "
		"echo on  > \"$d/control\"   2>/dev/null; "
		"echo -1  > \"$d/autosuspend\" 2>/dev/null; "
		"done");

	/*
	 * Step 2: If already provisioned, reconnect Wi-Fi.
	 * On success, restart net_app with the new network.
	 */
	bool wifi_up = false;
	bool net_online = false;
	time_t next_net_check = 0;
	time_t ble_shutdown_at = 0;
	int ping_fail_count = 0;

	if (ble_config_is_ap_mode()) {
		wifi_up = boot_ap_restore();
	} else if (ble_config_is_provisioned()) {
		wifi_up = boot_wifi_reconnect();
	} else {
		LOGI("main_app: not provisioned, waiting for BLE config");
		ble_led_set_state(BLE_LED_WAIT_PROVISION);
	}

	net_online = wifi_up;

	/*
	 * Step 3: Only initialise BLE when NOT provisioned.
	 *
	 * AIC8800DC combo-chip coex: BLE and Wi-Fi share a single radio.
	 * When the device is already provisioned (STA or AP), skip BLE
	 * entirely so the radio is dedicated to Wi-Fi from boot.
	 * Re-provisioning requires a long-press reset button.
	 */
	if (!wifi_up) {
		ble_init();
		LOGI("main_app: BLE started (device not provisioned)");
	} else {
		LOGI("main_app: BLE skipped (device already provisioned)");
	}

	/*
	 * Step 4: Main loop.
	 */
	while (g_running) {
		if (!wifi_up) {
			ble_poll();
		} else {
			ble_poll_commands();
		}

		ble_provision_poll();

		if (ble_button_poll()) {
			LOGI("main_app: factory reset triggered by button");
			wifi_up = false;
			net_online = false;
			/* ble_button_poll() already called ble_restart(),
			 * cleared provisioning, and set LED. The existing
			 * loop logic will resume full BLE polling. */
		}

		if (ble_provision_should_restart_app()) {
			ble_led_set_state(BLE_LED_ONLINE);
			launch_app();
			wifi_up = true;
			net_online = true;
			next_net_check = time_now_sec() + NET_CHECK_INTERVAL_SEC;
			ble_shutdown_at = time_now_sec() + BLE_SHUTDOWN_DELAY_SEC;
			LOGI("main_app: Wi-Fi provisioned, app restarted; "
			     "BLE shutdown in %ds (combo chip coex)",
			     BLE_SHUTDOWN_DELAY_SEC);
		}

		/*
		 * Deferred BLE shutdown: the combo chip shares one radio
		 * between BT and Wi-Fi.  Keeping bluetoothd alive stalls
		 * the Wi-Fi data plane.  Wait long enough for the App to
		 * receive the GATT success notification, then kill BLE.
		 */
		if (ble_shutdown_at && time_now_sec() >= ble_shutdown_at) {
			ble_shutdown_at = 0;
			ble_stop();
			LOGI("main_app: BLE stopped (combo chip coex)");
		}

		/* ── Network monitoring + auto-recovery ──────────── */
		if (wifi_up) {
			time_t now = time_now_sec();

			if (now >= next_net_check) {
				next_net_check = now + NET_CHECK_INTERVAL_SEC;
				char check_ip[48] = {0};
				bool reachable = ble_wifi_is_connected(
					check_ip, sizeof(check_ip));

				if (!reachable && net_online) {
					ping_fail_count++;
					LOGW("main_app: ping gateway failed "
					     "(%d/%d)", ping_fail_count,
					     PING_FAIL_RECOVER_THRESHOLD);

					if (ping_fail_count >=
					    PING_FAIL_RECOVER_THRESHOLD) {
						ble_led_set_state(
							BLE_LED_OFFLINE);
						LOGI("main_app: attempting "
						     "Wi-Fi recovery");
						net_online = false;
						if (ble_wifi_try_recover()) {
							LOGI("main_app: "
							     "Wi-Fi recovered");
							ble_led_set_state(
								BLE_LED_ONLINE);
							net_online = true;
						} else {
							LOGW("main_app: "
							     "Wi-Fi recovery "
							     "failed");
						}
						ping_fail_count = 0;
						next_net_check =
							time(NULL) +
							NET_CHECK_INTERVAL_SEC;
					}
				} else if (reachable) {
					if (!net_online ||
					    ping_fail_count > 0) {
						if (!net_online)
							LOGI("main_app: "
							     "Wi-Fi online "
							     "ip=%s",
							     check_ip);
						ble_led_set_state(
							BLE_LED_ONLINE);
						net_online = true;
					}
					ping_fail_count = 0;
				}
			}
		}

		/*
		 * If the device was reset to unprovisioned (via BLE
		 * reset_provision or button), restart BLE advertising
		 * so the device can be discovered and re-provisioned.
		 */
		if (wifi_up && !ble_config_is_provisioned() &&
		    !ble_config_is_ap_mode()) {
			wifi_up = false;
			net_online = false;
			ble_led_set_state(BLE_LED_WAIT_PROVISION);
			ble_restart();
			LOGI("main_app: device reset to unprovisioned, "
			     "BLE advertising restarted");
		}

		supervise_app();
		usleep(10 * 1000);
	}

	stop_app();
	ble_led_deinit();
	LOGI("%s stopping", BLE_APP_NAME);
	log_close();
	return 0;
}
