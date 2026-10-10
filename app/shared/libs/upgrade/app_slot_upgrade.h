#ifndef NET_APP_LIBS_UPGRADE_APP_SLOT_UPGRADE_H
#define NET_APP_LIBS_UPGRADE_APP_SLOT_UPGRADE_H

#include <stddef.h>

#define APP_SLOT_ROOT "/userdata/parkia"
#define APP_SLOT_DIR APP_SLOT_ROOT "/app"
#define APP_SLOT_BACKUP_DIR APP_SLOT_ROOT "/backup"
#define APP_SLOT_STAGING_DIR APP_SLOT_ROOT "/staging"

typedef struct {
	char version[64];
	char firmware_name[128];
} app_slot_upgrade_result_t;

/**
 * Apply a multi-component OTA package (gzip/plain tar .img).
 * Package must contain data.json with version, plus at least one of:
 *   main_app, net_app, fab_app, discovery_app,
 *   lib/libIRtekUVCSDK.so, lib/libIRtekFileSDK.so,
 *   lib/libmk_api.so, www/
 * Missing components are left unchanged in APP_SLOT_DIR (merge install).
 *
 * @return 0 on success; -1 on failure (errbuf filled when provided)
 */
int app_slot_upgrade_apply(const char *filepath, app_slot_upgrade_result_t *out,
			   char *errbuf, size_t errbuf_len);

#endif
