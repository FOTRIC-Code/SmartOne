#ifndef NET_APP_NET_COMMON_H
#define NET_APP_NET_COMMON_H

#include "net_enums.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_APP_NAME "net_app"
#define NET_APP_VERSION "1.0.0.0"
#define NET_DEFAULT_DEVICE_NAME "SmartOne"

#define NET_LEN_16 16
#define NET_LEN_32 32
#define NET_LEN_48 48
#define NET_LEN_64 64
#define NET_LEN_128 128
#define NET_MAX_NETWORKS 4
#define NET_MAX_STREAMS 4
#define NET_MAX_ARRAY 4

#define NET_CFG_DIR "/etc/cfg"
#define NET_TMP_DIR "/tmp/net_app"
#define NET_UPGRADE_DIR "/tmp/net_app/system/device_upgrade"
#define NET_APP_SLOT_ROOT "/userdata/parkia"
#define NET_APP_DIR "/userdata/parkia/app"
#define NET_APP_BACKUP_DIR "/userdata/parkia/backup"
#define NET_APP_STAGING_DIR "/userdata/parkia/staging"
#define NET_ALARM_RECORD_DIR "/userdata/parkia/record"
/* Published by /etc/init.d/S15_SD; lives on the SD card itself. */
#define NET_ALARM_RECORD_READY_FLAG NET_ALARM_RECORD_DIR "/.record_ready"
/* Holly-aligned static web root (sibling module app/www → /var/www). */
#define NET_WEB_ROOT "/var/www"

#define NET_DEFAULT_HTTP_PORT 80
#define NET_DEFAULT_RTSP_PORT 554
#define NET_DEFAULT_RTP_PORT 8001

#define ZLM_HTTP_PORT 8080
#define ZLM_RTC_PORT 8555

typedef struct hsi_info_t {
	char name[NET_LEN_128];
	char model[NET_LEN_128];
	char spec[NET_LEN_128];
	char serial_no[NET_LEN_16];
	char id[NET_LEN_32];
	char brand[NET_LEN_32];
	char company[NET_LEN_64];
	char firmware_name[NET_LEN_128];
	char firmware_version[NET_LEN_16];
	char manufacture_date[32];
} hsi_info_t;

typedef struct time_info_t {
	int time_mode; /* net_enum_time_mode_t: 0=LOCAL, 1=NTP */
	char local_time[64];
	char time_zone[32];
} time_info_t;

typedef struct ntp_info_t {
	char host_name[NET_LEN_128];
	int port_no;
	int syn_interval;
} ntp_info_t;

typedef struct network_info_t {
	int id;
	char name[NET_LEN_32];
	char ip[NET_LEN_64];
	char netmask[NET_LEN_64];
	int dhcp;
	char gateway[NET_MAX_ARRAY][NET_LEN_64];
	int gateway_count;
	char dns[NET_MAX_ARRAY][NET_LEN_64];
	int dns_count;
} network_info_t;

typedef struct provision_info_t {
	int provision_state; /* net_enum_provision_state_t */
	int network_state; /* net_enum_network_state_t */
	/*
	 * Desired radio role, orthogonal to provision_state: credentials may be
	 * stored (PROVISIONED) while the user deliberately keeps the device on
	 * its own hotspot. Persisted so the choice survives a reboot.
	 */
	int network_mode; /* net_enum_network_mode_t: 0=AP, 1=STA */
	char hotspot_ssid[64];
	char hotspot_password[128];
	int hotspot_channel; /* 0 = pick automatically */
	char http_host[48];
	int http_port_no;
	char setup_code[32];
	char ssid[64];
	char password[128];
	int last_error_code;
	char last_error_stage[32];
	char last_error_reason[64];
} provision_info_t;

typedef struct port_info_t {
	int http_port_no;
	int rtsp_port_no;
	int rtp_port_no;
} port_info_t;

typedef struct stream_info_t {
	int id;
	char codec_type[16];
	int bit_rate;
	int width;
	int height;
	int max_frame_rate;
	int disable_skip;
	int enable_rc;
	int gop_size;
	int h264_profile;
	int initial_qp;
	int maximum_qp;
	char quality_control_type[16];
	int connection_count;
} stream_info_t;

typedef struct system_status_t {
	int streaming;
	int zlm_ready;
	volatile int exit_flag;
} system_status_t;

typedef struct intrusion_alarm_t {
	int enabled;
	int no_repeat_minutes;
	int sensitivity;
	int record_duration_sec;
} intrusion_alarm_t;

typedef struct high_temp_alarm_t {
	int enabled;
	int no_repeat_minutes;
	float alarm_temp;
	int record_duration_sec;
} high_temp_alarm_t;

typedef struct body_temp_alarm_t {
	int enabled;
	int sensitivity;
} body_temp_alarm_t;

typedef struct alarm_trigger_t {
	int trigger_ftp_enabled;
	int trigger_webhook_enabled;
	int trigger_record_enabled;
} alarm_trigger_t;

#define NET_PARKIA_ALARM_TYPE_FIRST 2
#define NET_PARKIA_ALARM_TYPE_LAST 4
#define NET_PARKIA_ALARM_TYPE_COUNT 3
#define ALARM_EVENT_MESSAGE_MAX 96

typedef struct alarm_event {
	int type;
	uint64_t timestamp_us;
	int motion_level;
	float temperature;
	float threshold;
	float baseline;
	float slope;
	int record_duration_sec;
	/* Planned recording file; empty when record linkage is off. */
	char video_path[NET_LEN_128];
	char message[ALARM_EVENT_MESSAGE_MAX];
} alarm_event_t;

typedef struct ftp_info_t {
	int use_ssl;
	char ip[NET_LEN_64];
	int port_no;
	char user_name[NET_LEN_64];
	char pwd[NET_LEN_128];
	int enable_annoy;
	char path_template[NET_LEN_128];
} ftp_info_t;

typedef struct webhook_info_t {
	char url[256];
} webhook_info_t;

/* Thermal settings; refl_temp / atm_temp are Kelvin on the wire (Volga). */
typedef struct thermal_settings_t {
	int shutter_mode; /* net_enum_shutter_mode_t */
	int shutter_interval_min;
	int range_id;
	float emissivity;
	float distance;
	float refl_temp;
	float atm_temp;
	float rel_humidity; /* 0~1 */
	int temp_unit; /* net_enum_temp_unit_t */
	int distance_unit; /* net_enum_distance_unit_t */
	int color_dist; /* net_enum_color_dist_type_t */
	int palette_ref_no;
	int palette_inverted;
	int palette_color_count; /* non-zero only for NET_CUSTOM_PALETTE */
	int palette_colors[NET_MAX_PALETTE_COLORS];
	int gain_mode; /* net_enum_gain_mode_t */
	float gain_max; /* Kelvin */
	float gain_min; /* Kelvin */
	int gain_fixed_span;
	float gain_span; /* Kelvin delta */
} thermal_settings_t;

typedef struct image_settings_t {
	int flip_mode; /* net_enum_image_flip_mode_t */
	int rotation; /* clockwise degrees: 0/90/180/270 (Parkia extension) */
	int contrast; /* 0~100 */
	int brightness; /* 0~100 */
	int dde_enable;
	int dde_value; /* 0~100 */
} image_settings_t;

typedef struct net_app_config_t {
	hsi_info_t hsi_info;
	time_info_t time_info;
	ntp_info_t ntp_server;
	port_info_t port_info;
	network_info_t networks[NET_MAX_NETWORKS];
	int network_count;
	provision_info_t provision;
	stream_info_t streams[NET_MAX_STREAMS];
	int stream_count;
	intrusion_alarm_t intrusion_alarm;
	high_temp_alarm_t high_temp_alarm;
	body_temp_alarm_t body_temp_alarm;
	alarm_trigger_t alarm_triggers[NET_PARKIA_ALARM_TYPE_COUNT];
	ftp_info_t ftp_info;
	webhook_info_t webhook;
	thermal_settings_t thermal;
	image_settings_t image;
} net_app_config_t;

static inline int net_alarm_type_index(int type)
{
	return type >= NET_PARKIA_ALARM_TYPE_FIRST &&
		       type <= NET_PARKIA_ALARM_TYPE_LAST ?
		       type - NET_PARKIA_ALARM_TYPE_FIRST :
		       -1;
}

extern net_app_config_t g_config;
extern system_status_t g_system_status;

/* Boot lifecycle (config_manager.c). main.c must not include config_manager.h. */
int config_init(void);
void config_uninit(void);

void net_common_set_runtime_defaults(void);
void net_common_build_default_hotspot_ssid(char *dst, size_t size,
					   const char *serial_no);
void net_common_set_default_hotspot_ssid(provision_info_t *provision,
					 const char *serial_no);

/*
 * True while S15_SD reports the record volume usable. Without a mounted
 * card the flag cannot exist, so the underlying eMMC directory is never
 * mistaken for storage.
 */
int net_record_dir_ready(void);

#endif
