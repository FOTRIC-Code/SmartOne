#include "ble_wifi.h"

#include "libs/log/log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define WIFI_IFACE "wlan0"
#define WIFI_WPA_CONF "/tmp/wpa_supplicant.conf"
#define WIFI_WPA_CTRL_DIR "/var/run/wpa_supplicant"
#define WIFI_WPA_LOG "/tmp/wpa_supplicant_ble.log"
#define UDHCPC_TIMEOUT_SEC 15
#define UDHCPC_PID        "/var/run/udhcpc.wlan0.pid"
#define WPA_COMPLETED_WAIT_SEC 35

static int run_cmd(const char *cmd)
{
	int rc = system(cmd);

	if (rc == -1)
		return -1;
	return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

static int read_cmd_output(const char *cmd, char *buf, size_t buf_size)
{
	FILE *fp;
	size_t used = 0;

	if (!cmd || !buf || buf_size == 0)
		return -1;

	buf[0] = '\0';
	fp = popen(cmd, "r");
	if (!fp)
		return -1;

	while (used + 1 < buf_size) {
		size_t n = fread(buf + used, 1, buf_size - used - 1, fp);

		used += n;
		if (n == 0)
			break;
	}
	buf[used] = '\0';
	return pclose(fp);
}

static void trim_newline(char *s)
{
	size_t len;

	if (!s)
		return;
	len = strlen(s);
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
		s[--len] = '\0';
}

static int process_running(const char *name)
{
	char cmd[96];

	snprintf(cmd, sizeof(cmd), "pidof %s >/dev/null 2>&1", name);
	return run_cmd(cmd) == 0;
}

static void log_file_tail(const char *label, const char *path, int max_lines)
{
	char cmd[256];
	char buf[1024];

	snprintf(cmd, sizeof(cmd), "tail -n %d %s 2>/dev/null", max_lines, path);
	if (read_cmd_output(cmd, buf, sizeof(buf)) == 0 && buf[0]) {
		trim_newline(buf);
		LOGI("ble_wifi: %s:\n%s", label, buf);
	}
}

/*
 * Generate wpa_supplicant.conf directly in C.
 *
 * Byte-for-byte identical to what wifi-connect.sh produces: copy
 * /etc/wpa_supplicant.conf → sed SSID / PASSWORD. No wpa_passphrase
 * needed (the binary is not shipped on this image).
 *
 * The AIC8800DC supplicant has been observed to fail association when
 * extra directives (proto, pairwise, scan_ssid, ieee80211w) are present;
 * the stock template deliberately omits them.
 */
static int write_wpa_config(const char *ssid, const char *password)
{
	FILE *fp;

	fp = fopen(WIFI_WPA_CONF, "w");
	if (!fp) {
		LOGE("ble_wifi: failed to open %s for writing", WIFI_WPA_CONF);
		return -1;
	}

	fprintf(fp, "ctrl_interface=%s\n", WIFI_WPA_CTRL_DIR);
	fprintf(fp, "ap_scan=1\n");
	fprintf(fp, "update_config=1\n");
	fprintf(fp, "\n");
	fprintf(fp, "network={\n");
	fprintf(fp, "\tssid=\"%s\"\n", ssid);
	if (password && password[0]) {
		fprintf(fp, "\tpsk=\"%s\"\n", password);
		fprintf(fp, "\tkey_mgmt=WPA-PSK\n");
	} else {
		fprintf(fp, "\tkey_mgmt=NONE\n");
	}
	fprintf(fp, "}\n");
	fclose(fp);
	return 0;
}

static void log_wpa_status_full(void)
{
	char buf[512];

	if (read_cmd_output("wpa_cli -i " WIFI_IFACE " status 2>/dev/null",
			    buf, sizeof(buf)) == 0 && buf[0]) {
		trim_newline(buf);
		LOGI("ble_wifi: wpa_cli status:\n%s", buf);
	} else {
		LOGW("ble_wifi: wpa_cli status unavailable (control socket?)");
	}
}

/*
 * Poll wpa_state until COMPLETED or timeout.
 * Returns 0 on success, -1 on failure.
 */
static int wait_for_wpa_completed(void)
{
	char buf[256];
	int i;

	for (i = 0; i < WPA_COMPLETED_WAIT_SEC; i++) {
		sleep(1);
		if (read_cmd_output("wpa_cli -i " WIFI_IFACE
				    " status 2>/dev/null",
				    buf, sizeof(buf)) == 0) {
			if (strstr(buf, "wpa_state=COMPLETED")) {
				LOGI("ble_wifi: WPA COMPLETED after %ds", i + 1);
				return 0;
			}
		}
	}

	/*
	 * Final check: wpa_cli might have been unreachable (no control
	 * socket) for the entire wait. Log the final state and check once
	 * more — the supplicant could still be running and associated even
	 * without a working control interface.
	 */
	if (read_cmd_output("wpa_cli -i " WIFI_IFACE
			    " status 2>/dev/null | grep ^wpa_state=",
			    buf, sizeof(buf)) == 0 && buf[0]) {
		trim_newline(buf);
		LOGW("ble_wifi: final %s (timeout %ds)", buf,
		     WPA_COMPLETED_WAIT_SEC);
		if (strstr(buf, "COMPLETED"))
			return 0;
	} else {
		LOGW("ble_wifi: wpa_cli unreachable after %ds wait",
		     WPA_COMPLETED_WAIT_SEC);
	}

	return -1;
}

static bool iface_exists(const char *name)
{
	char path[64];

	snprintf(path, sizeof(path), "/sys/class/net/%s", name);
	return access(path, F_OK) == 0;
}

/*
 * The AIC driver registers p2p0 next to wlan0 at load time. A second
 * live VIF on the shared phy leaves the station associated but deaf to
 * unicast (group key still decrypts broadcasts, so DHCP works but ARP
 * doesn't). Only *down* p2p0 — never delete it, which corrupts the
 * AIC firmware's internal VIF table.
 */
static void release_idle_p2p_vif(void)
{
	if (!iface_exists("p2p0"))
		return;
	LOGI("ble_wifi: downing idle p2p0 before station bring-up");
	run_cmd("ifconfig p2p0 down >/dev/null 2>&1");
}

/*
 * Synchronously load AIC8800DC Wi-Fi kernel modules.
 * wifibt-init.sh runs everything in the background (`&`), so calling
 * it from C is unreliable. We bypass the script and insmod directly.
 *
 * Module search order (mirrors S36wifibt-init.sh):
 *   1. /lib/modules/           — SDK default (WIFIBT_MODULE_DIR)
 *   2. /lib/modules/<uname -r>/ — standard Buildroot layout
 */
static int load_wifi_modules(void)
{
	int rc;

	/*
	 * aic_load_fw must be loaded first — it handles firmware download.
	 * aic8800_fdrv is the actual Wi-Fi/combo driver that creates wlan0.
	 *
	 * The shell snippet:
	 *   - Determines MODULE_DIR by checking where the .ko files live
	 *   - Logs each insmod attempt instead of hiding errors
	 *   - Skips modules that are already loaded (lsmod check)
	 */
	rc = run_cmd(
		"MODULE_DIR=/lib/modules; "
		"if [ ! -f \"$MODULE_DIR/aic_load_fw.ko\" ]; then "
		"  KVER=$(uname -r); "
		"  for d in "
		"    \"/lib/modules/$KVER\" "
		"    \"/lib/modules/$KVER/extra\" "
		"    \"/lib/modules/$KVER/kernel/drivers/net/wireless\"; do "
		"    if [ -f \"$d/aic_load_fw.ko\" ]; then "
		"      MODULE_DIR=\"$d\"; break; "
		"    fi; "
		"  done; "
		"fi; "
		"echo \"ble_wifi: module dir=$MODULE_DIR\" >&2; "
		"RC=0; "
		"if ! lsmod | grep -wq aic_load_fw; then "
		"  if [ -f \"$MODULE_DIR/aic_load_fw.ko\" ]; then "
		"    echo \"ble_wifi: insmod aic_load_fw.ko\" >&2; "
		"    insmod \"$MODULE_DIR/aic_load_fw.ko\" || "
		"      { echo \"ble_wifi: insmod aic_load_fw FAILED\" >&2; RC=1; }; "
		"    sleep 1; "
		"  else "
		"    echo \"ble_wifi: aic_load_fw.ko not found in $MODULE_DIR\" >&2; "
		"    RC=1; "
		"  fi; "
		"fi; "
		"if ! lsmod | grep -wq aic8800_fdrv; then "
		"  if [ -f \"$MODULE_DIR/aic8800_fdrv.ko\" ]; then "
		"    echo \"ble_wifi: insmod aic8800_fdrv.ko\" >&2; "
		"    insmod \"$MODULE_DIR/aic8800_fdrv.ko\" || "
		"      { echo \"ble_wifi: insmod aic8800_fdrv FAILED\" >&2; RC=1; }; "
		"  else "
		"    echo \"ble_wifi: aic8800_fdrv.ko not found in $MODULE_DIR\" >&2; "
		"    RC=1; "
		"  fi; "
		"fi; "
		"exit $RC");

	if (rc != 0) {
		LOGE("ble_wifi: load_wifi_modules failed rc=%d", rc);
		return -1;
	}

	LOGI("ble_wifi: Wi-Fi modules loaded successfully");
	return 0;
}

static int wait_for_iface(int timeout_ms)
{
	int elapsed = 0;

	while (elapsed < timeout_ms) {
		if (access("/sys/class/net/" WIFI_IFACE, F_OK) == 0)
			return elapsed;
		usleep(200 * 1000);
		elapsed += 200;
	}
	return -1;
}

/*
 * Wait until `ifconfig wlan0 up` succeeds.
 *
 * The AIC8800DC driver creates the wlan0 sysfs entry quickly (~200ms
 * after insmod) but the firmware handshake continues asynchronously.
 * `ifconfig up` (SIOCSIFFLAGS) fails until the driver is truly ready.
 * Retry with 500ms intervals up to timeout_ms.
 */
static int wait_for_iface_up(int timeout_ms)
{
	int elapsed = 0;

	while (elapsed < timeout_ms) {
		if (run_cmd("ifconfig " WIFI_IFACE " up 2>/dev/null") == 0)
			return elapsed;
		usleep(500 * 1000);
		elapsed += 500;
	}
	return -1;
}

int ble_wifi_ensure_ready(void)
{
	int ms;

	if (access("/sys/class/net/" WIFI_IFACE, F_OK) == 0) {
		LOGI("ble_wifi: " WIFI_IFACE " already present, waiting for UP");
		goto bring_up;
	}

	LOGI("ble_wifi: " WIFI_IFACE " not present");

	/*
	 * Phase 1: Quick wait (5s) — the boot init script S36wifibt-init.sh
	 * might already be loading the driver in the background.
	 */
	ms = wait_for_iface(5000);
	if (ms >= 0) {
		LOGI("ble_wifi: " WIFI_IFACE " appeared after %d ms (boot init)", ms);
		goto bring_up;
	}

	/*
	 * Phase 2: Synchronous module load — directly insmod the AIC
	 * kernel modules. This is reliable because it runs in the
	 * foreground, unlike wifibt-init.sh which backgrounds everything.
	 */
	LOGI("ble_wifi: loading Wi-Fi modules directly");
	load_wifi_modules();

	ms = wait_for_iface(15000);
	if (ms >= 0) {
		LOGI("ble_wifi: " WIFI_IFACE " appeared after insmod (%d ms)", ms);
		goto bring_up;
	}

	/*
	 * Phase 3: Last resort — try the full init script. At this point
	 * USB should be fully enumerated (20s+ after boot).
	 */
	LOGW("ble_wifi: insmod did not produce " WIFI_IFACE ", trying full init");
	run_cmd("/etc/init.d/S36wifibt-init.sh start >/tmp/wifibt_retry.log 2>&1");

	ms = wait_for_iface(10000);
	if (ms >= 0) {
		LOGI("ble_wifi: " WIFI_IFACE " appeared after full init (%d ms)", ms);
		goto bring_up;
	}

	LOGE("ble_wifi: " WIFI_IFACE " still missing after 30s total");
	return -1;

bring_up:
	/*
	 * The AIC8800DC driver creates wlan0 quickly (~200ms after
	 * insmod) but the firmware handshake continues in the background.
	 * `ifconfig up` (SIOCSIFFLAGS) will fail until the driver is
	 * truly ready.  Retry for up to 10s.
	 */
	ms = wait_for_iface_up(10000);
	if (ms < 0) {
		LOGE("ble_wifi: " WIFI_IFACE
		     " exists but ifconfig up failed after 10s");
		return -1;
	}
	if (ms > 0)
		LOGI("ble_wifi: " WIFI_IFACE " up after %d ms", ms);
	return 0;
}

/* Forward declarations — AP helpers now shared by STA connect path. */
static void stop_ap_dnsmasq(void);
static void stop_stock_dnsmasq(void);
static void stop_udhcpc(void);
static void ensure_usb_dnsmasq(void);

int ble_wifi_connect(const char *ssid, const char *password,
		     char *ip_out, size_t ip_out_size)
{
	char cmd[512];
	char ip[48] = {0};
	int rc, ms;

	if (!ssid || ssid[0] == '\0')
		return -1;

	if (ip_out && ip_out_size > 0)
		ip_out[0] = '\0';

	LOGI("ble_wifi: connecting to ssid=%s (password %s)",
	     ssid, (password && password[0]) ? "provided" : "NONE");

	/*
	 * Step 1: write wpa_supplicant.conf — same format as the board
	 * template processed by wifi-connect.sh.
	 */
	if (write_wpa_config(ssid, password) != 0) {
		LOGE("ble_wifi: failed to write wpa config");
		return -1;
	}

	/* Log the generated config for diagnostics. */
	log_file_tail("wpa config", WIFI_WPA_CONF, 20);

	/*
	 * Do NOT touch p2p0.  wifi-connect.sh (the SDK reference that
	 * works reliably on AIC8800DC) never touches p2p0, and any
	 * ifconfig/iw operation on it — even a simple down — can upset
	 * the combo chip's shared-radio firmware and leave wlan0's
	 * unicast data plane dead.  Leave p2p0 alone.
	 */

	/* Step 3: terminate any running wpa_supplicant cleanly. */
	run_cmd("wpa_cli -i " WIFI_IFACE " terminate >/dev/null 2>&1");

	/*
	 * Step 4: kill leftover daemons.
	 * Do NOT use blanket `killall dnsmasq` — that murders the USB
	 * RNDIS dnsmasq.  Instead, use PID-file-targeted kills for
	 * dnsmasq (same approach as ble_wifi_start_ap).
	 */
	run_cmd("killall hostapd udhcpc wpa_supplicant >/dev/null 2>&1");
	stop_udhcpc();
	stop_ap_dnsmasq();
	stop_stock_dnsmasq();

	/* Step 5: wait for processes to die. */
	sleep(1);

	/* Verify wpa_supplicant is actually gone. */
	if (process_running("wpa_supplicant")) {
		LOGW("ble_wifi: wpa_supplicant still alive, sending SIGKILL");
		run_cmd("killall -9 wpa_supplicant >/dev/null 2>&1");
		usleep(500 * 1000);
	}

	/* Safety net: restore USB dnsmasq if it was collateral damage. */
	ensure_usb_dnsmasq();

	/*
	 * Step 6-9: cleanup_wifi_state() — mirrors wifi-connect.sh exactly.
	 * Bounce the interface through down→flush→type-set→up so the
	 * supplicant starts with a clean slate.
	 */
	run_cmd("ifconfig " WIFI_IFACE " down >/dev/null 2>&1");
	run_cmd("ip addr flush dev " WIFI_IFACE " >/dev/null 2>&1");
	run_cmd("ip route flush dev " WIFI_IFACE " >/dev/null 2>&1");

	run_cmd("iw dev " WIFI_IFACE " set type managed >/dev/null 2>&1");
	run_cmd("rm -f " WIFI_WPA_CTRL_DIR "/" WIFI_IFACE
		" /var/run/udhcpc*.pid >/dev/null 2>&1");

	/* Step 10: ensure ctrl_interface directory exists. */
	run_cmd("mkdir -p " WIFI_WPA_CTRL_DIR);

	/* Step 11: bring interface up (retry — driver may need time). */
	ms = wait_for_iface_up(10000);
	if (ms < 0) {
		LOGE("ble_wifi: ifconfig " WIFI_IFACE " up failed after 10s");
		return -1;
	}
	if (ms > 0)
		LOGI("ble_wifi: " WIFI_IFACE " up after %d ms (connect)", ms);

	/*
	 * Step 12: start wpa_supplicant.
	 *
	 * Do NOT redirect stderr — capture it to a log file so failures
	 * are diagnosable.  No -D flag, matching wifi-connect.sh exactly.
	 */
	snprintf(cmd, sizeof(cmd),
		 "wpa_supplicant -B -i " WIFI_IFACE
		 " -c " WIFI_WPA_CONF " > " WIFI_WPA_LOG " 2>&1");
	rc = run_cmd(cmd);
	if (rc != 0) {
		LOGE("ble_wifi: wpa_supplicant start failed rc=%d", rc);
		log_file_tail("wpa_supplicant log", WIFI_WPA_LOG, 10);
		return -1;
	}

	/* Verify wpa_supplicant is actually running after the fork. */
	usleep(200 * 1000);
	if (!process_running("wpa_supplicant")) {
		LOGE("ble_wifi: wpa_supplicant not running after start");
		log_file_tail("wpa_supplicant log", WIFI_WPA_LOG, 10);
		return -1;
	}
	LOGI("ble_wifi: wpa_supplicant started successfully");

	/* Step 13: wait for wpa_state=COMPLETED. */
	rc = wait_for_wpa_completed();
	if (rc != 0) {
		LOGE("ble_wifi: association failed (wpa_state not COMPLETED)");
		log_wpa_status_full();
		log_file_tail("wpa_supplicant log", WIFI_WPA_LOG, 20);
		return -1;
	}

	/* Disable power save — the AIC radio goes deaf with it on. */
	run_cmd("iwconfig " WIFI_IFACE " power off >/dev/null 2>&1");
	run_cmd("iw dev " WIFI_IFACE " set power_save off >/dev/null 2>&1");

	/* Step 14: DHCP. */
	LOGI("ble_wifi: requesting DHCP on " WIFI_IFACE);
	snprintf(cmd, sizeof(cmd),
		 "udhcpc -i " WIFI_IFACE " -t %d -T 1 -n -q 2>/dev/null",
		 UDHCPC_TIMEOUT_SEC);
	rc = run_cmd(cmd);
	if (rc != 0) {
		LOGW("ble_wifi: udhcpc failed rc=%d, retrying", rc);
		rc = run_cmd(cmd);
	}
	if (rc != 0) {
		LOGE("ble_wifi: DHCP failed rc=%d", rc);
		return -1;
	}

	snprintf(cmd, sizeof(cmd),
		 "ip -4 addr show " WIFI_IFACE
		 " | awk '/inet /{split($2,a,\"/\"); print a[1]}'");
	if (read_cmd_output(cmd, ip, sizeof(ip)) == 0 && ip[0]) {
		trim_newline(ip);
		LOGI("ble_wifi: connected ip=%s", ip);
		if (ip_out && ip_out_size > 0)
			snprintf(ip_out, ip_out_size, "%s", ip);

		/*
		 * Start udhcpc as a background daemon for lease renewal.
		 * The initial -q call above confirmed we have a lease;
		 * this daemon keeps it alive and handles server-side
		 * expiry or renewal.  -b backgrounds after obtaining
		 * the (quick) first lease.  No -R: avoid releasing
		 * the lease when the daemon is killed later.
		 */
		stop_udhcpc();
		run_cmd("udhcpc -i " WIFI_IFACE " -b"
			" -p " UDHCPC_PID " >/dev/null 2>&1 &");
		return 0;
	}

	LOGE("ble_wifi: connected but no IP assigned");
	return -1;
}

/* ── AP mode ───────────────────────────────────────────────────────── */

#define AP_IP          "192.168.4.1"
#define AP_NETMASK     "255.255.255.0"
#define HOSTAPD_CONF   "/tmp/parkia_cube_hostapd.conf"
#define DNSMASQ_CONF   "/tmp/parkia_cube_dnsmasq.conf"
#define DNSMASQ_PID    "/var/run/parkia_cube_dnsmasq.pid"
#define HOSTAPD_CTRL   "/var/run/hostapd"
#define STOCK_DNSMASQ_PID "/var/run/dnsmasq.pid"
#define USB_DNSMASQ_CONF  "/tmp/usb_dnsmasq.conf"
#define USB_DNSMASQ_PID   "/var/run/usb_dnsmasq.pid"

static int write_hostapd_config(const char *ssid, const char *password,
				int channel)
{
	FILE *fp;

	fp = fopen(HOSTAPD_CONF, "w");
	if (!fp) {
		LOGE("ble_wifi: failed to open %s for writing", HOSTAPD_CONF);
		return -1;
	}

	fprintf(fp, "interface=%s\n", WIFI_IFACE);
	fprintf(fp, "driver=nl80211\n");
	fprintf(fp, "ssid=%s\n", ssid);
	fprintf(fp, "hw_mode=g\n");
	fprintf(fp, "channel=%d\n", (channel >= 1 && channel <= 13) ? channel : 6);
	fprintf(fp, "ieee80211n=1\n");
	fprintf(fp, "ctrl_interface=%s\n", HOSTAPD_CTRL);
	fprintf(fp, "auth_algs=1\n");
	fprintf(fp, "ignore_broadcast_ssid=0\n");

	if (password && password[0]) {
		size_t pwd_len = strlen(password);

		if (pwd_len >= 8 && pwd_len <= 63) {
			fprintf(fp, "wpa=2\n");
			fprintf(fp, "wpa_passphrase=%s\n", password);
			fprintf(fp, "wpa_key_mgmt=WPA-PSK\n");
			fprintf(fp, "rsn_pairwise=CCMP\n");
		} else {
			LOGW("ble_wifi: AP passphrase length %zu invalid; "
			     "starting open hotspot", pwd_len);
		}
	}

	fclose(fp);
	return 0;
}

static int write_dnsmasq_config(void)
{
	FILE *fp;

	fp = fopen(DNSMASQ_CONF, "w");
	if (!fp) {
		LOGE("ble_wifi: failed to open %s for writing", DNSMASQ_CONF);
		return -1;
	}

	fprintf(fp, "interface=%s\n", WIFI_IFACE);
	fprintf(fp, "bind-interfaces\n");
	fprintf(fp, "dhcp-authoritative\n");
	fprintf(fp, "dhcp-range=192.168.4.20,192.168.4.200,%s,12h\n",
		AP_NETMASK);
	fprintf(fp, "dhcp-option=3,%s\n", AP_IP);   /* gateway */
	fprintf(fp, "dhcp-option=6,%s\n", AP_IP);   /* DNS */
	fprintf(fp, "address=/#/%s\n", AP_IP);       /* captive portal */
	fprintf(fp, "pid-file=%s\n", DNSMASQ_PID);

	fclose(fp);
	return 0;
}

/*
 * Stop only our AP dnsmasq (by PID file), leave USB dnsmasq alone.
 */
static void stop_ap_dnsmasq(void)
{
	run_cmd("if [ -f " DNSMASQ_PID " ]; then "
		"kill \"$(cat " DNSMASQ_PID ")\" 2>/dev/null; "
		"rm -f " DNSMASQ_PID "; fi");
}

/*
 * The stock S80dnsmasq holds UDP/67 globally — stop it so our
 * per-interface server can bind.
 */
static void stop_stock_dnsmasq(void)
{
	run_cmd("if [ -f " STOCK_DNSMASQ_PID " ]; then "
		"kill \"$(cat " STOCK_DNSMASQ_PID ")\" 2>/dev/null; "
		"rm -f " STOCK_DNSMASQ_PID "; fi");
}

/*
 * Stop udhcpc via its PID file (same pattern as stop_ap_dnsmasq).
 */
static void stop_udhcpc(void)
{
	run_cmd("if [ -f " UDHCPC_PID " ]; then "
		"kill \"$(cat " UDHCPC_PID ")\" 2>/dev/null; "
		"rm -f " UDHCPC_PID "; fi");
}

/*
 * Ensure a udhcpc daemon is running for DHCP lease renewal.
 * Used after boot-time reconnect where wlan0 already has an IP
 * from a previous session but no daemon is doing lease renewal.
 */
void ble_wifi_ensure_dhcp_daemon(void)
{
	char buf[32] = {0};

	/* Already running? */
	if (read_cmd_output("cat " UDHCPC_PID " 2>/dev/null",
			    buf, sizeof(buf)) == 0 && buf[0] != '\0') {
		trim_newline(buf);
		char proc[64];

		snprintf(proc, sizeof(proc), "/proc/%s", buf);
		if (access(proc, F_OK) == 0) {
			LOGD("ble_wifi: udhcpc daemon already running "
			     "(pid=%s)", buf);
			return;
		}
	}

	LOGI("ble_wifi: starting udhcpc daemon for lease renewal");
	run_cmd("udhcpc -i " WIFI_IFACE " -b"
		" -p " UDHCPC_PID " >/dev/null 2>&1 &");
}

/*
 * Restart the USB RNDIS dnsmasq that killall may have wiped out.
 */
static void ensure_usb_dnsmasq(void)
{
	stop_stock_dnsmasq();
	run_cmd("if [ -f " USB_DNSMASQ_CONF " ] && "
		"{ [ ! -f " USB_DNSMASQ_PID " ] || "
		"! kill -0 \"$(cat " USB_DNSMASQ_PID ")\" 2>/dev/null; }; then "
		"dnsmasq -C " USB_DNSMASQ_CONF " >/dev/null 2>&1 || true; fi");
}

int ble_wifi_start_ap(const char *ssid, const char *password, int channel)
{
	char cmd[256];
	int rc, ms;

	if (!ssid || ssid[0] == '\0')
		return -1;

	LOGI("ble_wifi: starting AP mode ssid=%s channel=%d", ssid, channel);

	/* Step 0: make sure wlan0 exists (driver may still be loading). */
	if (ble_wifi_ensure_ready() != 0) {
		LOGE("ble_wifi: " WIFI_IFACE " not available for AP");
		return -1;
	}

	/* Step 1: kill existing network daemons — targeted, spare USB dnsmasq. */
	run_cmd("wpa_cli -i " WIFI_IFACE " terminate >/dev/null 2>&1");
	run_cmd("killall wpa_supplicant hostapd udhcpc >/dev/null 2>&1");
	stop_udhcpc();
	stop_ap_dnsmasq();
	stop_stock_dnsmasq();

	/* Step 2: wait for processes to die. */
	sleep(1);

	if (process_running("hostapd")) {
		LOGW("ble_wifi: hostapd still alive, sending SIGKILL");
		run_cmd("killall -9 hostapd >/dev/null 2>&1");
		usleep(500 * 1000);
	}

	/*
	 * Step 3: clean interface state and switch to AP mode.
	 *
	 * CRITICAL: wlan0 must be set to type __ap WHILE DOWN, before
	 * bringing it up.  If wlan0 is still in managed mode when
	 * hostapd starts, the nl80211 driver creates a NEW virtual
	 * interface (wlan1) for the AP, which confuses the AIC8800DC
	 * combo chip and causes hostapd to fail.
	 */
	run_cmd("ifconfig " WIFI_IFACE " down >/dev/null 2>&1");
	run_cmd("ip addr flush dev " WIFI_IFACE " >/dev/null 2>&1");
	run_cmd("ip route flush dev " WIFI_IFACE " >/dev/null 2>&1");

	/* Switch wlan0 to AP type while it is DOWN — prevents nl80211
	 * from creating a separate wlan1 interface for the AP. */
	run_cmd("iw dev " WIFI_IFACE " set type __ap 2>/dev/null");

	/* Step 4: bring interface up with AP IP (retry — driver may need time). */
	ms = wait_for_iface_up(10000);
	if (ms < 0) {
		LOGE("ble_wifi: ifconfig " WIFI_IFACE " up failed after 10s (AP)");
		return -1;
	}
	if (ms > 0)
		LOGI("ble_wifi: " WIFI_IFACE " up after %d ms (AP)", ms);

	snprintf(cmd, sizeof(cmd),
		 "ifconfig " WIFI_IFACE " %s netmask %s", AP_IP, AP_NETMASK);
	rc = run_cmd(cmd);
	if (rc != 0) {
		LOGE("ble_wifi: ifconfig AP IP failed rc=%d", rc);
		return -1;
	}

	/* Step 5: write hostapd config. */
	if (write_hostapd_config(ssid, password, channel) != 0)
		return -1;

	/* Step 6: write dnsmasq config. */
	if (write_dnsmasq_config() != 0)
		return -1;

	/* Step 7: start dnsmasq. */
	snprintf(cmd, sizeof(cmd), "dnsmasq -C %s", DNSMASQ_CONF);
	rc = run_cmd(cmd);
	if (rc != 0) {
		LOGE("ble_wifi: dnsmasq start failed rc=%d", rc);
		ensure_usb_dnsmasq();
		return -1;
	}

	/* Step 8: restore USB dnsmasq in case killall touched it. */
	ensure_usb_dnsmasq();

	/* Step 9: start hostapd. */
	run_cmd("mkdir -p " HOSTAPD_CTRL);
	snprintf(cmd, sizeof(cmd), "hostapd -B %s", HOSTAPD_CONF);
	rc = run_cmd(cmd);
	if (rc != 0) {
		LOGE("ble_wifi: hostapd start failed rc=%d", rc);
		log_file_tail("hostapd config", HOSTAPD_CONF, 20);
		stop_ap_dnsmasq();
		return -1;
	}

	/* Verify hostapd is actually running. */
	usleep(500 * 1000);
	if (!process_running("hostapd")) {
		LOGE("ble_wifi: hostapd not running after start");
		log_file_tail("hostapd config", HOSTAPD_CONF, 20);
		stop_ap_dnsmasq();
		return -1;
	}

	LOGI("ble_wifi: AP mode started ssid=%s ip=%s", ssid, AP_IP);
	return 0;
}

bool ble_wifi_is_connected(char *ip_out, size_t ip_out_size)
{
	char ip[48] = {0};
	char gw[48] = {0};
	char cmd[256];

	/* Step 1: check wlan0 has an IP address. */
	snprintf(cmd, sizeof(cmd),
		 "ip -4 addr show " WIFI_IFACE
		 " | awk '/inet /{split($2,a,\"/\"); print a[1]}'");
	if (read_cmd_output(cmd, ip, sizeof(ip)) != 0 || ip[0] == '\0')
		return false;

	trim_newline(ip);
	if (ip_out && ip_out_size > 0)
		snprintf(ip_out, ip_out_size, "%s", ip);

	/*
	 * Step 2: check wpa_supplicant association state.
	 * If wlan0 is not associated, no point pinging.
	 */
	if (read_cmd_output("wpa_cli -i " WIFI_IFACE
			    " status 2>/dev/null"
			    " | awk -F= '/^wpa_state=/{print $2}'",
			    gw, sizeof(gw)) == 0) {
		trim_newline(gw);
		if (gw[0] && strcmp(gw, "COMPLETED") != 0) {
			LOGD("ble_wifi: wpa_state=%s, not associated", gw);
			return false;
		}
	}

	/*
	 * Step 3: verify data-plane reachability by pinging the default
	 * gateway.  Use 5 packets with 3s timeout — success if ANY one
	 * gets a reply.  The AIC8800DC has very high RX-dropped counts
	 * when the device is streaming video (TX-heavy), so individual
	 * ICMP replies get dropped in the kernel receive path.  5 packets
	 * with 3s timeout tolerates this without false-positive offline
	 * detection.
	 */
	if (read_cmd_output("ip route show default dev " WIFI_IFACE
			    " | awk '/via/{print $3}'",
			    gw, sizeof(gw)) != 0 || gw[0] == '\0')
		return true; /* no gateway info — assume OK */

	trim_newline(gw);
	snprintf(cmd, sizeof(cmd),
		 "ping -I " WIFI_IFACE " -c 5 -W 3 %s >/dev/null 2>&1", gw);
	if (run_cmd(cmd) != 0)
		return false;

	return true;
}

/* ── WiFi auto-recovery ────────────────────────────────────────────
 *
 * Lightweight recovery for the AIC8800DC data-plane stall caused by
 * thermal-camera USB hot-plug events.  The chip stays associated
 * (iw link shows connected) but the unicast data plane dies.
 */
bool ble_wifi_try_recover(void)
{
	char buf[256];
	char cmd[256];
	int rc;

	LOGI("ble_wifi: attempting Wi-Fi recovery (disconnect + reconnect + DHCP)");

	/*
	 * Step 1: disconnect + reconnect.
	 * A simple reassociate is not enough when the AIC8800DC data
	 * plane is stuck (wpa_state=COMPLETED but 100% packet loss).
	 * A full disconnect/reconnect cycle resets the driver state.
	 */
	run_cmd("wpa_cli -i " WIFI_IFACE " disconnect 2>/dev/null");
	usleep(500 * 1000);
	rc = run_cmd("wpa_cli -i " WIFI_IFACE " reconnect 2>/dev/null");
	if (rc != 0) {
		LOGW("ble_wifi: wpa_cli reconnect failed rc=%d", rc);
		return false;
	}

	/* Step 2: wait for reconnection to complete (~5s typical). */
	for (int i = 0; i < 8; i++) {
		sleep(1);
		if (read_cmd_output("wpa_cli -i " WIFI_IFACE
				    " status 2>/dev/null",
				    buf, sizeof(buf)) == 0 &&
		    strstr(buf, "wpa_state=COMPLETED")) {
			LOGI("ble_wifi: reconnection completed after %ds",
			     i + 1);
			goto dhcp;
		}
	}

	LOGW("ble_wifi: wpa_state not COMPLETED after reconnect (8s)");
	return false;

dhcp:
	/*
	 * Step 3: renew DHCP lease.
	 * Read the current IP BEFORE killing udhcpc so we can request
	 * the same address with -r.  Do NOT use -R (release-on-exit)
	 * because that tells the DHCP server the lease is abandoned,
	 * causing it to hand out a different IP next time.
	 */
	{
		char old_ip[48] = {0};

		/* Grab current IP while udhcpc is still alive. */
		read_cmd_output("ip -4 addr show " WIFI_IFACE
				" | awk '/inet /{split($2,a,\"/\"); print a[1]}'",
				old_ip, sizeof(old_ip));
		trim_newline(old_ip);
		LOGI("ble_wifi: old_ip before stop_udhcpc = '%s'", old_ip);

		stop_udhcpc();

		/*
		 * Try up to DHCP_OLD_IP_RETRIES times with -r <old_ip>,
		 * then fall back to a plain request (accept any IP).
		 */
#define DHCP_OLD_IP_RETRIES 3

		rc = -1;
		if (old_ip[0] != '\0') {
			for (int i = 1; i <= DHCP_OLD_IP_RETRIES; i++) {
				LOGI("ble_wifi: requesting previous IP %s "
				     "(%d/%d)", old_ip, i,
				     DHCP_OLD_IP_RETRIES);
				snprintf(cmd, sizeof(cmd),
					 "udhcpc -i " WIFI_IFACE
					 " -r %s -t 3 -T 1 -n -q"
					 " 2>/dev/null", old_ip);
				rc = run_cmd(cmd);
				if (rc == 0)
					break;
			}
			if (rc != 0)
				LOGW("ble_wifi: old IP %s rejected %d times,"
				     " falling back to any IP",
				     old_ip, DHCP_OLD_IP_RETRIES);
		}
		if (rc != 0) {
			snprintf(cmd, sizeof(cmd),
				 "udhcpc -i " WIFI_IFACE
				 " -t 5 -T 1 -n -q 2>/dev/null");
			rc = run_cmd(cmd);
		}
		if (rc != 0) {
			LOGW("ble_wifi: udhcpc failed during recovery rc=%d",
			     rc);
			return false;
		}

		/* Restart udhcpc daemon for ongoing lease renewal.
		 * No -R: avoid releasing the lease when the daemon
		 * is killed during the next recovery cycle.         */
		run_cmd("udhcpc -i " WIFI_IFACE " -b"
			" -p " UDHCPC_PID " >/dev/null 2>&1 &");
	}

	/* Step 4: verify we actually have an IP. */
	if (ble_wifi_is_connected(NULL, 0)) {
		LOGI("ble_wifi: recovery successful");
		return true;
	}

	LOGW("ble_wifi: recovery failed — no IP after DHCP");
	return false;
}

