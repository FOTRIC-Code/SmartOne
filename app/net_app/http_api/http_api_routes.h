#ifndef NET_APP_HTTP_API_ROUTES_H
#define NET_APP_HTTP_API_ROUTES_H

/* Shared route path constants — register every route in http_authz.c. */

#define ROUTE_SYSTEM_DEVICE_INFO "system/device_info"
#define ROUTE_SYSTEM_DEVICE_RESTART "system/device_restart"
#define ROUTE_SYSTEM_DEVICE_RESET "system/device_reset"
#define ROUTE_SYSTEM_DEVICE_UPGRADE "system/device_upgrade"
#define ROUTE_SYSTEM_TIME "system/time"
#define ROUTE_SYSTEM_NTP_SERVER "system/ntp_server"
#define ROUTE_SYSTEM_NETWORKS "system/networks"
#define ROUTE_SYSTEM_STATUS "system/status"
#define ROUTE_SYSTEM_PORT "system/port"
#define ROUTE_SYSTEM_FTP "system/ftp"
#define ROUTE_SYSTEM_FTP_TEST "system/ftp/test"
#define ROUTE_SYSTEM_WEBHOOK "system/webhook"
#define ROUTE_DISCOVERY_RECOVER_DHCP "discovery/recover_dhcp"
#define ROUTE_THERMAL_INTRUSION "thermal/intrusion/alarm"
#define ROUTE_THERMAL_HIGH_TEMP "thermal/high_temp/alarm"
#define ROUTE_THERMAL_BODY_TEMP "thermal/body_temp/alarm"
#define ROUTE_THERMAL_LENS_INFO "thermal/lens_info"
#define ROUTE_THERMAL_CALIBRATE "thermal/thermal_calibrate"
#define ROUTE_THERMAL_SHUTTER_MODE "thermal/thermal_shutter_mode"
#define ROUTE_THERMAL_SHUTTER_TIME "thermal/thermal_shutter_time"
#define ROUTE_THERMAL_RANGE "thermal/thermal_range"
#define ROUTE_THERMAL_PARAMS "thermal/thermal_params"
#define ROUTE_THERMAL_UNIT "thermal/thermal_unit"
#define ROUTE_THERMAL_COLOR_DIST "thermal/color_dist"
#define ROUTE_THERMAL_PALETTE "thermal/palette"
#define ROUTE_THERMAL_PRESET_PALETTES "thermal/preset_palettes"
#define ROUTE_THERMAL_GAIN "thermal/gain"
#define ROUTE_IMAGE_CHANNELS "image/channels"
#define ROUTE_ALARM_TRIGGERS "alarm/triggers"
#define ROUTE_STREAMING_CHANNELS "streaming/channels"
#define ROUTE_MEDIA_WEBRTC "media/webrtc"

#endif
