#ifndef NET_APP_HTTP_API_COMMON_H
#define NET_APP_HTTP_API_COMMON_H

#include "config/config_manager.h"
#include "http_dto.h"
#include "libs/json/json_bind.h"
#include "libs/json/json_util.h"
#include "net_common/net_common.h"

#include <string.h>

typedef enum {
	EC_OK = 0,
	EC_FAIL = -1,
	EC_MEMORY = -2,
	EC_NULL_POINTER = -3,
	EC_INVALID_PARAM = -4,
	EC_NOT_SUPPORTED = -5,
	EC_UNAUTHORIZED = -401,
	EC_FORBIDDEN = -403
} error_code_t;

char *create_response(int code, const char *message, json_t data);
char *create_error(int code, const char *message);

int parse_body_to_dto(const char *data, int len, void *dto,
		      const cfg_field_t *fields, int fc);

/*
 * GET: stor -> dto -> JSON via dto_fields (Holly-aligned layering).
 */
#define HANDLE_GET(stor_type, dto_type, getter, to_dto_fn, fields, fc)       \
	do {                                                                   \
		stor_type *_s = getter();                                      \
		dto_type _d;                                                   \
		if (!_s)                                                       \
			return create_error(EC_FAIL, "Config unavailable");  \
		memset(&_d, 0, sizeof(_d));                                    \
		to_dto_fn(_s, &_d);                                            \
		return create_response(EC_OK, "OK",                            \
				       cfg_to_json(&_d, fields, fc));         \
	} while (0)

/*
 * PUT (Cube merge): seed dto from current stor, merge body keys, persist.
 * Unlike Holly HANDLE_PUT (zero dto then full replace), missing JSON keys
 * keep the current values so partial updates remain valid.
 */
#define HANDLE_PUT_BODY(dto_type, stor_type, getter, to_dto_fn, from_dto_fn, \
			setter, fields, fc, body, len, msg)                    \
	do {                                                                   \
		dto_type _d;                                                   \
		stor_type _s;                                                  \
		stor_type *_cur = getter();                                    \
		if (!_cur)                                                     \
			return create_error(EC_FAIL, "Config unavailable");  \
		to_dto_fn(_cur, &_d);                                          \
		if (parse_body_to_dto(body, len, &_d, fields, fc) != 0)         \
			return create_error(EC_INVALID_PARAM,                  \
					    "Invalid request body");           \
		_s = *_cur;                                                    \
		from_dto_fn(&_d, &_s);                                         \
		if (setter(&_s) != 0)                                          \
			return create_error(EC_FAIL, "Failed to save");        \
		return create_response(EC_OK, msg, NULL);                      \
	} while (0)

#define HANDLE_PUT_VALID_BODY(dto_type, stor_type, getter, to_dto_fn,        \
			      from_dto_fn, setter, fields, fc, body, len,    \
			      validator, msg)                                  \
	do {                                                                   \
		dto_type _d;                                                   \
		stor_type _s;                                                  \
		stor_type *_cur = getter();                                    \
		if (!_cur)                                                     \
			return create_error(EC_FAIL, "Config unavailable");  \
		to_dto_fn(_cur, &_d);                                          \
		if (parse_body_to_dto(body, len, &_d, fields, fc) != 0)         \
			return create_error(EC_INVALID_PARAM,                  \
					    "Invalid request body");           \
		if (!(validator)(&_d))                                         \
			return create_error(EC_INVALID_PARAM,                  \
					    "Invalid request body");           \
		_s = *_cur;                                                    \
		from_dto_fn(&_d, &_s);                                         \
		if (setter(&_s) != 0)                                          \
			return create_error(EC_FAIL, "Failed to save");        \
		return create_response(EC_OK, msg, NULL);                      \
	} while (0)

/* System */
char *handle_system_device_info_get(void);
char *handle_system_device_info_put(const char *body, int len);
char *handle_system_restart(void);
char *handle_system_reset(void);
char *handle_system_upgrade(const char *filepath);
char *handle_system_time_get(void);
char *handle_system_time_put(const char *body, int len);
char *handle_system_ntp_server_get(void);
char *handle_system_ntp_server_put(const char *body, int len);
char *handle_system_networks_get(void);
char *handle_system_network_get(int id);
char *handle_system_network_put(int id, const char *body, int len);
char *handle_system_status_get(void);
char *handle_system_port_get(void);
char *handle_system_port_put(const char *body, int len);
char *handle_system_networks_put(const char *body, int len);
char *handle_system_ftp_get(void);
char *handle_system_ftp_put(const char *body, int len);
char *handle_system_ftp_test(const char *body, int len);
char *handle_system_webhook_get(void);
char *handle_system_webhook_put(const char *body, int len);

/* Discovery — anonymous localhost PUT from discovery_service */
char *handle_discovery_recover_dhcp(void);

/* Alarm */
char *handle_intrusion_alarm_get(void);
char *handle_intrusion_alarm_put(const char *body, int len);
char *handle_high_temp_alarm_get(void);
char *handle_high_temp_alarm_put(const char *body, int len);
char *handle_body_temp_alarm_get(void);
char *handle_body_temp_alarm_put(const char *body, int len);
char *handle_alarm_trigger_get(int type);
char *handle_alarm_trigger_put(int type, const char *body, int len);

/* Streaming */
char *handle_streaming_channels_get(void);
char *handle_streaming_channel_get(int id);
char *handle_streaming_channel_status_get(int id);

/* Thermal / image */
char *handle_thermal_shutter_mode_get(void);
char *handle_thermal_shutter_mode_put(const char *body, int len);
char *handle_thermal_shutter_time_get(void);
char *handle_thermal_shutter_time_put(const char *body, int len);
char *handle_thermal_calibrate_put(void);
char *handle_thermal_range_get(void);
char *handle_thermal_range_put(const char *body, int len);
char *handle_thermal_params_get(void);
char *handle_thermal_params_put(const char *body, int len);
char *handle_thermal_unit_get(void);
char *handle_thermal_unit_put(const char *body, int len);
char *handle_thermal_lens_info_get(void);
char *handle_thermal_color_dist_get(void);
char *handle_thermal_color_dist_put(const char *body, int len);
char *handle_thermal_palette_get(void);
char *handle_thermal_palette_put(const char *body, int len);
char *handle_thermal_palettes_get(void);
char *handle_thermal_gain_get(void);
char *handle_thermal_gain_put(const char *body, int len);
char *handle_image_flip_get(int id);
char *handle_image_flip_put(int id, const char *body, int len);
char *handle_image_params_get(int id);
char *handle_image_params_put(int id, const char *body, int len);

#endif
