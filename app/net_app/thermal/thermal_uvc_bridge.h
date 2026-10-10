#ifndef NET_APP_THERMAL_UVC_BRIDGE_H
#define NET_APP_THERMAL_UVC_BRIDGE_H

#include "thermal_provider_types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int thermal_uvc_provider_start(void);
void thermal_uvc_provider_stop(void);
int thermal_uvc_provider_get_marker_max_temp(const char *marker_name, float *temp_celsius);
int thermal_uvc_provider_copy_latest_raw(uint16_t *pixels, size_t capacity,
					 size_t *count, int *width, int *height,
					 unsigned int *frame_no);
int thermal_uvc_provider_get_status(thermal_provider_status_t *status);
int thermal_uvc_provider_copy_preview_bgr(thermal_preview_frame_t *frame);
int thermal_uvc_provider_get_native_size(int *width, int *height);
int thermal_uvc_provider_get_frame_stats(thermal_frame_stats_t *stats);
int thermal_uvc_provider_apply_thermal_settings(void);
int thermal_uvc_provider_apply_image_settings(void);
int thermal_uvc_provider_calibrate(void);
int thermal_uvc_provider_get_lens_info(thermal_lens_info_t *info);
int thermal_uvc_provider_get_preset_palettes(thermal_preset_palette_t *out,
					     int max_count, int *out_count);
/* 256 RGB entries (0x00RRGGBB) from ir_file_get_preset_palette_image. */
int thermal_uvc_provider_get_preset_palette_colors(int ref_no, int *colors,
						   int max_colors, int *color_count);
int thermal_uvc_provider_get_live_gain(float *min_k, float *max_k);
int thermal_uvc_provider_get_live_palette(int *colors, int max_colors,
					  int *color_count);

#ifdef __cplusplus
}
#endif

#endif
