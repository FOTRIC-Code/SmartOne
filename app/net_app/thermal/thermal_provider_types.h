#ifndef NET_APP_THERMAL_PROVIDER_TYPES_H
#define NET_APP_THERMAL_PROVIDER_TYPES_H

#include <stddef.h>

#define THERMAL_PROVIDER_STATUS_MESSAGE_LEN 128

typedef enum thermal_preview_err {
	THERMAL_PREVIEW_OK = 0,
	THERMAL_PREVIEW_ERR_ARG = 1,
	THERMAL_PREVIEW_ERR_NO_RAW = 2,
	THERMAL_PREVIEW_ERR_BUF = 3,
	THERMAL_PREVIEW_ERR_RENDER = 4,
} thermal_preview_err_t;

typedef struct thermal_preview_frame_t {
	unsigned char *data;
	size_t capacity;
	int width;
	int height;
	unsigned int frame_no;
	thermal_preview_err_t err;
} thermal_preview_frame_t;

/*
 * UVC frame accounting. Every drop in the stream callback is silent, so these
 * counters are the only way to tell "callback never fired" from
 * "frames arrive but are rejected".
 */
typedef struct thermal_frame_stats_t {
	unsigned int callbacks;
	unsigned int drop_empty;
	unsigned int drop_parse;
	unsigned int drop_shutter;
	unsigned int accepted;
	unsigned int last_length;
} thermal_frame_stats_t;

typedef struct thermal_snapshot_t {
	unsigned char *data;
	size_t size;
	unsigned int frame_no;
} thermal_snapshot_t;

typedef struct thermal_provider_status_t {
	int connected;
	int streaming;
	int file_sdk_ready;
	int has_frame;
	unsigned short width;
	unsigned short height;
	unsigned int frame_no;
	float center_temp_celsius;
	float max_temp_celsius;
	char last_error[THERMAL_PROVIDER_STATUS_MESSAGE_LEN];
} thermal_provider_status_t;

#define THERMAL_LENS_MAX_RANGES 4

typedef struct thermal_lens_range_t {
	int id;
	float min;
	float max;
} thermal_lens_range_t;

/* Wire shape of GET /thermal/lens_info (Volga thermal_lens_info). */
typedef struct thermal_lens_info_t {
	char name[64];
	float focal;
	float vfov;
	float hfov;
	char band[32];
	thermal_lens_range_t range_map[THERMAL_LENS_MAX_RANGES];
	int ranges_count;
} thermal_lens_info_t;

#define THERMAL_PRESET_PALETTE_NAME_LEN 32

typedef struct thermal_preset_palette_t {
	int ref_no;
	char name[THERMAL_PRESET_PALETTE_NAME_LEN];
} thermal_preset_palette_t;

#endif
