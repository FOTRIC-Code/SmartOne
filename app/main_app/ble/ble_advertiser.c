#include "ble/ble_advertiser.h"

#include "ble_compat.h"
#include "libs/log/log.h"
#include "libs/time/time_util.h"

ble_compat_config_t g_ble_compat_config;

#include <RkBle.h>
#include <RkBtBase.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BLE_ADV_INTERVAL_MIN 160 /* 100ms (160 * 0.625ms) */
#define BLE_ADV_INTERVAL_MAX 320 /* 200ms (320 * 0.625ms) */
#define BLE_USE_CUSTOM_ADV_INTERVAL 0
#define BLE_INIT_TIMEOUT_SEC 5
#define BLE_POWER_ON_TIMEOUT_SEC 5
#define BLE_ADV_START_TIMEOUT_SEC 5
#define BLE_ADV_CONFIRM_GRACE_SEC 20
#define BLE_ADV_RUNTIME_LOSS_GRACE_SEC 15
#define BLE_ADV_SERVICE_UUID16 "1234"
#define BLE_ADVERTISE_SERVICE_UUID BLE_SERVICE_UUID
#define BLE_BT_CONF_DIR "/userdata"
#define BLE_BT_CONF_PATH BLE_BT_CONF_DIR "/main.conf"
#define BLE_BLUETOOTHD_INIT_SCRIPT "/etc/init.d/S40bluetoothd"
#define BLE_AIC_BT_BIND_INIT_SCRIPT "/etc/init.d/S39aic-btusb-bind"
#define BLE_HCI0_WAIT_SEC 12
/* Let S36wifibt-init.sh finish binding the AIC combo before forcing a rebind. */
#define BLE_HCI0_GRACE_WAIT_SEC 8
#define BLE_START_RETRY_SEC 2
#define BLE_MANUAL_ADV_LOG_PATH "/tmp/main_app_ble_advertiser.log"
#define BLE_MANUAL_ADV_HELPER_PATH "/usr/libexec/main_app/main_app_ble_adv"
#define BLE_MANUAL_ADV_HELPER_FALLBACK_PATH "/tmp/main_app/main_app_ble_adv"
#define BLE_MANUAL_ADV_HELPER_ENV "BLE_APP_ENABLE_BLUEZ_ADV_HELPER"
#define BLE_BLUEZ_ADV_PATH_RK "/org/bluez/advertising"
#define BLE_BLUEZ_ADV_PATH_HELPER "/com/parkia/netapp/advertisement0"
#define BLE_BLUEZ_ADV_PATH_HCI0 "/org/bluez/hci0/advertisement0"
#define BLE_BLUEZ_CMD_TIMEOUT_SEC 2
#define BLE_ADV_STOP_CONFIRM_RETRY 10
#define BLE_REQ_ID_NONE (-1)
#define BLE_CODE_BUSY (-409)
#define BLE_CODE_TOO_LARGE (-413)
#define BLE_PROVISION_STA_ERROR_CODE 300101
#define BLE_PROVISION_AP_ERROR_CODE 300102
#define BLE_PROVISION_NOT_ALLOWED_CODE 300103

/* g_wifi removed: main_app uses ble_wifi.h for Wi-Fi ops. */

static RkBtContent s_bt_content;
static volatile bool s_bt_inited;
static volatile bool s_ble_advertised;
static volatile bool s_ble_running;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool s_bt_debug_inited;
static bool s_bt_content_sync_inited;
static time_t s_ble_last_probe_time;
static time_t s_ble_confirm_deadline;
static time_t s_ble_runtime_loss_deadline;
static time_t s_manual_adv_retry_time;
static pid_t s_manual_adv_pid = -1;
static bool s_notify_enabled;
static char s_last_event[BT_ATT_MAX_VALUE_LEN + 1];
static int s_last_event_len;
static char s_session_addr[18];
static char s_session_addr_type[8];
static struct {
	volatile bool pending;
	bool too_large;
	int len;
	char payload[BT_ATT_MAX_VALUE_LEN + 1];
} s_pending_command;
static struct {
	bool active;
	int req_id;
	char op[32];
	provision_run_state_t last_state;
	char last_stage[32];
	char last_ip[48];
	int last_error_code;
	char last_error_reason[64];
} s_async_request;
static time_t s_async_probe_time;
static net_ble_advertisement_t s_retry_adv;
static bool s_start_retry_pending;
static time_t s_start_retry_time;
static bool s_bt_init_done; /* true after first successful rk_bt_init() */
static int s_power_on_fail_count;

/*
 * Rockchip's BlueZ bridge registers server-side characteristics from the
 * chr_props string list. Keeping the declaration close to the vendor samples
 * avoids duplicate/invalid notify metadata on newer BlueZ builds.
 */
static char *s_chr_props_write[] = {"read", "write",
				    "write-without-response", NULL};
static char *s_chr_props_notify[] = {"notify", NULL};

static const char *ble_get_runtime_name(const net_ble_advertisement_t *adv)
{
	static char name[32];
	const char *serial = adv ? adv->serial_no : NULL;
	size_t len;
	const char *suffix;

	if (!serial || !serial[0])
		return NET_DEFAULT_DEVICE_NAME;

	len = strlen(serial);
	suffix = serial + (len > 4 ? len - 4 : 0);
	snprintf(name, sizeof(name), "%s-%s", NET_DEFAULT_DEVICE_NAME, suffix);
	return name;
}

static bool ble_manual_bluez_advertiser_enabled(void)
{
	static int s_manual_adv_enabled = -1;

	if (s_manual_adv_enabled < 0) {
		const char *value = getenv(BLE_MANUAL_ADV_HELPER_ENV);

		s_manual_adv_enabled = (value && strcmp(value, "1") == 0) ? 1 : 0;
		if (s_manual_adv_enabled == 0) {
			LOGI("BLE: skipping direct BlueZ advertiser helper by default; set %s=1 to force it",
			     BLE_MANUAL_ADV_HELPER_ENV);
		}
	}

	return s_manual_adv_enabled > 0;
}

static void ble_trim_newline(char *line)
{
	size_t len;

	if (!line)
		return;

	len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
		line[--len] = '\0';
}

static void ble_log_bluez_state_snapshot(const char *reason)
{
	FILE *fp;
	char line[256];
	bool has_adv_object = false;

	fp = popen("bluetoothctl show 2>/dev/null", "r");
	if (fp) {
		while (fgets(line, sizeof(line), fp)) {
			if (!strstr(line, "Alias:") &&
			    !strstr(line, "ActiveInstances:") &&
			    !strstr(line, "SupportedInstances:") &&
			    !strstr(line, "UUID:"))
				continue;

			ble_trim_newline(line);
			LOGI("BLE: BlueZ snapshot (%s): %s", reason, line);
		}

		pclose(fp);
	}

	fp = popen("busctl tree org.bluez /org/bluez 2>/dev/null", "r");
	if (!fp)
		return;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "advertisement") || strstr(line, "Advertisement")) {
			has_adv_object = true;
			break;
		}
	}

	pclose(fp);
	LOGI("BLE: BlueZ snapshot (%s): advertisement object %s", reason,
	     has_adv_object ? "present" : "absent");
}

static const char *ble_get_manual_adv_helper_path(void)
{
	if (access(BLE_MANUAL_ADV_HELPER_PATH, X_OK) == 0)
		return BLE_MANUAL_ADV_HELPER_PATH;

	if (access(BLE_MANUAL_ADV_HELPER_FALLBACK_PATH, X_OK) == 0)
		return BLE_MANUAL_ADV_HELPER_FALLBACK_PATH;

	return NULL;
}

static void ble_wait_manual_adv_child(pid_t pid, int signal_no)
{
	int retry = 20;
	int status;
	pid_t waited;

	if (pid <= 0)
		return;

	while (retry-- > 0) {
		waited = waitpid(pid, &status, WNOHANG);
		if (waited == pid || (waited < 0 && errno == ECHILD))
			return;
		usleep(100 * 1000);
	}

	if (signal_no > 0)
		kill(pid, signal_no);

	retry = 20;
	while (retry-- > 0) {
		waited = waitpid(pid, &status, WNOHANG);
		if (waited == pid || (waited < 0 && errno == ECHILD))
			return;
		usleep(100 * 1000);
	}

	if (signal_no == SIGTERM)
		kill(pid, SIGKILL);

	(void)waitpid(pid, &status, 0);
}

static void ble_stop_manual_bluez_advertiser(void)
{
	pid_t pid = s_manual_adv_pid;

	s_manual_adv_pid = -1;
	s_manual_adv_retry_time = 0;
	ble_wait_manual_adv_child(pid, SIGTERM);
}

static bool ble_manual_adv_process_alive(void)
{
	int status;
	pid_t waited;

	if (s_manual_adv_pid <= 0)
		return false;

	waited = waitpid(s_manual_adv_pid, &status, WNOHANG);
	if (waited == 0)
		return true;

	if (waited == s_manual_adv_pid) {
		if (WIFEXITED(status)) {
			LOGW("BLE: direct BlueZ advertiser exited rc=%d",
			     WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			LOGW("BLE: direct BlueZ advertiser killed by signal %d",
			     WTERMSIG(status));
		}
	} else if (waited < 0 && errno != ECHILD) {
		LOGW("BLE: waitpid on direct BlueZ advertiser failed: %s",
		     strerror(errno));
	}

	s_manual_adv_pid = -1;
	return false;
}

static int ble_start_manual_bluez_advertiser(const char *ble_name)
{
	const char *helper_path;
	int log_fd;
	pid_t pid;

	ble_stop_manual_bluez_advertiser();
	helper_path = ble_get_manual_adv_helper_path();
	if (!helper_path) {
		LOGE("BLE: direct BlueZ advertiser helper not found");
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		LOGE("BLE: failed to fork direct BlueZ advertiser: %s",
		     strerror(errno));
		return -1;
	}

	if (pid == 0) {
		log_fd = open(BLE_MANUAL_ADV_LOG_PATH,
			     O_CREAT | O_TRUNC | O_WRONLY,
			     0644);
		if (log_fd < 0)
			log_fd = open("/dev/null", O_WRONLY);

		if (log_fd >= 0) {
			dup2(log_fd, STDOUT_FILENO);
			dup2(log_fd, STDERR_FILENO);
		}

		if (log_fd > STDERR_FILENO)
			close(log_fd);

		execl(helper_path, helper_path,
		      ble_name ? ble_name : NET_DEFAULT_DEVICE_NAME, NULL);
		_exit(127);
	}

	s_manual_adv_pid = pid;
	s_manual_adv_retry_time = time_now_sec() + 1;
	LOGI("BLE: started direct BlueZ advertiser helper pid=%d path=%s",
	     (int)s_manual_adv_pid, helper_path);
	return 0;
}

static int ble_retry_manual_bluez_advertiser(void)
{
	const char *ble_name = s_bt_content.bt_name[0] ?
			       s_bt_content.bt_name : NET_DEFAULT_DEVICE_NAME;

	if (ble_manual_adv_process_alive()) {
		LOGI("BLE: direct BlueZ advertiser still awaiting confirmation");
		return 0;
	}

	LOGW("BLE: direct BlueZ advertiser not running, restarting helper");
	return ble_start_manual_bluez_advertiser(ble_name);
}

static int ble_ensure_dir_exists(const char *path)
{
	struct stat st;

	if (stat(path, &st) == 0) {
		if (S_ISDIR(st.st_mode))
			return 0;

		errno = ENOTDIR;
		return -1;
	}

	if (mkdir(path, 0700) == 0 || errno == EEXIST)
		return 0;

	return -1;
}

static void ble_prepare_bt_content_sync_primitives(void)
{
	if (s_bt_content_sync_inited) {
		pthread_mutex_destroy(&s_bt_content.bt_mutex);
		pthread_cond_destroy(&s_bt_content.cond);
		s_bt_content_sync_inited = false;
	}

	pthread_mutex_init(&s_bt_content.bt_mutex, NULL);
	pthread_cond_init(&s_bt_content.cond, NULL);
	s_bt_content_sync_inited = true;
}

static void ble_write_bt_main_conf(const char *ble_name)
{
	FILE *fp;
	int err;

	if (ble_ensure_dir_exists(BLE_BT_CONF_DIR) != 0) {
		err = errno;
		LOGW("BLE: failed to create %s: %s", BLE_BT_CONF_DIR,
		     strerror(err));
		return;
	}

	fp = fopen(BLE_BT_CONF_PATH, "w");
	if (!fp) {
		err = errno;
		LOGW("BLE: failed to open %s: %s", BLE_BT_CONF_PATH,
		     strerror(err));
		return;
	}

	fprintf(fp,
		"[General]\n"
		"BleName = %s\n"
		"ControllerMode = le\n"
		"JustWorksRepairing = always\n"
		"DiscoverableTimeout = 0\n"
		"Privacy = off\n"
		"\n"
		"[GATT]\n"
		"Cache = no\n",
		ble_name ? ble_name : NET_DEFAULT_DEVICE_NAME);
	fclose(fp);

	LOGI("BLE: wrote Bluetooth config to %s", BLE_BT_CONF_PATH);
}

static bool ble_command_has_output(const char *command)
{
	FILE *fp;
	char line[64];
	bool has_output = false;

	fp = popen(command, "r");
	if (!fp)
		return false;

	if (fgets(line, sizeof(line), fp))
		has_output = true;

	pclose(fp);
	return has_output;
}

static bool ble_is_hci0_present(void)
{
	return access("/sys/class/bluetooth/hci0", F_OK) == 0;
}

static void ble_cache_retry_advertisement(const net_ble_advertisement_t *adv)
{
	if (!adv)
		return;

	s_retry_adv = *adv;
}

static void ble_clear_start_retry_locked(void)
{
	s_start_retry_pending = false;
	s_start_retry_time = 0;
}

static void ble_schedule_start_retry_locked(unsigned int delay_sec)
{
	if (delay_sec == 0)
		delay_sec = 1;

	s_start_retry_pending = true;
	s_start_retry_time = time_now_sec() + delay_sec;
}

static bool ble_wait_hci0_present(int timeout_sec)
{
	int retry = timeout_sec * 10;

	while (retry-- > 0) {
		if (ble_is_hci0_present())
			return true;
		usleep(100 * 1000);
	}

	return ble_is_hci0_present();
}

static void ble_request_aic_btusb_bind(void)
{
	int ret;

	if (ble_is_hci0_present())
		return;

	if (access(BLE_AIC_BT_BIND_INIT_SCRIPT, X_OK) != 0) {
		LOGW("BLE: hci0 absent and %s is unavailable",
		     BLE_AIC_BT_BIND_INIT_SCRIPT);
		return;
	}

	LOGW("BLE: hci0 absent, running %s restart",
	     BLE_AIC_BT_BIND_INIT_SCRIPT);
	ret = system(BLE_AIC_BT_BIND_INIT_SCRIPT " restart >/dev/null 2>&1");
	if (ret != 0) {
		LOGW("BLE: %s restart failed (rc=%d)",
		     BLE_AIC_BT_BIND_INIT_SCRIPT, ret);
	}
}

static bool ble_is_bluetoothd_running(void)
{
	return ble_command_has_output("pidof bluetoothd 2>/dev/null");
}

static bool ble_is_bluez_ready(void)
{
	FILE *fp;
	char line[256];
	bool ready = false;

	fp = popen("bluetoothctl show 2>/dev/null", "r");
	if (!fp)
		return false;

	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "Controller ") || strstr(line, "Alias:") ||
		    strstr(line, "Name:") || strstr(line, "Powered:")) {
			ready = true;
			break;
		}
	}

	pclose(fp);
	return ready;
}

static bool ble_ensure_bluez_ready(void)
{
	int ret;
	int retry = 50;

	if (ble_is_bluez_ready())
		return true;

	if (!ble_is_bluetoothd_running()) {
		if (access(BLE_BLUETOOTHD_INIT_SCRIPT, X_OK) == 0) {
			LOGW("BLE: bluetoothd not running, starting %s",
			     BLE_BLUETOOTHD_INIT_SCRIPT);
			ret = system(BLE_BLUETOOTHD_INIT_SCRIPT
				     " start >/dev/null 2>&1");
			if (ret != 0)
				LOGW("BLE: failed to start bluetoothd via %s (rc=%d)",
				     BLE_BLUETOOTHD_INIT_SCRIPT, ret);
		} else {
			LOGW("BLE: bluetoothd missing and %s not available",
			     BLE_BLUETOOTHD_INIT_SCRIPT);
		}
	}

	while (retry-- > 0) {
		if (ble_is_bluez_ready())
			return true;
		usleep(100 * 1000);
	}

	LOGE("BLE: BlueZ daemon not ready");
	return false;
}

static bool ble_prepare_controller_ready(void)
{
	if (!ble_is_hci0_present()) {
		/*
		 * S36wifibt-init.sh binds the AIC combo asynchronously during
		 * boot. Restarting the bind script on top of that races it and
		 * can leave hci0 gone for good, so give the boot script a
		 * grace period and only then force the rebind.
		 */
		LOGI("BLE: hci0 absent, waiting up to %ds for boot init",
		     BLE_HCI0_GRACE_WAIT_SEC);
		if (!ble_wait_hci0_present(BLE_HCI0_GRACE_WAIT_SEC)) {
			ble_request_aic_btusb_bind();
			(void)ble_wait_hci0_present(BLE_HCI0_WAIT_SEC);
		}
	}

	if (!ble_is_hci0_present()) {
		LOGW("BLE: hci0 still absent after %d seconds",
		     BLE_HCI0_GRACE_WAIT_SEC + BLE_HCI0_WAIT_SEC);
		return false;
	}

	if (!ble_ensure_bluez_ready()) {
		LOGW("BLE: bluetoothd still not ready after hci0 appeared");
		return false;
	}

	return true;
}

static int ble_query_powered_state(void)
{
	FILE *fp;
	char line[256];
	int powered = -1;

	fp = popen("bluetoothctl show 2>/dev/null", "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		char *p = strstr(line, "Powered:");

		if (!p)
			continue;

		p += strlen("Powered:");
		while (*p == ' ' || *p == '\t')
			p++;

		if (strncmp(p, "yes", 3) == 0)
			powered = 1;
		else if (strncmp(p, "no", 2) == 0)
			powered = 0;
		break;
	}

	pclose(fp);
	return powered;
}

static bool ble_ensure_adapter_powered(void)
{
	int retry = BLE_POWER_ON_TIMEOUT_SEC * 10;

	if (ble_query_powered_state() > 0)
		return true;

	LOGW("BLE: adapter not powered in BlueZ, requesting power on");
	rk_bt_set_power(true);

	while (retry-- > 0) {
		if (ble_query_powered_state() > 0)
			return true;
		usleep(100 * 1000);
	}

	LOGW("BLE: vendor power-on did not reach BlueZ, retrying via bluetoothctl");
	(void)system("bluetoothctl power on >/dev/null 2>&1");

	retry = BLE_POWER_ON_TIMEOUT_SEC * 10;
	while (retry-- > 0) {
		if (ble_query_powered_state() > 0)
			return true;
		usleep(100 * 1000);
	}

	ble_log_bluez_state_snapshot("power-on-timeout");
	LOGE("BLE: controller failed to reach Powered=yes");
	s_power_on_fail_count++;
	return false;
}

static int ble_query_active_instances(void)
{
	FILE *fp;
	char line[256];
	int active_instances = -1;

	fp = popen("bluetoothctl show 2>/dev/null", "r");
	if (!fp)
		return -1;

	while (fgets(line, sizeof(line), fp)) {
		char *p = strstr(line, "ActiveInstances:");
		unsigned int value;

		if (!p)
			continue;

		p += strlen("ActiveInstances:");
		while (*p == ' ' || *p == '\t')
			p++;

		if (sscanf(p, "0x%x", &value) == 1) {
			active_instances = (int)value;
			break;
		}

		if (sscanf(p, "%u", &value) == 1) {
			active_instances = (int)value;
			break;
		}
	}

	pclose(fp);
	return active_instances;
}

static int ble_read_cmd_output(const char *cmd, char *buf, size_t buf_len);

static int ble_query_active_instances_dbus(void)
{
	char out[256];
	char *p;
	unsigned int value;

	if (ble_read_cmd_output(
		    "dbus-send --system --print-reply --dest=org.bluez "
		    "--reply-timeout=2000 /org/bluez/hci0 "
		    "org.freedesktop.DBus.Properties.Get "
		    "string:org.bluez.LEAdvertisingManager1 "
		    "string:ActiveInstances 2>/dev/null",
		    out, sizeof(out)) == -1)
		return -1;

	p = strstr(out, "byte");
	if (!p)
		return -1;
	if (sscanf(p + 4, "%u", &value) == 1)
		return (int)value;
	return -1;
}

static bool ble_valid_dbus_path(const char *path)
{
	size_t i;

	if (!path || path[0] != '/')
		return false;

	for (i = 0; path[i]; i++) {
		unsigned char c = (unsigned char)path[i];

		if (!(isalnum(c) || c == '/' || c == '_'))
			return false;
		if (i >= 127)
			return false;
	}

	return i > 1;
}

static int ble_read_cmd_output(const char *cmd, char *buf, size_t buf_len)
{
	FILE *fp;
	size_t used = 0;

	if (!cmd || !buf || buf_len == 0)
		return -1;

	buf[0] = '\0';
	fp = popen(cmd, "r");
	if (!fp)
		return -1;

	while (used + 1 < buf_len) {
		size_t n = fread(buf + used, 1, buf_len - used - 1, fp);
		used += n;
		if (n == 0)
			break;
	}
	buf[used] = '\0';
	return pclose(fp);
}

static bool ble_dbus_reply_ok(const char *out)
{
	return out && strstr(out, "method return") && !strstr(out, "Error ");
}

static int ble_bluez_unregister_path(const char *path)
{
	char cmd[512];
	char out[512];
	int rc;

	if (!ble_valid_dbus_path(path))
		return -1;

	/*
	 * This image has no busctl. dbus-send only succeeds when the sender
	 * matches the RegisterAdvertisement owner (librkwifibt). Keep it as
	 * a fallback; the owner-path Unregister is rk_ble_adv_stop().
	 */
	snprintf(cmd, sizeof(cmd),
		 "dbus-send --system --dest=org.bluez --type=method_call "
		 "--print-reply --reply-timeout=%d /org/bluez/hci0 "
		 "org.bluez.LEAdvertisingManager1.UnregisterAdvertisement "
		 "objpath:%s 2>&1",
		 BLE_BLUEZ_CMD_TIMEOUT_SEC * 1000, path);
	rc = ble_read_cmd_output(cmd, out, sizeof(out));
	if (ble_dbus_reply_ok(out)) {
		LOGI("BLE: UnregisterAdvertisement %s succeeded", path);
		return 0;
	}

	ble_trim_newline(out);
	LOGW("BLE: UnregisterAdvertisement %s failed rc=%d (%s)", path, rc,
	     out[0] ? out : "no reply");
	return -1;
}

static void ble_bluez_unregister_advertisements(void)
{
	static const char *const paths[] = {
		BLE_BLUEZ_ADV_PATH_RK,
		BLE_BLUEZ_ADV_PATH_HELPER,
		BLE_BLUEZ_ADV_PATH_HCI0,
	};
	size_t i;

	for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
		(void)ble_bluez_unregister_path(paths[i]);
}

static void ble_path_to_bt_addr(const char *path, char *addr, size_t addr_len)
{
	const char *p;
	size_t i = 0;

	if (!path || !addr || addr_len < 18)
		return;

	p = strstr(path, "/dev_");
	if (!p)
		return;
	p += 5;
	addr[0] = '\0';
	while (*p && i + 1 < addr_len) {
		if (*p == '_') {
			addr[i++] = ':';
		} else if (isxdigit((unsigned char)*p)) {
			addr[i++] = *p;
		} else {
			break;
		}
		p++;
	}
	addr[i] = '\0';
}

static bool ble_device_is_connected(const char *path)
{
	char cmd[384];
	char out[256];

	snprintf(cmd, sizeof(cmd),
		 "dbus-send --system --print-reply --dest=org.bluez "
		 "--reply-timeout=%d %s org.freedesktop.DBus.Properties.Get "
		 "string:org.bluez.Device1 string:Connected 2>&1",
		 BLE_BLUEZ_CMD_TIMEOUT_SEC * 1000, path);
	if (ble_read_cmd_output(cmd, out, sizeof(out)) == -1)
		return false;
	return strstr(out, "true") != NULL;
}

static void ble_get_device_addr_type(const char *path, char *type,
				     size_t type_len)
{
	char cmd[384];
	char out[256];
	char *p;
	char *end;

	snprintf(type, type_len, "%s", "public");
	snprintf(cmd, sizeof(cmd),
		 "dbus-send --system --print-reply --dest=org.bluez "
		 "--reply-timeout=%d %s org.freedesktop.DBus.Properties.Get "
		 "string:org.bluez.Device1 string:AddressType 2>&1",
		 BLE_BLUEZ_CMD_TIMEOUT_SEC * 1000, path);
	if (ble_read_cmd_output(cmd, out, sizeof(out)) == -1)
		return;

	p = strstr(out, "string \"");
	if (!p)
		return;
	p += 8;
	end = strchr(p, '"');
	if (!end || end == p)
		return;
	*end = '\0';
	if (strcmp(p, "public") == 0 || strcmp(p, "random") == 0)
		snprintf(type, type_len, "%s", p);
}

static void ble_disconnect_device_path(const char *path)
{
	char addr[18];
	char addr_type[16];
	char cmd[384];
	char out[256];
	static const char *const fallback_types[] = { "le", "auto", "public",
						      "random" };
	size_t i;

	if (!ble_valid_dbus_path(path) || !strstr(path, "/dev_"))
		return;
	if (!ble_device_is_connected(path))
		return;

	ble_path_to_bt_addr(path, addr, sizeof(addr));
	ble_get_device_addr_type(path, addr_type, sizeof(addr_type));

	if (addr[0] && rk_bt_is_open()) {
		LOGI("BLE: disconnecting central %s type=%s via rk_bt", addr,
		     addr_type);
		rk_bt_disconnect_by_addr(addr, addr_type);
	}

	/* Rockchip BlueZ Device1.Disconnect requires ADDR_TYPE. */
	snprintf(cmd, sizeof(cmd),
		 "dbus-send --system --dest=org.bluez --type=method_call "
		 "--print-reply --reply-timeout=%d %s "
		 "org.bluez.Device1.Disconnect string:%s 2>&1",
		 BLE_BLUEZ_CMD_TIMEOUT_SEC * 1000, path, addr_type);
	if (ble_read_cmd_output(cmd, out, sizeof(out)) != -1 &&
	    ble_dbus_reply_ok(out)) {
		LOGI("BLE: disconnected central %s type=%s",
		     addr[0] ? addr : path, addr_type);
		return;
	}

	for (i = 0; i < sizeof(fallback_types) / sizeof(fallback_types[0]);
	     i++) {
		if (strcmp(fallback_types[i], addr_type) == 0)
			continue;
		snprintf(cmd, sizeof(cmd),
			 "dbus-send --system --dest=org.bluez --type=method_call "
			 "--print-reply --reply-timeout=%d %s "
			 "org.bluez.Device1.Disconnect string:%s 2>&1",
			 BLE_BLUEZ_CMD_TIMEOUT_SEC * 1000, path,
			 fallback_types[i]);
		if (ble_read_cmd_output(cmd, out, sizeof(out)) != -1 &&
		    ble_dbus_reply_ok(out) && !ble_device_is_connected(path)) {
			LOGI("BLE: disconnected central %s type=%s",
			     addr[0] ? addr : path, fallback_types[i]);
			return;
		}
	}

	LOGW("BLE: failed to disconnect central %s", addr[0] ? addr : path);
}

static void ble_disconnect_connected_centrals(void)
{
	char cmd[256];
	char out[4096];
	char *p;
	char *end;
	char path[128];

	snprintf(cmd, sizeof(cmd),
		 "dbus-send --system --print-reply --dest=org.bluez "
		 "--reply-timeout=%d /org/bluez/hci0 "
		 "org.freedesktop.DBus.Introspectable.Introspect 2>&1",
		 BLE_BLUEZ_CMD_TIMEOUT_SEC * 1000);
	if (ble_read_cmd_output(cmd, out, sizeof(out)) == -1)
		return;

	p = out;
	while ((p = strstr(p, "node name=\"dev_")) != NULL) {
		p += 11; /* skip to dev_ */
		end = strchr(p, '"');
		if (!end)
			break;
		*end = '\0';
		snprintf(path, sizeof(path), "/org/bluez/hci0/%s", p);
		p = end + 1;
		ble_disconnect_device_path(path);
	}
}

static void ble_wait_advertising_inactive(void)
{
	int i;
	int active = -1;

	for (i = 0; i < BLE_ADV_STOP_CONFIRM_RETRY; i++) {
		active = ble_query_active_instances_dbus();
		if (active == 0) {
			LOGI("BLE: BlueZ ActiveInstances=0 after stop");
			return;
		}
		usleep(100 * 1000);
	}

	LOGW("BLE: ActiveInstances still %d after UnregisterAdvertisement",
	     active);
}

static bool ble_confirm_advertising_started(void)
{
	int active_instances = ble_query_active_instances();

	if (active_instances <= 0)
		return false;

	s_ble_advertised = true;
	s_ble_running = true;
	s_ble_confirm_deadline = 0;
	s_ble_runtime_loss_deadline = 0;
	s_manual_adv_retry_time = 0;
	s_power_on_fail_count = 0;
	s_bt_content.ble_content.ble_advertised = true;
	LOGI("BLE: advertising started (confirmed via BlueZ ActiveInstances=%d)",
	     active_instances);
	return true;
}

static void ble_mark_advertising_lost(const char *reason, int active_instances)
{
	if (!s_ble_running)
		return;

	s_ble_advertised = false;
	s_ble_running = false;
	s_ble_confirm_deadline = 0;
	s_ble_runtime_loss_deadline = 0;
	s_manual_adv_retry_time = 0;
	s_bt_content.ble_content.ble_advertised = false;
	LOGW("BLE: advertising inactive (%s, BlueZ ActiveInstances=%d)",
	     reason, active_instances);

	/*
	 * Without a scheduled restart the device stays non-discoverable
	 * for the rest of the uptime once advertising is lost. That blocks
	 * re-provisioning over BLE entirely.
	 */
	ble_schedule_start_retry_locked(BLE_START_RETRY_SEC * 2);
}

static void ble_poll_advertising_status(void)
{
	int active_instances;
	time_t now;

	if (!s_ble_running)
		return;

	now = time_now_sec();
	if (now == s_ble_last_probe_time)
		return;

	/*
	 * A connected central legitimately suspends advertising. Running the
	 * runtime-loss logic then tears the session state down underneath the
	 * very client we are serving.
	 */
	if (s_session_addr[0] != '\0' || s_notify_enabled) {
		s_ble_runtime_loss_deadline = 0;
		return;
	}

	s_ble_last_probe_time = now;
	active_instances = ble_query_active_instances();

	if (active_instances > 0) {
		if (s_ble_runtime_loss_deadline != 0) {
			LOGI("BLE: ActiveInstances recovered before runtime-loss timeout");
			s_ble_runtime_loss_deadline = 0;
		}
		if (!s_ble_advertised)
			(void)ble_confirm_advertising_started();
		return;
	}

	if (s_ble_advertised) {
		if (s_ble_runtime_loss_deadline == 0) {
			s_ble_runtime_loss_deadline =
				now + BLE_ADV_RUNTIME_LOSS_GRACE_SEC;
			LOGW("BLE: ActiveInstances dropped to %d, waiting %ds before recovery",
			     active_instances, BLE_ADV_RUNTIME_LOSS_GRACE_SEC);
			return;
		}

		if (now < s_ble_runtime_loss_deadline)
			return;

		ble_log_bluez_state_snapshot("runtime-loss-timeout");
		ble_mark_advertising_lost("runtime-loss-timeout", active_instances);
		return;
	}

	if (s_ble_confirm_deadline != 0 && now < s_ble_confirm_deadline &&
	    s_manual_adv_retry_time != 0 && now >= s_manual_adv_retry_time) {
		if (ble_retry_manual_bluez_advertiser() == 0)
			s_manual_adv_retry_time = now + 2;
		else
			s_manual_adv_retry_time = now + 1;
	}

	if (s_ble_confirm_deadline != 0 && now >= s_ble_confirm_deadline) {
		ble_log_bluez_state_snapshot("adv-grace-expired");
		ble_mark_advertising_lost("startup-grace-expired",
					  active_instances);
		return;
	}

	/*
	 * Stuck state: the session ended but BlueZ did not resume
	 * advertising, and no recovery deadline is pending. Re-arm the
	 * confirm window so the existing grace/mark-lost/retry path fires.
	 */
	if (s_ble_confirm_deadline == 0) {
		s_ble_confirm_deadline =
			now + BLE_ADV_RUNTIME_LOSS_GRACE_SEC;
		LOGW("BLE: not advertising with no recovery pending, "
		     "re-arming grace (%ds)", BLE_ADV_RUNTIME_LOSS_GRACE_SEC);
	}
}

static const char *ble_current_name(void)
{
	return s_bt_content.bt_name[0] ? s_bt_content.bt_name :
					 NET_DEFAULT_DEVICE_NAME;
}

static const char *ble_provision_state_string(void)
{
	return provision_state_str(g_config.provision.provision_state);
}

static const char *ble_network_state_string(int state)
{
	return network_state_str(state);
}

static const char *ble_current_network_mode(void)
{
	return network_mode_str(g_config.provision.network_mode);
}

static const char *ble_current_ip(const provision_run_status_t *run,
				      int live_state, const char *live_ip)
{
	if (run && run->ip[0])
		return run->ip;
	if (strcmp(ble_current_network_mode(), "ap") == 0)
		return g_config.provision.http_host[0] ?
			       g_config.provision.http_host :
			       "192.168.4.1";
	if (live_state == NET_NETWORK_STATE_ONLINE && live_ip && live_ip[0])
		return live_ip;
	if (g_config.provision.network_state == NET_NETWORK_STATE_ONLINE)
		return g_config.networks[0].ip;
	return "";
}

static void ble_cache_event_payload(const char *payload)
{
	size_t len;

	if (!payload) {
		s_last_event[0] = '\0';
		s_last_event_len = 0;
		return;
	}

	len = strlen(payload);
	if (len > BT_ATT_MAX_VALUE_LEN)
		len = BT_ATT_MAX_VALUE_LEN;
	memcpy(s_last_event, payload, len);
	s_last_event[len] = '\0';
	s_last_event_len = (int)len;
}

/*
 * BlueZ retires the advertising instance as soon as a central connects, so
 * s_ble_running goes false in the middle of a perfectly healthy GATT session.
 * Gating notifications on it silently dropped every provisioning result, and
 * the phone sat there until it timed out. A live notify subscription is the
 * only precondition that actually matters; the payload is cached either way so
 * a client that reads the characteristic still sees the last event.
 */
static int ble_push_notify_payload(const char *payload)
{
	ble_cache_event_payload(payload);
	if (!s_notify_enabled || s_last_event_len <= 0)
		return 0;
	return rk_ble_send_notify(BLE_CHR_NOTIFY_UUID, s_last_event,
				  s_last_event_len);
}

static cJSON *ble_build_info_data(void)
{
	cJSON *data = cJSON_CreateObject();

	ble_compat_sync_config();
	cJSON_AddStringToObject(data, "device_name", ble_current_name());
	cJSON_AddStringToObject(data, "device_id", g_config.hsi_info.id);
	cJSON_AddStringToObject(data, "serial_no", g_config.hsi_info.serial_no);
	cJSON_AddStringToObject(data, "model", g_config.hsi_info.model);
	cJSON_AddStringToObject(data, "fw_version",
				g_config.hsi_info.firmware_version);
	cJSON_AddBoolToObject(data, "provisioned",
			      g_config.provision.provision_state ==
				      NET_PROVISION_STATE_PROVISIONED);
	cJSON_AddBoolToObject(data, "commissioning_open",
			      provision_is_commissioning_open() ? 1 : 0);
	cJSON_AddStringToObject(data, "ble_name", ble_current_name());
	return data;
}

static cJSON *ble_build_status_data(void)
{
	cJSON *data = cJSON_CreateObject();
	cJSON *last_error = cJSON_CreateObject();
	provision_run_status_t run;
	char live_ip[48] = { 0 };
	int live_state = g_config.provision.network_state;
	const char *stage;
	const char *reason;
	int error_code;

	ble_compat_sync_config();
	memset(&run, 0, sizeof(run));
	provision_get_run_status(&run);
	if (ble_wifi_is_connected(live_ip, sizeof(live_ip)))
		live_state = NETWORK_STATE_ONLINE;
	stage = run.stage[0] ? run.stage : g_config.provision.last_error_stage;
	if (!stage[0] || strcmp(stage, "idle") == 0) {
		if (run.state == PROVISION_RUN_SUCCESS)
			stage = "done";
		else if (run.state == PROVISION_RUN_FAILED)
			stage = "failed";
		else
			stage = "idle";
	}

	error_code = run.state == PROVISION_RUN_FAILED ?
			     run.error_code :
			     g_config.provision.last_error_code;
	reason = run.state == PROVISION_RUN_FAILED && run.error_reason[0] ?
			 run.error_reason :
			 g_config.provision.last_error_reason;

	cJSON_AddStringToObject(data, "provision_state",
				ble_provision_state_string());
	cJSON_AddStringToObject(data, "network_state",
				ble_network_state_string(live_state));
	cJSON_AddStringToObject(data, "network_mode", ble_current_network_mode());
	cJSON_AddStringToObject(data, "stage", stage);
	cJSON_AddStringToObject(data, "saved_ssid", g_config.provision.ssid);
	cJSON_AddStringToObject(data, "ip",
				ble_current_ip(&run, live_state, live_ip));
	cJSON_AddBoolToObject(data, "commissioning_open",
			      provision_is_commissioning_open() ? 1 : 0);
	cJSON_AddNumberToObject(last_error, "code", error_code);
	cJSON_AddStringToObject(last_error, "reason",
				reason && reason[0] ? reason : "none");
	cJSON_AddItemToObject(data, "last_error", last_error);
	return data;
}

static int ble_serialize_response(const char *op, int req_id, int code,
				  const char *msg, cJSON *data, char *out,
				  size_t out_size)
{
	cJSON *root;
	char *json;
	size_t len;
	int rc = -1;

	if (!op || !msg || !out || out_size == 0) {
		if (data)
			cJSON_Delete(data);
		return -1;
	}

	root = cJSON_CreateObject();
	if (!root) {
		if (data)
			cJSON_Delete(data);
		return -1;
	}

	cJSON_AddStringToObject(root, "op", op);
	if (req_id != BLE_REQ_ID_NONE)
		cJSON_AddNumberToObject(root, "req_id", req_id);
	cJSON_AddNumberToObject(root, "code", code);
	cJSON_AddStringToObject(root, "msg", msg);
	if (data)
		cJSON_AddItemToObject(root, "data", data);
	else
		cJSON_AddItemToObject(root, "data", cJSON_CreateObject());

	json = cJSON_PrintUnformatted(root);
	if (!json)
		goto out;

	len = strlen(json);
	if (len >= out_size)
		goto out;

	memcpy(out, json, len + 1);
	rc = 0;

out:
	free(json);
	cJSON_Delete(root);
	return rc;
}

static void ble_send_simple_error(const char *op, int req_id, int code,
				  const char *msg)
{
	if (req_id != BLE_REQ_ID_NONE) {
		char with_req[224];

		snprintf(with_req, sizeof(with_req),
			 "{\"op\":\"%s\",\"req_id\":%d,\"code\":%d,"
			 "\"msg\":\"%s\",\"data\":{}}",
			 op ? op : "unknown", req_id, code,
			 msg ? msg : "Error");
		(void)ble_push_notify_payload(with_req);
		return;
	}

	{
		char payload[192];

		snprintf(payload, sizeof(payload),
			 "{\"op\":\"%s\",\"code\":%d,\"msg\":\"%s\","
			 "\"data\":{}}",
			 op ? op : "unknown", code, msg ? msg : "Error");
		(void)ble_push_notify_payload(payload);
	}
}

static void ble_send_response(const char *op, int req_id, int code,
			      const char *msg, cJSON *data)
{
	char payload[BT_ATT_MAX_VALUE_LEN + 1];

	if (ble_serialize_response(op, req_id, code, msg, data, payload,
				   sizeof(payload)) != 0) {
		ble_send_simple_error(op, req_id, BLE_CODE_TOO_LARGE,
				      "payload_too_large");
		return;
	}

	(void)ble_push_notify_payload(payload);
}

static void ble_send_info_event(int req_id)
{
	ble_send_response("info", req_id, EC_OK, "OK", ble_build_info_data());
}

static void ble_send_status_event(int req_id)
{
	ble_send_response("status", req_id, EC_OK, "OK", ble_build_status_data());
}

static void ble_send_not_allowed(const char *op, int req_id)
{
	ble_send_response(op, req_id, BLE_PROVISION_NOT_ALLOWED_CODE,
			  "commissioning_closed", NULL);
}

static void ble_send_busy(const char *op, int req_id)
{
	ble_send_response(op, req_id, BLE_CODE_BUSY, "busy", NULL);
}

static void ble_send_invalid_param(const char *op, int req_id,
				   const char *reason)
{
	ble_send_response(op, req_id, EC_INVALID_PARAM,
			  reason && reason[0] ? reason : "invalid_param", NULL);
}

static void ble_clear_async_request(void)
{
	memset(&s_async_request, 0, sizeof(s_async_request));
	s_async_probe_time = 0;
}

static void ble_start_async_request(const char *op, int req_id)
{
	ble_clear_async_request();
	s_async_request.active = true;
	s_async_request.req_id = req_id;
	snprintf(s_async_request.op, sizeof(s_async_request.op), "%s", op);
	s_async_request.last_state = PROVISION_RUN_IDLE;
}

static int ble_extract_req_id(cJSON *root, int *req_id)
{
	cJSON *item;

	if (!root || !req_id)
		return -1;

	item = cJSON_GetObjectItemCaseSensitive(root, "req_id");
	if (!cJSON_IsNumber(item))
		return -1;
	*req_id = item->valueint;
	return 0;
}

static void ble_queue_command(const char *data, int len)
{
	pthread_mutex_lock(&s_mutex);
	if (s_pending_command.pending) {
		pthread_mutex_unlock(&s_mutex);
		ble_send_busy("busy", BLE_REQ_ID_NONE);
		return;
	}

	memset(&s_pending_command, 0, sizeof(s_pending_command));
	if (len > BT_ATT_MAX_VALUE_LEN) {
		s_pending_command.too_large = true;
		s_pending_command.pending = true;
		pthread_mutex_unlock(&s_mutex);
		return;
	}

	memcpy(s_pending_command.payload, data, (size_t)len);
	s_pending_command.payload[len] = '\0';
	s_pending_command.len = len;
	__sync_synchronize();
	s_pending_command.pending = true;
	pthread_mutex_unlock(&s_mutex);
}

static void ble_read_current_snapshot(char *data, int *len)
{
	char payload[BT_ATT_MAX_VALUE_LEN + 1];

	if (!data || !len || *len <= 0)
		return;

	if (s_last_event_len > 0) {
		memcpy(data, s_last_event, (size_t)s_last_event_len);
		*len = s_last_event_len;
		return;
	}

	if (ble_serialize_response("info", BLE_REQ_ID_NONE, EC_OK, "OK",
				   ble_build_info_data(), payload,
				   sizeof(payload)) != 0) {
		*len = 0;
		return;
	}

	*len = (int)strlen(payload);
	memcpy(data, payload, (size_t)*len);
}

static void ble_recv_data_callback(const char *uuid, char *data, int *len,
				   RK_BLE_GATT_STATE state)
{
	switch (state) {
	case RK_BLE_GATT_SERVER_READ_BY_REMOTE:
		LOGD("BLE read by remote uuid=%s", uuid);
		if (strcmp(uuid, BLE_CHR_WRITE_UUID) == 0 ||
		    strcmp(uuid, BLE_CHR_NOTIFY_UUID) == 0)
			ble_read_current_snapshot(data, len);
		break;

	case RK_BLE_GATT_SERVER_WRITE_BY_REMOTE:
		LOGI("BLE write by remote uuid=%s len=%d", uuid, *len);
		if (strcmp(uuid, BLE_CHR_WRITE_UUID) == 0 && data && len && *len > 0)
			ble_queue_command(data, *len);
		break;

	case RK_BLE_GATT_SERVER_ENABLE_NOTIFY_BY_REMOTE:
		LOGD("BLE notify enabled uuid=%s", uuid);
		if (strcmp(uuid, BLE_CHR_NOTIFY_UUID) == 0) {
			s_notify_enabled = true;
			/*
			 * Some vendor builds skip RK_BT_STATE_BLE_CONNECTED;
			 * a notify subscription proves a central is attached.
			 */
			provision_open_commissioning_window(0);
			ble_send_info_event(BLE_REQ_ID_NONE);
		}
		break;

	case RK_BLE_GATT_SERVER_DISABLE_NOTIFY_BY_REMOTE:
		LOGD("BLE notify disabled uuid=%s", uuid);
		s_notify_enabled = false;
		break;

	case RK_BLE_GATT_MTU:
		LOGD("BLE MTU: %d", *(uint16_t *)data);
		break;

	default:
		break;
	}
}

static void bt_state_callback(RkBtRemoteDev *rdev, RK_BT_STATE state)
{
	switch (state) {
	case RK_BT_STATE_INIT_ON:
		LOGI("BLE: BT stack initialized");
		s_bt_inited = true;
		break;
	case RK_BT_STATE_INIT_OFF:
		LOGI("BLE: BT stack deinitialized");
		s_bt_inited = false;
		s_ble_advertised = false;
		s_ble_running = false;
		s_bt_content.ble_content.ble_advertised = false;
		break;
	case RK_BT_STATE_ADAPTER_BLE_ADV_START:
		/*
		 * Rockchip can raise the vendor start callback before BlueZ
		 * exposes a live LE advertising instance. Only treat BlueZ's
		 * ActiveInstances>0 as the final success signal.
		 */
		LOGI("BLE: advertising start callback received");
		ble_log_bluez_state_snapshot("adv-start-callback");
		break;
	case RK_BT_STATE_ADAPTER_BLE_ADV_STOP:
		/*
		 * The vendor can emit ADV_STOP before BlueZ has held an
		 * advertisement (or while HCI 0x2036 is still in flight).
		 * Clearing s_ble_running here makes the main loop tear the
		 * GATT session down every ~10s, so advertising is never
		 * reached. Ignore stop during confirmation grace.
		 */
		if (s_ble_confirm_deadline != 0 &&
		    time_now_sec() < s_ble_confirm_deadline) {
			LOGW("BLE: vendor adv-stop during confirmation grace, keeping session");
			break;
		}
		/*
		 * Advertising always stops once a central is connected. That is
		 * the normal course of a provisioning session, not a failure.
		 */
		if (s_session_addr[0] != '\0' || s_notify_enabled) {
			s_ble_advertised = false;
			s_bt_content.ble_content.ble_advertised = false;
			LOGI("BLE: advertising stopped while central connected, session kept");
			break;
		}
		s_ble_advertised = false;
		s_ble_running = false;
		s_bt_content.ble_content.ble_advertised = false;
		LOGI("BLE: advertising stopped");
		break;
	case RK_BT_STATE_ADAPTER_BLE_ADV_ERR:
		LOGE("BLE: advertising error callback received");
		ble_log_bluez_state_snapshot("adv-error");
		break;
	case RK_BT_STATE_ADAPTER_LOCAL_NAME:
		LOGI("BLE: local name applied");
		break;
	case RK_BT_STATE_COMMAND_RESP_ERR:
		LOGW("BLE: command response error callback received");
		break;
	case RK_BT_STATE_BLE_CONNECTED:
		/*
		 * A central that got through pairing-free BLE is the only peer
		 * that can reconfigure us, so its presence is the window. Without
		 * this every op is rejected with commissioning_closed once the
		 * device is provisioned, leaving no way back in over BLE.
		 */
		provision_open_commissioning_window(0);
		if (rdev && rdev->remote_address[0]) {
			if (s_session_addr[0] == '\0') {
				snprintf(s_session_addr, sizeof(s_session_addr), "%s",
					 rdev->remote_address);
				snprintf(s_session_addr_type,
					 sizeof(s_session_addr_type), "%s",
					 rdev->remote_address_type[0] ?
						 rdev->remote_address_type :
						 "auto");
				LOGI("BLE: client connected %s type=%s",
				     s_session_addr, s_session_addr_type);
			} else if (strcmp(s_session_addr, rdev->remote_address) != 0) {
				LOGW("BLE: rejecting second client %s while %s owns the session",
				     rdev->remote_address, s_session_addr);
				rk_bt_disconnect_by_addr(
					rdev->remote_address,
					rdev->remote_address_type[0] ?
						rdev->remote_address_type :
						"auto");
			}
		} else {
			LOGI("BLE: client connected");
		}
		break;
	case RK_BT_STATE_BLE_DISCONN:
		LOGI("BLE: client disconnected%s%s",
		     rdev && rdev->remote_address[0] ? " " : "",
		     rdev && rdev->remote_address[0] ? rdev->remote_address : "");
		if (!rdev || !rdev->remote_address[0] ||
		    strcmp(s_session_addr, rdev->remote_address) == 0) {
			s_session_addr[0] = '\0';
			s_session_addr_type[0] = '\0';
			s_notify_enabled = false;
			provision_close_commissioning_window();
		}
		break;
	default:
		break;
	}
}

static bool bt_vendor_callback(bool enable)
{
	if (enable) {
		if (!ble_prepare_controller_ready()) {
			LOGW("BLE: controller not ready in vendor callback; keep process alive and retry later");
			return true;
		}
	}
	return true;
}

static void setup_gatt_service(void)
{
	RkBleContent *ble = &s_bt_content.ble_content;
	RkBleGattService *service = &ble->gatt_instance[0];

	ble->srv_cnt = 1;
	/*
	 * Use the SAME 128-bit UUID for both the advertisement and the GATT
	 * service.  The Rockchip library registers a GATT application for
	 * adv_server_uuid AND for each gatt_instance[].server_uuid.  When
	 * these were different strings ("1234" vs "00001234-…") the library
	 * failed to de-duplicate and BlueZ ended up with two identical
	 * service entries → the scanner showed two 0x1234 services.
	 */
	ble->adv_server_uuid.uuid = BLE_SERVICE_UUID;
	ble->Appearance = 0x0080;

	service->server_uuid.uuid = BLE_SERVICE_UUID;
	service->chr_cnt = 2;

	service->chr_uuid[0].uuid = BLE_CHR_WRITE_UUID;
	service->chr_uuid[0].chr_props = s_chr_props_write;

	service->chr_uuid[1].uuid = BLE_CHR_NOTIFY_UUID;
	service->chr_uuid[1].chr_props = s_chr_props_notify;

	ble->cb_ble_recv_fun = ble_recv_data_callback;
}

static void ble_full_bt_reset(void)
{
	LOGW("BLE: performing full BT reset after %d consecutive power-on failures",
	     s_power_on_fail_count);

	/* Tear down vendor BT stack completely */
	if (rk_bt_is_open()) {
		rk_ble_adv_stop();
		rk_bt_deinit();
	}
	s_bt_inited = false;
	s_bt_init_done = false;
	s_ble_advertised = false;
	s_ble_running = false;
	s_bt_content.ble_content.ble_advertised = false;

	/* Kill bluetoothd */
	(void)system("killall bluetoothd >/dev/null 2>&1");
	usleep(500 * 1000);

	/* Re-bind AIC BT USB interface */
	if (access(BLE_AIC_BT_BIND_INIT_SCRIPT, X_OK) == 0) {
		LOGI("BLE: re-binding AIC BT USB interface");
		(void)system(BLE_AIC_BT_BIND_INIT_SCRIPT
			     " restart >/dev/null 2>&1");
		usleep(1000 * 1000);
	}

	/* Wait for hci0 to reappear */
	if (!ble_wait_hci0_present(BLE_HCI0_WAIT_SEC)) {
		LOGW("BLE: hci0 did not reappear after full reset");
	}

	s_power_on_fail_count = 0;
}

int net_ble_advertiser_start(const net_ble_advertisement_t *adv)
{
	int init_timeout;
	int adv_timeout;
	int active_instances;
	int ret;
	const char *ble_name;
	bool reuse_bt_session = false;

	if (!adv)
		return -1;

	if (s_power_on_fail_count >= 2)
		ble_full_bt_reset();

	pthread_mutex_lock(&s_mutex);

	if (s_ble_running) {
		if (!s_ble_advertised)
			LOGW("BLE: advertising start already pending confirmation");
		pthread_mutex_unlock(&s_mutex);
		return 0;
	}

	ble_stop_manual_bluez_advertiser();
	memset(&s_pending_command, 0, sizeof(s_pending_command));
	ble_clear_async_request();
	s_notify_enabled = false;
	s_session_addr[0] = '\0';
	s_session_addr_type[0] = '\0';
	ble_cache_event_payload(NULL);
	ble_name = ble_get_runtime_name(adv);
	ble_cache_retry_advertisement(adv);
	rk_bt_register_state_callback(bt_state_callback);
	rk_bt_register_vendor_callback(bt_vendor_callback);

	if (!ble_prepare_controller_ready()) {
		LOGW("BLE: controller unavailable, deferring advertiser start");
		s_ble_advertised = false;
		s_ble_running = false;
		s_bt_content.ble_content.ble_advertised = false;
		ble_schedule_start_retry_locked(BLE_START_RETRY_SEC);
		pthread_mutex_unlock(&s_mutex);
		return 0;
	}

	ble_clear_start_retry_locked();

	reuse_bt_session = rk_bt_is_open() || s_bt_init_done;
	if (reuse_bt_session) {
		active_instances = ble_query_active_instances();
		LOGW("BLE: BT stack already open (ActiveInstances=%d), reusing session",
		     active_instances);
		s_bt_inited = true;
	} else {
		/*
		 * Wait for aic_btusb firmware download to finish before
		 * killing bluetoothd.  btusb_shutdown sends HCI_RESET;
		 * if the controller firmware isn't ready, the reset times
		 * out (~2s) and leaves the controller in a bad state where
		 * advertising never works.
		 */
		if (ble_is_hci0_present()) {
			int pw = ble_query_powered_state();
			if (pw <= 0) {
				LOGI("BLE: hci0 present but not powered, waiting for controller init");
				(void)ble_wait_hci0_present(5);
				usleep(2000 * 1000);
			}
		}
		LOGI("BLE: restarting bluetoothd for clean GATT state");
		(void)system("killall bluetoothd >/dev/null 2>&1");
		usleep(3000 * 1000);
		(void)system("/etc/init.d/S40bluetoothd start >/dev/null 2>&1");
		usleep(1000 * 1000);

		memset(&s_bt_content, 0, sizeof(s_bt_content));
		ble_prepare_bt_content_sync_primitives();

		snprintf(s_bt_content.bt_name, sizeof(s_bt_content.bt_name),
			 "%s", ble_name);
		snprintf(s_bt_content.ble_content.ble_name,
			 sizeof(s_bt_content.ble_content.ble_name),
			 "%s", ble_name);

		s_bt_content.io_capability = IO_CAPABILITY_NOINPUTNOOUTPUT;
		s_bt_content.profile = PROFILE_BLE;
		s_bt_content.bluealsa = false;

		setup_gatt_service();
		ble_write_bt_main_conf(ble_name);

		if (!s_bt_debug_inited) {
			rk_debug_init(true);
			s_bt_debug_inited = true;
		}

		s_bt_inited = false;
		ret = rk_bt_init(&s_bt_content);

		s_bt_init_done = true;
		if (ret != 0) {
			LOGE("BLE: rk_bt_init failed: %d", ret);
			ble_schedule_start_retry_locked(BLE_START_RETRY_SEC);
			pthread_mutex_unlock(&s_mutex);
			return -1;
		}

		init_timeout = BLE_INIT_TIMEOUT_SEC * 10;
		while (!s_bt_inited && init_timeout-- > 0)
			usleep(100 * 1000);

		if (!s_bt_inited) {
			LOGW("BLE: INIT_ON callback not observed, continuing after rk_bt_init success");
			s_bt_inited = true;
		}
	}

	if (!ble_ensure_adapter_powered()) {
		LOGW("BLE: Powered=yes not confirmed, continuing to rk_ble_adv_start");
	}

	LOGI("BLE: applying local name '%s' before advertising", ble_name);
	rk_bt_set_local_name((char *)ble_name);
	LOGI("BLE: requesting discoverable mode before advertising");
	rk_bt_set_discoverable(true);
	/* Conf DiscoverableTimeout=0 is ignored if BlueZ already stored 180. */
	(void)system("bluetoothctl discoverable-timeout 0 >/dev/null 2>&1");

	s_ble_advertised = false;
	s_ble_running = true;
	s_ble_last_probe_time = 0;
	s_ble_confirm_deadline = time_now_sec() + BLE_ADV_CONFIRM_GRACE_SEC;
	s_ble_runtime_loss_deadline = 0;
	s_manual_adv_retry_time = 0;
	s_bt_content.ble_content.ble_advertised = false;

#if BLE_USE_CUSTOM_ADV_INTERVAL
	ret = rk_ble_set_adv_interval(BLE_ADV_INTERVAL_MIN, BLE_ADV_INTERVAL_MAX);
	if (ret != 0) {
		LOGW("BLE: rk_ble_set_adv_interval failed: %d, continuing with BlueZ defaults",
		     ret);
	}
#else
	LOGI("BLE: skipping rk_ble_set_adv_interval to keep BlueZ in control");
#endif

	ret = rk_ble_adv_start();
	if (ret != 0) {
		LOGE("BLE: rk_ble_adv_start failed: %d", ret);
		s_ble_advertised = false;
		s_ble_running = false;
		s_ble_confirm_deadline = 0;
		s_ble_runtime_loss_deadline = 0;
		s_manual_adv_retry_time = 0;
		s_bt_content.ble_content.ble_advertised = false;
		ble_schedule_start_retry_locked(BLE_START_RETRY_SEC);
		pthread_mutex_unlock(&s_mutex);
		return -1;
	}
	LOGI("BLE: rk_ble_adv_start requested successfully");

	if (ble_manual_bluez_advertiser_enabled()) {
		ret = ble_start_manual_bluez_advertiser(ble_name);
		if (ret != 0) {
			LOGE("BLE: manual BlueZ advertisement start failed");
			s_ble_advertised = false;
			s_ble_running = false;
			s_ble_confirm_deadline = 0;
			s_ble_runtime_loss_deadline = 0;
			s_manual_adv_retry_time = 0;
			s_bt_content.ble_content.ble_advertised = false;
			ble_schedule_start_retry_locked(BLE_START_RETRY_SEC);
			pthread_mutex_unlock(&s_mutex);
			return -1;
		}
	}

	adv_timeout = BLE_ADV_START_TIMEOUT_SEC * 10;
	while (!s_ble_advertised && adv_timeout-- > 0) {
		if ((adv_timeout % 5) == 0 && ble_confirm_advertising_started())
			break;
		usleep(100 * 1000);
	}

	if (!s_ble_advertised && !ble_confirm_advertising_started()) {
		ble_log_bluez_state_snapshot("adv-pending");
		LOGW("BLE: advertising not confirmed within startup timeout, keeping advertiser alive for background confirmation");
	}

	pthread_mutex_unlock(&s_mutex);

	LOGI("BLE advertise started: device_id=%s ssid=%s host=%s:%d",
	     adv->device_id, adv->hotspot_ssid, adv->http_host,
	     adv->http_port_no);
	return 0;
}

int net_ble_advertiser_stop(void)
{
	bool call_vendor_stop = false;

	pthread_mutex_lock(&s_mutex);

	ble_stop_manual_bluez_advertiser();
	call_vendor_stop = s_ble_running || s_ble_advertised || s_bt_inited ||
			   rk_bt_is_open();

	s_ble_advertised = false;
	s_ble_running = false;
	s_ble_confirm_deadline = 0;
	s_ble_runtime_loss_deadline = 0;
	s_manual_adv_retry_time = 0;
	s_bt_content.ble_content.ble_advertised = false;
	s_notify_enabled = false;
	s_session_addr[0] = '\0';
	s_session_addr_type[0] = '\0';
	ble_clear_start_retry_locked();
	memset(&s_pending_command, 0, sizeof(s_pending_command));
	ble_clear_async_request();
	pthread_mutex_unlock(&s_mutex);

	ble_disconnect_connected_centrals();

	if (call_vendor_stop)
		rk_ble_adv_stop();

	ble_bluez_unregister_advertisements();
	ble_wait_advertising_inactive();
	LOGI("BLE advertise stopped ActiveInstances=%d (BT stack left idle, rk_bt_deinit skipped)",
	     ble_query_active_instances_dbus());
	return 0;
}


bool net_ble_is_running(void)
{
	return s_ble_running || s_bt_inited || s_notify_enabled ||
	       s_session_addr[0] != '\0';
}

static void ble_handle_wifi_sta_apply(const char *payload, int req_id)
{
	char ssid[64] = { 0 };
	char password[128] = { 0 };
	cJSON *data;
	int rc;

	if (!provision_is_commissioning_open()) {
		ble_send_not_allowed("wifi_sta_apply", req_id);
		return;
	}
	if (s_async_request.active) {
		ble_send_busy("wifi_sta_apply", req_id);
		return;
	}
	if (ble_parse_wifi_sta_json(payload, (int)strlen(payload),
				    ssid, sizeof(ssid),
				    password, sizeof(password)) != 0) {
		ble_send_invalid_param("wifi_sta_apply", req_id,
				       "invalid_wifi_sta_apply");
		return;
	}

	rc = ble_provision_request_wifi(ssid, password);
	if (rc == -EBUSY) {
		ble_send_busy("wifi_sta_apply", req_id);
		return;
	}
	if (rc != 0) {
		ble_send_invalid_param("wifi_sta_apply", req_id,
				       "invalid_credentials");
		return;
	}

	ble_start_async_request("wifi_sta_apply", req_id);
	snprintf(s_async_request.last_stage, sizeof(s_async_request.last_stage),
		 "queued");
	data = cJSON_CreateObject();
	cJSON_AddStringToObject(data, "stage", "queued");
	ble_send_response("wifi_sta_apply", req_id, EC_OK, "Accepted", data);
}

static void ble_handle_wifi_ap_apply(const char *payload, int req_id)
{
	char ssid[64];
	char password[128];
	int channel = 0;
	int has_password = 0;
	int rc;
	cJSON *data;

	if (!provision_is_commissioning_open()) {
		ble_send_not_allowed("wifi_ap_apply", req_id);
		return;
	}
	if (ble_parse_hotspot_json(payload, (int)strlen(payload),
				  ssid, sizeof(ssid),
				  password, sizeof(password),
				  &has_password, &channel) != 0) {
		ble_send_invalid_param("wifi_ap_apply", req_id,
				       "invalid_wifi_ap_apply");
		return;
	}

	ble_compat_sync_config();
	rc = ble_config_set_hotspot(ssid, has_password ? password : "", channel);
	if (rc != 0) {
		ble_send_response("wifi_ap_apply", req_id,
				  BLE_PROVISION_AP_ERROR_CODE,
				  "wifi_ap_apply_failed", NULL);
		return;
	}

	/* Actually start AP mode. */
	rc = provision_request_ap(ssid, has_password ? password : "", channel);
	if (rc != 0) {
		ble_send_response("wifi_ap_apply", req_id,
				  BLE_PROVISION_AP_ERROR_CODE,
				  "ap_start_failed", NULL);
		return;
	}

	data = cJSON_CreateObject();
	cJSON_AddStringToObject(data, "network_mode", "ap");
	cJSON_AddStringToObject(data, "ip",
			       g_ble_config.provision.http_host[0] ?
				       g_ble_config.provision.http_host :
				       "192.168.4.1");
	ble_send_response("wifi_ap_apply", req_id, EC_OK, "OK", data);
}

static void ble_handle_reset_provision(int req_id)
{
	cJSON *data;
	int rc;

	if (!provision_is_commissioning_open()) {
		ble_send_not_allowed("reset_provision", req_id);
		return;
	}

	rc = provision_reset_to_ble();
	if (rc == -EBUSY) {
		ble_send_busy("reset_provision", req_id);
		return;
	}
	if (rc != 0) {
		ble_send_response("reset_provision", req_id,
				  BLE_PROVISION_AP_ERROR_CODE,
				  "provision_reset_failed", NULL);
		return;
	}

	data = cJSON_CreateObject();
	cJSON_AddStringToObject(data, "provision_state", "unprovisioned");
	cJSON_AddBoolToObject(data, "commissioning_open", 1);
	ble_send_response("reset_provision", req_id, EC_OK, "Provision reset",
			  data);
}

static void ble_process_command_payload(const char *payload, bool too_large)
{
	cJSON *root;
	cJSON *op_item;
	int req_id = BLE_REQ_ID_NONE;
	const char *op;
	char op_buf[32] = { 0 };

	if (too_large) {
		ble_send_simple_error("unknown", BLE_REQ_ID_NONE,
				      BLE_CODE_TOO_LARGE,
				      "payload_too_large");
		return;
	}
	if (!payload || payload[0] == '\0') {
		ble_send_invalid_param("unknown", BLE_REQ_ID_NONE, "empty_payload");
		return;
	}

	root = cJSON_Parse(payload);
	if (!root) {
		ble_send_invalid_param("unknown", BLE_REQ_ID_NONE, "invalid_json");
		return;
	}

	op_item = cJSON_GetObjectItemCaseSensitive(root, "op");
	if (!cJSON_IsString(op_item) || !op_item->valuestring) {
		cJSON_Delete(root);
		ble_send_invalid_param("unknown", BLE_REQ_ID_NONE, "missing_op");
		return;
	}
	snprintf(op_buf, sizeof(op_buf), "%s", op_item->valuestring);
	op = op_buf;
	if (ble_extract_req_id(root, &req_id) != 0) {
		cJSON_Delete(root);
		ble_send_invalid_param(op, BLE_REQ_ID_NONE, "missing_req_id");
		return;
	}
	cJSON_Delete(root);

	if (strcmp(op, "info") == 0)
		ble_send_info_event(req_id);
	else if (strcmp(op, "status") == 0)
		ble_send_status_event(req_id);
	else if (strcmp(op, "wifi_sta_apply") == 0)
		ble_handle_wifi_sta_apply(payload, req_id);
	else if (strcmp(op, "wifi_ap_apply") == 0)
		ble_handle_wifi_ap_apply(payload, req_id);
	else if (strcmp(op, "reset_provision") == 0)
		ble_handle_reset_provision(req_id);
	else
		ble_send_invalid_param(op, req_id, "unknown_op");
}

static void ble_poll_async_wifi_sta_apply(void)
{
	provision_run_status_t run;
	char live_ip[48] = { 0 };
	int live_state = g_config.provision.network_state;
	const char *stage;
	time_t now;

	if (!s_async_request.active ||
	    strcmp(s_async_request.op, "wifi_sta_apply") != 0)
		return;

	now = time_now_sec();
	if (now == s_async_probe_time)
		return;
	s_async_probe_time = now;

	ble_compat_sync_config();
	memset(&run, 0, sizeof(run));
	provision_get_run_status(&run);
	if (ble_wifi_is_connected(live_ip, sizeof(live_ip)))
		live_state = NETWORK_STATE_ONLINE;
	stage = run.stage[0] ? run.stage : "connecting";

	if (run.state == PROVISION_RUN_SUCCESS) {
		cJSON *data = cJSON_CreateObject();
		cJSON_AddStringToObject(data, "stage", "done");
		cJSON_AddStringToObject(data, "ip",
					ble_current_ip(&run, live_state, live_ip));
		cJSON_AddNumberToObject(data, "http_port",
					g_config.port_info.http_port_no > 0 ?
						g_config.port_info.http_port_no :
						NET_DEFAULT_HTTP_PORT);
		ble_send_response("wifi_sta_apply", s_async_request.req_id,
				  EC_OK, "Provisioned", data);
		ble_clear_async_request();
		return;
	}

	if (run.state == PROVISION_RUN_FAILED) {
		cJSON *data = cJSON_CreateObject();
		cJSON_AddStringToObject(data, "stage",
					run.stage[0] ? run.stage : "failed");
		ble_send_response("wifi_sta_apply", s_async_request.req_id,
				  run.error_code > 0 ? run.error_code :
						      BLE_PROVISION_STA_ERROR_CODE,
				  run.error_reason[0] ? run.error_reason :
						      "wifi_connect_failed",
				  data);
		ble_clear_async_request();
		return;
	}

	if (run.state == PROVISION_RUN_CONNECTING &&
	    stage[0] &&
	    strcmp(stage, s_async_request.last_stage) != 0) {
		cJSON *data = cJSON_CreateObject();
		snprintf(s_async_request.last_stage,
			 sizeof(s_async_request.last_stage), "%s", stage);
		cJSON_AddStringToObject(data, "stage", stage);
		ble_send_response("wifi_sta_apply", s_async_request.req_id, EC_OK,
				  "OK", data);
	}
}

bool net_ble_poll(void)
{
	char payload[BT_ATT_MAX_VALUE_LEN + 1];
	bool pending = false;
	bool too_large = false;
	bool retry_start = false;
	net_ble_advertisement_t retry_adv;

	pthread_mutex_lock(&s_mutex);
	if (s_start_retry_pending && !s_ble_running &&
	    time_now_sec() >= s_start_retry_time) {
		retry_adv = s_retry_adv;
		ble_clear_start_retry_locked();
		retry_start = true;
	}
	pthread_mutex_unlock(&s_mutex);

	if (retry_start) {
		LOGI("BLE: retrying advertiser start after controller delay");
		(void)net_ble_advertiser_start(&retry_adv);
	}

	ble_poll_advertising_status();
	ble_poll_async_wifi_sta_apply();

	if (!s_pending_command.pending)
		return false;

	pthread_mutex_lock(&s_mutex);
	if (s_pending_command.pending) {
		pending = true;
		too_large = s_pending_command.too_large;
		snprintf(payload, sizeof(payload), "%s", s_pending_command.payload);
		memset(&s_pending_command, 0, sizeof(s_pending_command));
	}
	pthread_mutex_unlock(&s_mutex);

	if (!pending)
		return false;

	ble_process_command_payload(payload, too_large);
	return true;
}

bool net_ble_poll_commands(void)
{
	char payload[BT_ATT_MAX_VALUE_LEN + 1];
	bool pending = false;
	bool too_large = false;
	bool retry_start = false;
	net_ble_advertisement_t retry_adv;

	/*
	 * Check for pending advertising start retry.  This is the same
	 * logic as net_ble_poll() but without ble_poll_advertising_status()
	 * (which triggers HCI queries that starve Wi-Fi on AIC8800DC).
	 */
	pthread_mutex_lock(&s_mutex);
	if (s_start_retry_pending && !s_ble_running &&
	    time_now_sec() >= s_start_retry_time) {
		retry_adv = s_retry_adv;
		ble_clear_start_retry_locked();
		retry_start = true;
	}
	pthread_mutex_unlock(&s_mutex);

	if (retry_start) {
		LOGI("BLE: retrying advertiser start (commands-only path)");
		(void)net_ble_advertiser_start(&retry_adv);
	}

	ble_poll_async_wifi_sta_apply();

	if (!s_pending_command.pending)
		return false;

	pthread_mutex_lock(&s_mutex);
	if (s_pending_command.pending) {
		pending = true;
		too_large = s_pending_command.too_large;
		snprintf(payload, sizeof(payload), "%s",
			 s_pending_command.payload);
		memset(&s_pending_command, 0, sizeof(s_pending_command));
	}
	pthread_mutex_unlock(&s_mutex);

	if (!pending)
		return false;

	ble_process_command_payload(payload, too_large);
	return true;
}

void net_ble_notify_provision_result(bool success, const char *ip)
{
	if (!s_async_request.active ||
	    strcmp(s_async_request.op, "wifi_sta_apply") != 0)
		return;

	if (success) {
		cJSON *data = cJSON_CreateObject();
		cJSON_AddStringToObject(data, "stage", "done");
		cJSON_AddStringToObject(data, "ip", ip ? ip : "");
		cJSON_AddNumberToObject(data, "http_port",
					g_config.port_info.http_port_no > 0 ?
						g_config.port_info.http_port_no :
						NET_DEFAULT_HTTP_PORT);
		ble_send_response("wifi_sta_apply", s_async_request.req_id,
				  EC_OK, "Provisioned", data);
	} else {
		cJSON *data = cJSON_CreateObject();
		cJSON_AddStringToObject(data, "stage", "failed");
		ble_send_response("wifi_sta_apply", s_async_request.req_id,
				  BLE_PROVISION_STA_ERROR_CODE,
				  "wifi_connect_failed", data);
	}

	ble_clear_async_request();
}
