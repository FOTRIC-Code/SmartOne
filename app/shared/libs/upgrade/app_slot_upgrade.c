#include "app_slot_upgrade.h"

#include "libs/file/file_util.h"
#include "libs/json/json_util.h"
#include "libs/log/log.h"
#include "libs/platform/command.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void set_err(char *errbuf, size_t errbuf_len, const char *msg)
{
	if (!errbuf || errbuf_len == 0)
		return;
	snprintf(errbuf, errbuf_len, "%s", msg ? msg : "Upgrade failed");
}

static const char *read_app_meta_version(const char *meta_path)
{
	static char ver_buf[64];
	char *buf = NULL;
	size_t len = 0;
	char *line;

	ver_buf[0] = '\0';
	if (file_read(meta_path, &buf, &len) != 0 || !buf)
		return NULL;

	line = strstr(buf, "version=");
	if (line) {
		char *val = line + 8;
		char *nl = strchr(val, '\n');

		if (nl)
			*nl = '\0';
		snprintf(ver_buf, sizeof(ver_buf), "%s", val);
	}
	free(buf);
	return ver_buf[0] ? ver_buf : NULL;
}

static int app_dir_valid(const char *dir)
{
	char meta[256];
	char bin[256];

	if (!dir || !dir[0])
		return 0;
	snprintf(meta, sizeof(meta), "%s/app_meta", dir);
	snprintf(bin, sizeof(bin), "%s/bin/net_app", dir);
	if (!file_exists(meta) || !read_app_meta_version(meta))
		return 0;
	return file_exists(bin) ? 1 : 0;
}

static int backup_active_app(void)
{
	char cmd[512];

	if (!app_dir_valid(APP_SLOT_DIR))
		return 0;

	file_remove_tree_contents(APP_SLOT_BACKUP_DIR);
	create_dir_recursive(APP_SLOT_BACKUP_DIR);
	snprintf(cmd, sizeof(cmd), "cp -a '%s/.' '%s/'", APP_SLOT_DIR,
		 APP_SLOT_BACKUP_DIR);
	if (net_run_command(cmd) != 0) {
		LOGE("Failed to backup active app to %s", APP_SLOT_BACKUP_DIR);
		return -1;
	}
	LOGI("Backed up active app to %s", APP_SLOT_BACKUP_DIR);
	return 0;
}

static int restore_app_from_backup(void)
{
	char cmd[512];

	if (!app_dir_valid(APP_SLOT_BACKUP_DIR)) {
		LOGW("No valid backup at %s", APP_SLOT_BACKUP_DIR);
		return -1;
	}
	file_remove_tree_contents(APP_SLOT_DIR);
	create_dir_recursive(APP_SLOT_DIR);
	snprintf(cmd, sizeof(cmd), "cp -a '%s/.' '%s/'", APP_SLOT_BACKUP_DIR,
		 APP_SLOT_DIR);
	if (net_run_command(cmd) != 0) {
		LOGE("Failed to restore app from backup");
		return -1;
	}
	LOGI("Restored active app from %s", APP_SLOT_BACKUP_DIR);
	return 0;
}

static int install_binary(const char *src, const char *dst_name)
{
	char dst_bin[256];
	char dst_path[288];
	char cmd[640];

	if (!file_exists(src))
		return 0;

	snprintf(dst_bin, sizeof(dst_bin), "%s/bin", APP_SLOT_DIR);
	snprintf(dst_path, sizeof(dst_path), "%s/bin/%s", APP_SLOT_DIR,
		 dst_name);
	if (create_dir_recursive(dst_bin) != 0)
		return -1;
	unlink(dst_path);
	if (rename(src, dst_path) != 0) {
		snprintf(cmd, sizeof(cmd), "cp -a '%s' '%s'", src, dst_path);
		if (net_run_command(cmd) != 0)
			return -1;
		unlink(src);
	}
	file_chmod(dst_path, 0755);
	LOGI("Installed %s -> %s", dst_name, dst_path);
	return 0;
}

static int install_shared_library(const char *src, const char *name)
{
	char dst_dir[256];
	char dst_path[320];
	char cmd[640];

	if (!file_exists(src))
		return 0;
	snprintf(dst_dir, sizeof(dst_dir), "%s/lib", APP_SLOT_DIR);
	snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, name);
	if (create_dir_recursive(dst_dir) != 0)
		return -1;
	unlink(dst_path);
	if (rename(src, dst_path) != 0) {
		snprintf(cmd, sizeof(cmd), "cp -a '%s' '%s'", src, dst_path);
		if (net_run_command(cmd) != 0)
			return -1;
		unlink(src);
	}
	file_chmod(dst_path, 0755);
	LOGI("Installed %s -> %s", name, dst_path);
	return 0;
}

static int install_www(const char *www_src)
{
	char www_dst[256];
	char cmd[640];

	if (!file_exists(www_src))
		return 0;

	snprintf(www_dst, sizeof(www_dst), "%s/www", APP_SLOT_DIR);
	create_dir_recursive(www_dst);
	file_remove_tree_contents(www_dst);
	snprintf(cmd, sizeof(cmd), "cp -a '%s/.' '%s/'", www_src, www_dst);
	if (net_run_command(cmd) != 0)
		return -1;
	LOGI("Installed www -> %s", www_dst);
	return 0;
}

int app_slot_upgrade_apply(const char *filepath, app_slot_upgrade_result_t *out,
			   char *errbuf, size_t errbuf_len)
{
	json_t data_json = NULL;
	int staging_dirty = 0;
	int app_dirty = 0;
	int ret;
	char data_path[192];
	char src_main[192];
	char src_net[192];
	char src_fab[192];
	char src_disc[192];
	char src_irtek_uvc_sdk[256];
	char src_irtek_file_sdk[256];
	char src_zlm[256];
	char src_www[192];
	char meta_path[192];
	char meta_buf[96];
	int have_any = 0;
	app_slot_upgrade_result_t local;

	memset(&local, 0, sizeof(local));
	if (out)
		memset(out, 0, sizeof(*out));

	if (!filepath || !filepath[0]) {
		set_err(errbuf, errbuf_len, "Missing upgrade file");
		return -1;
	}

	LOGI("Slot upgrade requested, file: %s", filepath);
	create_dir_recursive(APP_SLOT_ROOT);
	create_dir_recursive(APP_SLOT_DIR);

	if (backup_active_app() != 0) {
		set_err(errbuf, errbuf_len, "Failed to backup current app");
		goto fail;
	}

	if (file_remove_tree_contents(APP_SLOT_STAGING_DIR) != 0) {
		set_err(errbuf, errbuf_len, "Failed to prepare staging dir");
		goto fail;
	}
	staging_dirty = 1;

	sync();
	{
		int drop_fd = open("/proc/sys/vm/drop_caches", O_WRONLY);

		if (drop_fd >= 0) {
			(void)write(drop_fd, "3\n", 2);
			close(drop_fd);
		}
	}

	LOGI("Extracting upgrade file: %s (%ld bytes) -> %s", filepath,
	     file_size(filepath), APP_SLOT_STAGING_DIR);
	ret = file_extract_compressed_tar(filepath, APP_SLOT_STAGING_DIR);
	if (ret != 0) {
		set_err(errbuf, errbuf_len, "Failed to extract upgrade file");
		goto fail;
	}
	unlink(filepath);
	staging_dirty = 0;

	snprintf(data_path, sizeof(data_path), "%s/data.json",
		 APP_SLOT_STAGING_DIR);
	snprintf(src_main, sizeof(src_main), "%s/main_app",
		 APP_SLOT_STAGING_DIR);
	snprintf(src_net, sizeof(src_net), "%s/net_app", APP_SLOT_STAGING_DIR);
	snprintf(src_fab, sizeof(src_fab), "%s/fab_app", APP_SLOT_STAGING_DIR);
	snprintf(src_disc, sizeof(src_disc), "%s/discovery_app",
		 APP_SLOT_STAGING_DIR);
	snprintf(src_irtek_uvc_sdk, sizeof(src_irtek_uvc_sdk),
		 "%s/lib/libIRtekUVCSDK.so",
		 APP_SLOT_STAGING_DIR);
	snprintf(src_irtek_file_sdk, sizeof(src_irtek_file_sdk),
		 "%s/lib/libIRtekFileSDK.so",
		 APP_SLOT_STAGING_DIR);
	snprintf(src_zlm, sizeof(src_zlm), "%s/lib/libmk_api.so",
		 APP_SLOT_STAGING_DIR);
	snprintf(src_www, sizeof(src_www), "%s/www", APP_SLOT_STAGING_DIR);

	if (file_exists(src_main))
		have_any = 1;
	if (file_exists(src_net))
		have_any = 1;
	if (file_exists(src_fab))
		have_any = 1;
	if (file_exists(src_disc))
		have_any = 1;
	if (file_exists(src_irtek_uvc_sdk))
		have_any = 1;
	if (file_exists(src_irtek_file_sdk))
		have_any = 1;
	if (file_exists(src_zlm))
		have_any = 1;
	if (file_exists(src_www))
		have_any = 1;
	if (!have_any) {
		set_err(errbuf, errbuf_len,
			"Upgrade package has no supported app components");
		goto fail;
	}

	if (json_load_file(data_path, &data_json) != 0 || !data_json) {
		set_err(errbuf, errbuf_len, "Invalid data.json in upgrade package");
		goto fail;
	}
	{
		const char *jv = json_get_string(data_json, "version", NULL);
		const char *jn = json_get_string(data_json, "firmware_name", NULL);

		if (!jv || !jv[0]) {
			set_err(errbuf, errbuf_len,
				"Missing version in upgrade package");
			goto fail;
		}
		snprintf(local.version, sizeof(local.version), "%s", jv);
		if (jn && jn[0])
			snprintf(local.firmware_name, sizeof(local.firmware_name),
				 "%s", jn);
	}

	app_dirty = 1;

	if (install_binary(src_main, "main_app") != 0) {
		set_err(errbuf, errbuf_len, "Failed to install main_app");
		goto fail;
	}
	if (install_binary(src_net, "net_app") != 0) {
		set_err(errbuf, errbuf_len, "Failed to install net_app");
		goto fail;
	}
	if (install_binary(src_fab, "fab_app") != 0) {
		set_err(errbuf, errbuf_len, "Failed to install fab_app");
		goto fail;
	}
	if (install_binary(src_disc, "discovery_app") != 0) {
		set_err(errbuf, errbuf_len, "Failed to install discovery_app");
		goto fail;
	}
	if (install_shared_library(src_irtek_uvc_sdk,
				   "libIRtekUVCSDK.so") != 0) {
		set_err(errbuf, errbuf_len, "Failed to install libIRtekUVCSDK.so");
		goto fail;
	}
	if (install_shared_library(src_irtek_file_sdk,
				   "libIRtekFileSDK.so") != 0) {
		set_err(errbuf, errbuf_len,
			"Failed to install libIRtekFileSDK.so");
		goto fail;
	}
	if (install_shared_library(src_zlm, "libmk_api.so") != 0) {
		set_err(errbuf, errbuf_len, "Failed to install libmk_api.so");
		goto fail;
	}
	if (install_www(src_www) != 0) {
		set_err(errbuf, errbuf_len, "Failed to install www");
		goto fail;
	}

	/* main_app is the boot entry point and launches net_app. */
	if (!file_exists(APP_SLOT_DIR "/bin/main_app") ||
	    !file_exists(APP_SLOT_DIR "/bin/net_app") ||
	    !file_exists(APP_SLOT_DIR "/lib/libIRtekUVCSDK.so") ||
	    !file_exists(APP_SLOT_DIR "/lib/libIRtekFileSDK.so") ||
	    !file_exists(APP_SLOT_DIR "/lib/libmk_api.so")) {
		set_err(errbuf, errbuf_len,
			"Slot missing required runtime components after upgrade");
		goto fail;
	}

	file_remove_tree(APP_SLOT_STAGING_DIR);

	snprintf(meta_path, sizeof(meta_path), "%s/app_meta", APP_SLOT_DIR);
	snprintf(meta_buf, sizeof(meta_buf), "version=%s\n", local.version);
	if (file_write(meta_path, meta_buf) != 0) {
		set_err(errbuf, errbuf_len, "Failed to update app metadata");
		goto fail;
	}
	app_dirty = 0;
	sync();

	json_free(data_json);
	if (out)
		*out = local;
	LOGI("Slot upgrade installed to %s, version=%s", APP_SLOT_DIR,
	     local.version);
	return 0;

fail:
	json_free(data_json);
	if (staging_dirty)
		file_remove_tree_contents(APP_SLOT_STAGING_DIR);
	if (app_dirty) {
		if (restore_app_from_backup() != 0)
			LOGE("Upgrade failed and backup restore also failed");
		sync();
	}
	if (filepath && filepath[0])
		unlink(filepath);
	return -1;
}
