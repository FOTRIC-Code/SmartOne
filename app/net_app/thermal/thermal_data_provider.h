#ifndef NET_APP_THERMAL_DATA_PROVIDER_H
#define NET_APP_THERMAL_DATA_PROVIDER_H

#include "thermal_provider_types.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Abstract interface for thermal sensor data.
 */

int thermal_provider_start(void);
void thermal_provider_stop(void);
int thermal_get_global_max_temp(float *temp);
/*
 * Copy the latest accepted raw AD frame. Shutter frames are not stored.
 * *count is 0 when no frame is available, otherwise the pixel count required.
 * Returns 0 after copying. Returns -1 when there is no frame, or when pixels
 * is NULL / capacity is smaller than *count (grow the buffer and retry).
 */
int thermal_copy_latest_raw(uint16_t *pixels, size_t capacity, size_t *count,
			    int *width, int *height, unsigned int *frame_no);
int thermal_get_status(thermal_provider_status_t *status);
int thermal_provider_copy_preview_bgr(thermal_preview_frame_t *frame);
const char *thermal_preview_err_str(thermal_preview_err_t err);
int thermal_provider_get_native_size(int *width, int *height);
int thermal_provider_get_frame_stats(thermal_frame_stats_t *stats);
int thermal_provider_capture_jpeg(thermal_snapshot_t *snapshot);
void thermal_provider_release_snapshot(thermal_snapshot_t *snapshot);

/*
 * Push g_config.thermal / g_config.image onto the current UVC session and the
 * render path. Best-effort: no-op when no device is connected; the session
 * applies config again at open.
 */
int thermal_provider_apply_thermal_settings(void);
int thermal_provider_apply_image_settings(void);
/* Immediate shutter calibrate; fails when no device is connected. */
int thermal_provider_calibrate(void);
/* Live lens info from the connected device; fails when offline. */
int thermal_provider_get_lens_info(thermal_lens_info_t *info);
/* FileSDK preset list. Returns -1 when the SDK cannot provide one. */
int thermal_provider_get_preset_palettes(thermal_preset_palette_t *out,
					 int max_count, int *out_count);
/* FileSDK preset color bar for ref_no. Returns -1 when the SDK cannot provide it. */
int thermal_provider_get_preset_palette_colors(int ref_no, int *colors,
					       int max_colors, int *color_count);
/* Live AUTO span from the FileSDK session, Kelvin. */
int thermal_provider_get_live_gain(float *min_k, float *max_k);
/* Live palette RGB table from the FileSDK session. */
int thermal_provider_get_live_palette(int *colors, int max_colors,
				      int *color_count);

#endif
