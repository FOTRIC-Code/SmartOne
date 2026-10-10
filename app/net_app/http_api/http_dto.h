#ifndef NET_APP_HTTP_DTO_H
#define NET_APP_HTTP_DTO_H

#include "config/config_types.h"
#include "libs/json/json_bind.h"
#include "libs/json/json_util.h"
#include "thermal/thermal_provider_types.h"

/* ==================== DTO structures (wire layer) ==================== */

typedef struct {
	char name[NET_LEN_128];
	char model[NET_LEN_128];
	char spec[NET_LEN_128];
	char serial_no[NET_LEN_16];
	char id[NET_LEN_32];
	char brand[NET_LEN_32];
	char company[NET_LEN_64];
	char firmware_name[NET_LEN_128];
	char firmware_version[NET_LEN_16];
} device_info_dto;

typedef struct {
	int time_mode;
	char local_time[NET_LEN_64];
	char time_zone[NET_LEN_32];
} time_info_dto;

typedef struct {
	char host_name[NET_LEN_128];
	int port_no;
	int syn_interval;
} ntp_info_dto;

typedef struct {
	int id;
	char name[NET_LEN_32];
	int dhcp;
	char ip[NET_LEN_64];
	char netmask[NET_LEN_64];
	char dns[NET_MAX_ARRAY][NET_LEN_64];
	int dns_count;
	char gateway[NET_MAX_ARRAY][NET_LEN_64];
	int gateway_count;
} network_info_dto;

typedef port_info_cfg port_info_dto;

typedef struct {
	int enabled;
	int no_repeat_minutes;
	int sensitivity;
	int record_duration_sec;
} intrusion_alarm_dto;

typedef struct {
	int enabled;
	int no_repeat_minutes;
	float alarm_temp;
	int record_duration_sec;
} high_temp_alarm_dto;

typedef struct {
	int enabled;
	int sensitivity;
} body_temp_alarm_dto;

typedef struct {
	int type;
	int trigger_ftp_enabled;
	int trigger_webhook_enabled;
	int trigger_record_enabled;
} alarm_trigger_dto;

typedef ftp_info_cfg ftp_info_dto;
typedef webhook_info_cfg webhook_info_dto;

typedef struct {
	int id;
	int bit_rate;
	char codec_type[16];
	int disable_skip;
	int enable_rc;
	int gop_size;
	int h264_profile;
	int initial_qp;
	int max_frame_rate;
	int maximum_qp;
	char quality_control_type[16];
	int resolution_width;
	int resolution_height;
} stream_info_dto;

typedef struct {
	int id;
	int connection_count;
} stream_status_dto;

typedef struct {
	int streaming;
	int provision_state;
	int network_state;
	int network_mode;
	int thermal_source;
	/* Hotspot SSID in AP mode, saved STA SSID otherwise (even offline). */
	char ssid[64];
	char ip[48];
} system_status_dto;

/* Thermal / image settings (Volga wire shapes; rotation is a Parkia extension) */

typedef struct {
	int mode;
} shutter_mode_dto;

typedef struct {
	int interval;
} shutter_time_dto;

typedef struct {
	int range_id;
} thermal_range_dto;

typedef struct {
	float emissivity;
	float distance;
	float refl_temp;
	float atm_temp;
	float rel_humidity;
} thermal_params_dto;

typedef struct {
	int temp_unit;
	int distance_unit;
} thermal_unit_dto;

typedef struct {
	int contrast;
	int brightness;
	int dde_enable;
	int dde_value;
} image_params_dto;

typedef struct {
	int mode;
	int rotation;
} image_flip_dto;

typedef struct {
	int type;
} color_dist_dto;

typedef struct {
	int ref_no;
	int colors[NET_MAX_PALETTE_COLORS];
	int color_count;
	int inverted;
} palette_dto;

typedef struct {
	int ref_no;
	char name[32];
} palette_list_item_dto;

typedef struct {
	int mode;
	float max;
	float min;
	int fixedSpan;
	float span;
} gain_mode_dto;

/* ==================== Field descriptors ==================== */

extern const cfg_field_t device_info_dto_fields[];
extern const int device_info_dto_fc;

extern const cfg_field_t time_info_dto_fields[];
extern const int time_info_dto_fc;

extern const cfg_field_t ntp_info_dto_fields[];
extern const int ntp_info_dto_fc;

extern const cfg_field_t network_info_dto_fields[];
extern const int network_info_dto_fc;

extern const cfg_field_t port_info_dto_fields[];
extern const int port_info_dto_fc;

extern const cfg_field_t intrusion_alarm_dto_fields[];
extern const int intrusion_alarm_dto_fc;
extern const cfg_field_t high_temp_alarm_dto_fields[];
extern const int high_temp_alarm_dto_fc;
extern const cfg_field_t body_temp_alarm_dto_fields[];
extern const int body_temp_alarm_dto_fc;

extern const cfg_field_t alarm_trigger_dto_fields[];
extern const int alarm_trigger_dto_fc;
extern const cfg_field_t ftp_info_dto_fields[];
extern const int ftp_info_dto_fc;
extern const cfg_field_t webhook_info_dto_fields[];
extern const int webhook_info_dto_fc;

extern const cfg_field_t stream_info_dto_fields[];
extern const int stream_info_dto_fc;

extern const cfg_field_t stream_status_dto_fields[];
extern const int stream_status_dto_fc;

extern const cfg_field_t system_status_dto_fields[];
extern const int system_status_dto_fc;

extern const cfg_field_t shutter_mode_dto_fields[];
extern const int shutter_mode_dto_fc;
extern const cfg_field_t shutter_time_dto_fields[];
extern const int shutter_time_dto_fc;
extern const cfg_field_t thermal_range_dto_fields[];
extern const int thermal_range_dto_fc;
extern const cfg_field_t thermal_params_dto_fields[];
extern const int thermal_params_dto_fc;
extern const cfg_field_t thermal_unit_dto_fields[];
extern const int thermal_unit_dto_fc;
extern const cfg_field_t image_params_dto_fields[];
extern const int image_params_dto_fc;
extern const cfg_field_t image_flip_dto_fields[];
extern const int image_flip_dto_fc;
extern const cfg_field_t color_dist_dto_fields[];
extern const int color_dist_dto_fc;
extern const cfg_field_t palette_dto_fields[];
extern const int palette_dto_fc;
extern const cfg_field_t palette_list_item_dto_fields[];
extern const int palette_list_item_dto_fc;
extern const cfg_field_t gain_mode_dto_fields[];
extern const int gain_mode_dto_fc;

/* ==================== cfg <-> dto ==================== */

void device_info_to_dto(const hsi_info_cfg *s, device_info_dto *d);
void device_info_from_hsi_info_to_dto(const hsi_info_cfg *s,
				     const recipe_info_cfg *r,
				     device_info_dto *d);
void time_info_to_dto(const time_info_cfg *s, time_info_dto *d);
void time_info_from_dto(const time_info_dto *d, time_info_cfg *s);
void ntp_info_to_dto(const ntp_info_cfg *s, ntp_info_dto *d);
void ntp_info_from_dto(const ntp_info_dto *d, ntp_info_cfg *s);
void network_info_to_dto(const network_info_cfg *s, network_info_dto *d);
void network_info_from_dto(const network_info_dto *d, network_info_cfg *s);
void port_info_to_dto(const port_info_cfg *s, port_info_dto *d);
void port_info_from_dto(const port_info_dto *d, port_info_cfg *s);

void intrusion_alarm_to_dto(const intrusion_alarm_cfg *s,
			    intrusion_alarm_dto *d);
void intrusion_alarm_from_dto(const intrusion_alarm_dto *d,
			      intrusion_alarm_cfg *s);
void high_temp_alarm_to_dto(const high_temp_alarm_cfg *s,
			    high_temp_alarm_dto *d);
void high_temp_alarm_from_dto(const high_temp_alarm_dto *d,
			      high_temp_alarm_cfg *s);
void body_temp_alarm_to_dto(const body_temp_alarm_cfg *s,
			    body_temp_alarm_dto *d);
void body_temp_alarm_from_dto(const body_temp_alarm_dto *d,
			      body_temp_alarm_cfg *s);

void alarm_trigger_to_dto(const alarm_trigger_cfg *s, int type,
			  alarm_trigger_dto *d);
void alarm_trigger_from_dto(const alarm_trigger_dto *d, alarm_trigger_cfg *s);
void ftp_info_to_dto(const ftp_info_cfg *s, ftp_info_dto *d);
void ftp_info_from_dto(const ftp_info_dto *d, ftp_info_cfg *s);
void webhook_info_to_dto(const webhook_info_cfg *s, webhook_info_dto *d);
void webhook_info_from_dto(const webhook_info_dto *d, webhook_info_cfg *s);

void stream_info_to_dto(const stream_info_cfg *s, stream_info_dto *d);
void stream_status_to_dto(const stream_info_cfg *s, stream_status_dto *d);
void system_status_to_dto(system_status_dto *d);

void thermal_settings_to_shutter_mode_dto(const thermal_settings_cfg *s,
					  shutter_mode_dto *d);
void thermal_settings_from_shutter_mode_dto(const shutter_mode_dto *d,
					    thermal_settings_cfg *s);
void thermal_settings_to_shutter_time_dto(const thermal_settings_cfg *s,
					  shutter_time_dto *d);
void thermal_settings_from_shutter_time_dto(const shutter_time_dto *d,
					    thermal_settings_cfg *s);
void thermal_settings_to_range_dto(const thermal_settings_cfg *s,
				   thermal_range_dto *d);
void thermal_settings_from_range_dto(const thermal_range_dto *d,
				     thermal_settings_cfg *s);
void thermal_settings_to_params_dto(const thermal_settings_cfg *s,
				    thermal_params_dto *d);
void thermal_settings_from_params_dto(const thermal_params_dto *d,
				      thermal_settings_cfg *s);
void thermal_settings_to_unit_dto(const thermal_settings_cfg *s,
				  thermal_unit_dto *d);
void thermal_settings_from_unit_dto(const thermal_unit_dto *d,
				    thermal_settings_cfg *s);
void image_settings_to_params_dto(const image_settings_cfg *s,
				  image_params_dto *d);
void image_settings_from_params_dto(const image_params_dto *d,
				    image_settings_cfg *s);
void image_settings_to_flip_dto(const image_settings_cfg *s,
				image_flip_dto *d);
void image_settings_from_flip_dto(const image_flip_dto *d,
				  image_settings_cfg *s);
void thermal_settings_to_color_dist_dto(const thermal_settings_cfg *s,
					color_dist_dto *d);
void thermal_settings_from_color_dist_dto(const color_dist_dto *d,
					  thermal_settings_cfg *s);
void thermal_settings_to_palette_dto(const thermal_settings_cfg *s,
				     palette_dto *d);
void thermal_settings_from_palette_dto(const palette_dto *d,
				       thermal_settings_cfg *s);
void thermal_settings_to_gain_dto(const thermal_settings_cfg *s,
				  gain_mode_dto *d);
void thermal_settings_from_gain_dto(const gain_mode_dto *d,
				    thermal_settings_cfg *s);
/* Overlay a live Kelvin span onto a gain DTO already converted to temp_unit. */
void gain_mode_dto_set_live_kelvin(gain_mode_dto *d, float min_k, float max_k);
json_t lens_info_to_json(const thermal_lens_info_t *info);

/* ==================== validators ==================== */

int str_contains_windows_folder_illegal_chars(const char *s);
int time_info_dto_is_valid(const time_info_dto *dto);
int ntp_info_dto_is_valid(const ntp_info_dto *dto);
int network_info_dto_is_valid(const network_info_dto *dto);
int port_info_dto_is_valid(const port_info_dto *dto);
int intrusion_alarm_dto_is_valid(const intrusion_alarm_dto *dto);
int high_temp_alarm_dto_is_valid(const high_temp_alarm_dto *dto);
int body_temp_alarm_dto_is_valid(const body_temp_alarm_dto *dto);
int alarm_trigger_dto_is_valid(const alarm_trigger_dto *dto, int path_type);
int ftp_info_dto_is_valid(const ftp_info_dto *dto);
int webhook_info_dto_is_valid(const webhook_info_dto *dto);
int shutter_mode_dto_is_valid(const shutter_mode_dto *dto);
int shutter_time_dto_is_valid(const shutter_time_dto *dto);
int thermal_range_dto_is_valid(const thermal_range_dto *dto);
int thermal_params_dto_is_valid(const thermal_params_dto *dto);
int thermal_unit_dto_is_valid(const thermal_unit_dto *dto);
int image_params_dto_is_valid(const image_params_dto *dto);
int image_flip_dto_is_valid(const image_flip_dto *dto);
int color_dist_dto_is_valid(const color_dist_dto *dto);
int palette_dto_is_valid(const palette_dto *dto);
int gain_mode_dto_is_valid(const gain_mode_dto *dto);

/* ==================== array helpers ==================== */

json_t networks_to_json(const network_info_cfg *nets, int count);
int networks_from_json(const char *body, int len, network_info_cfg *out_nets,
		       int max_count, int *out_count);
json_t streams_to_json(void);

#endif
