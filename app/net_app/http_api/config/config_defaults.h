#ifndef NET_APP_CONFIG_DEFAULTS_H
#define NET_APP_CONFIG_DEFAULTS_H

#include "config_types.h"
#include "net_common/net_common.h"

#define DEFAULT_TIME_INFO { \
	.time_mode = NET_TIME_MODE_LOCAL, \
	.local_time = "", \
	.time_zone = "UTC+08:00" \
}

#define DEFAULT_NTP_INFO { \
	.host_name = "pool.ntp.org", \
	.port_no = 123, \
	.syn_interval = 1440 \
}

#define DEFAULT_PORT_INFO { \
	.http_port_no = NET_DEFAULT_HTTP_PORT, \
	.rtsp_port_no = NET_DEFAULT_RTSP_PORT, \
	.rtp_port_no = NET_DEFAULT_RTP_PORT \
}

#define DEFAULT_SEAL_PARAM { \
	.sealed = 0 \
}

#define DEFAULT_INTRUSION_ALARM { \
	.enabled = 0, \
	.no_repeat_minutes = 5, \
	.sensitivity = NET_ALARM_SENSITIVITY_MID, \
	.record_duration_sec = 30 \
}

#define DEFAULT_HIGH_TEMP_ALARM { \
	.enabled = 0, \
	.no_repeat_minutes = 3, \
	.alarm_temp = 38.0f, \
	.record_duration_sec = 30 \
}

#define DEFAULT_BODY_TEMP_ALARM { \
	.enabled = 0, \
	.sensitivity = NET_ALARM_SENSITIVITY_MID \
}

#define DEFAULT_ALARM_TRIGGER { \
	.trigger_ftp_enabled = 0, \
	.trigger_webhook_enabled = 0, \
	.trigger_record_enabled = 1 \
}

#define DEFAULT_FTP_INFO { \
	.use_ssl = 0, \
	.ip = "", \
	.port_no = 21, \
	.user_name = "", \
	.pwd = "", \
	.enable_annoy = 0, \
	.path_template = "Parkia/{yyyyMMddHHmmss}.jpg" \
}

#define DEFAULT_WEBHOOK_INFO { \
	.url = "" \
}

/* Volga defaults: refl/atm temp on the wire are Kelvin (293.15 = 20°C).
 * Range ids are 1-based on the UVC module; default to the first range. */
#define DEFAULT_THERMAL_SETTINGS { \
	.shutter_mode = NET_SHUTTER_AUTOMATIC, \
	.shutter_interval_min = 10, \
	.range_id = 1, \
	.emissivity = 0.95f, \
	.distance = 1.0f, \
	.refl_temp = 293.15f, \
	.atm_temp = 293.15f, \
	.rel_humidity = 0.5f, \
	.temp_unit = NET_TEMP_UNIT_CELSIUS, \
	.distance_unit = NET_DISTANCE_UNIT_METER, \
	.color_dist = NET_THERMAL_AD_LINEAR, \
	.palette_ref_no = 1, \
	.palette_inverted = 0, \
	.palette_color_count = 0, \
	.gain_mode = NET_THERMAL_AUTO, \
	.gain_max = 373.15f, \
	.gain_min = 273.15f, \
	.gain_fixed_span = 0, \
	.gain_span = 2.0f \
}

#define DEFAULT_IMAGE_SETTINGS { \
	.flip_mode = NET_IMAGE_FLIP_NONE, \
	.rotation = 0, \
	.contrast = 50, \
	.brightness = 50, \
	.dde_enable = 0, \
	.dde_value = 50 \
}

#define DEFAULT_LOG_CONFIG { \
	.enabled = 1, \
	.level = 0, \
	.max_file_size = 32768, \
	.file_path = "/tmp/net_app.log" \
}

#define DEFAULT_RECIPE_INFO { \
	.company = "", \
	.brand = "", \
	.language = "" \
}

#define DEFAULT_NETWORK_ITEM { \
	.id = 1, \
	.name = "wlan0", \
	.ip = "", \
	.netmask = "255.255.255.0", \
	.dhcp = 1, \
	.gateway = { "" }, \
	.gateway_count = 1, \
	.dns = { "" }, \
	.dns_count = 1 \
}

#define DEFAULT_PROVISION_INFO { \
	.provision_state = NET_PROVISION_STATE_AP_READY, \
	.network_state = NET_NETWORK_STATE_OFFLINE, \
	.network_mode = NET_NETWORK_MODE_AP, \
	.hotspot_ssid = "", \
	.hotspot_password = "", \
	.hotspot_channel = 0, \
	.http_host = "192.168.4.1", \
	.http_port_no = NET_DEFAULT_HTTP_PORT, \
	.setup_code = "123456", \
	.ssid = "", \
	.password = "", \
	.last_error_code = 0, \
	.last_error_stage = "idle", \
	.last_error_reason = "none" \
}

#endif
