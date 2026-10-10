#ifndef NET_APP_NET_ENUMS_H
#define NET_APP_NET_ENUMS_H

typedef enum net_enum_time_mode {
	NET_TIME_MODE_LOCAL = 0,
	NET_TIME_MODE_NTP = 1
} net_enum_time_mode_t;

typedef enum net_enum_task_type {
	NET_TASK_NONE = 0,
	NET_TASK_NTP_SYNC = 1 << 0,
	NET_TASK_DEVICE_RESTART = 1 << 1,
	NET_TASK_ALARM_FTP_UPLOAD = 1 << 2,
	NET_TASK_ALARM_WEBHOOK = 1 << 3,
	NET_TASK_ALARM_RECORD = 1 << 4,
	NET_TASK_HTTP_REBIND = 1 << 5
} net_enum_task_type_t;

typedef enum net_enum_alarm_sensitivity {
	NET_ALARM_SENSITIVITY_LOW = 0,
	NET_ALARM_SENSITIVITY_MID = 1,
	NET_ALARM_SENSITIVITY_HIGH = 2
} net_enum_alarm_sensitivity_t;

typedef enum net_enum_alarm_type {
	NET_ALARM_THERMAL_TEMP = 1,
	NET_ALARM_INTRUSION = 2,
	NET_ALARM_HIGH_TEMP = 3,
	NET_ALARM_BODY_TEMP = 4
} net_enum_alarm_type_t;

/* SoftAP vs station radio role (orthogonal to provision_state). */
typedef enum net_enum_network_mode {
	NET_NETWORK_MODE_AP = 0,
	NET_NETWORK_MODE_STA = 1
} net_enum_network_mode_t;

typedef enum net_enum_provision_state {
	NET_PROVISION_STATE_AP_READY = 0,
	NET_PROVISION_STATE_PROVISIONING = 1,
	NET_PROVISION_STATE_PROVISIONED = 2,
	NET_PROVISION_STATE_FAILED = 3
} net_enum_provision_state_t;

typedef enum net_enum_network_state {
	NET_NETWORK_STATE_OFFLINE = 0,
	NET_NETWORK_STATE_ONLINE = 1
} net_enum_network_state_t;

typedef enum net_enum_wifi_band {
	NET_WIFI_BAND_2_4G = 0,
	NET_WIFI_BAND_5G = 1
} net_enum_wifi_band_t;

/* Wi-Fi security enum values (shared with main_app). */
typedef enum net_enum_wifi_security {
	NET_WIFI_SEC_OPEN = 0,
	NET_WIFI_SEC_WPA2_PSK = 1,
	NET_WIFI_SEC_WPA3_SAE = 2,
	NET_WIFI_SEC_WPA2_WPA3_MIXED = 3
} net_enum_wifi_security_t;

/* Thermal source behind net_app; drives www capability gating. */
typedef enum net_enum_thermal_source {
	NET_THERMAL_SOURCE_UVC = 0,
	NET_THERMAL_SOURCE_LANTERN = 1
} net_enum_thermal_source_t;

typedef enum net_enum_shutter_mode {
	NET_SHUTTER_AUTOMATIC = 0,
	NET_SHUTTER_MANUAL = 1
} net_enum_shutter_mode_t;

typedef enum net_enum_image_flip_mode {
	NET_IMAGE_FLIP_NONE = 0,
	NET_IMAGE_FLIP_UP_DOWN = 1,
	NET_IMAGE_FLIP_LEFT_RIGHT = 2,
	NET_IMAGE_FLIP_CENTER = 3
} net_enum_image_flip_mode_t;

typedef enum net_enum_temp_unit {
	NET_TEMP_UNIT_CELSIUS = 0,
	NET_TEMP_UNIT_KELVIN = 1,
	NET_TEMP_UNIT_FAHRENHEIT = 2
} net_enum_temp_unit_t;

typedef enum net_enum_distance_unit {
	NET_DISTANCE_UNIT_METER = 0,
	NET_DISTANCE_UNIT_FEET = 1
} net_enum_distance_unit_t;

/* Color distribution. Values match IRtek FileSDK ir_file_enum_color_dist_type. */
typedef enum net_enum_color_dist_type {
	NET_THERMAL_T_TWB = 1,
	NET_THERMAL_TEMP_LINEAR = 2,
	NET_THERMAL_AD_LINEAR = 3
} net_enum_color_dist_type_t;

/* Temperature span. Values match IRtek FileSDK ir_file_enum_gain_mode. */
typedef enum net_enum_gain_mode {
	NET_THERMAL_FIXED = 1,
	NET_THERMAL_AUTO = 2
} net_enum_gain_mode_t;

#define NET_MAX_PALETTE_COLORS 256
#define NET_CUSTOM_PALETTE 0xFF

#endif
