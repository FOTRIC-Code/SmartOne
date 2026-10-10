#include "http_authz.h"

#include "http_api_routes.h"

#include <stddef.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

typedef struct {
	const char *path;
	const char *method; /* NULL = any method on exact/prefix path */
	int min_level;
} route_acl_t;

/*
 * Level 0 — open status.
 * Level 1 — normal API; unprovisioned devices skip auth (SoftAP bring-up).
 * Unlisted routes → HTTP_AUTHZ_UNREGISTERED (fail-closed).
 */
static const route_acl_t s_route_acl[] = {
	/* open */
	/* Discovery DHCP recover — localhost PUT from discovery_service */
	{ ROUTE_DISCOVERY_RECOVER_DHCP, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_STATUS, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_DEVICE_INFO, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_DEVICE_INFO, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_DEVICE_RESTART, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_DEVICE_RESET, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_DEVICE_UPGRADE, "POST", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_TIME, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_TIME, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_NTP_SERVER, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_NTP_SERVER, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_NETWORKS, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_NETWORKS, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_NETWORKS "/", NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_PORT, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_PORT, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_FTP, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_FTP_TEST, "POST", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_SYSTEM_WEBHOOK, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_INTRUSION, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_HIGH_TEMP, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_BODY_TEMP, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_LENS_INFO, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_CALIBRATE, "PUT", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_SHUTTER_MODE, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_SHUTTER_TIME, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_RANGE, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_PARAMS, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_UNIT, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_COLOR_DIST, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_PALETTE, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_PRESET_PALETTES, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_THERMAL_GAIN, NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_IMAGE_CHANNELS "/", NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_ALARM_TRIGGERS "/", NULL, HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_STREAMING_CHANNELS, "GET", HTTP_AUTHZ_LEVEL_OPEN },
	{ ROUTE_STREAMING_CHANNELS "/", NULL, HTTP_AUTHZ_LEVEL_OPEN },
	/* Open like ZLM :8080 — SoftAP preview before / with login */
	{ ROUTE_MEDIA_WEBRTC, "POST", HTTP_AUTHZ_LEVEL_OPEN },
};

static int method_matches(const char *acl_method, const char *method)
{
	if (!acl_method)
		return 1;
	if (!method)
		return 0;
	return strcmp(acl_method, method) == 0;
}

int http_authz_required_level(const char *path, const char *method)
{
	size_t i;
	int best_level = HTTP_AUTHZ_UNREGISTERED;
	int best_prefix_len = -1;

	if (!path || !path[0])
		return HTTP_AUTHZ_UNREGISTERED;
	if (path[0] == '/')
		path++;

	for (i = 0; i < ARRAY_SIZE(s_route_acl); i++) {
		const route_acl_t *r = &s_route_acl[i];
		size_t plen = strlen(r->path);

		if (!method_matches(r->method, method))
			continue;
		if (plen > 0 && r->path[plen - 1] == '/')
			continue;
		if (strcmp(r->path, path) == 0)
			return r->min_level;
	}

	for (i = 0; i < ARRAY_SIZE(s_route_acl); i++) {
		const route_acl_t *r = &s_route_acl[i];
		size_t plen = strlen(r->path);

		if (!method_matches(r->method, method))
			continue;
		if (plen == 0 || r->path[plen - 1] != '/')
			continue;
		if (strncmp(path, r->path, plen) != 0)
			continue;
		if ((int)plen > best_prefix_len) {
			best_prefix_len = (int)plen;
			best_level = r->min_level;
		}
	}

	return best_level;
}
