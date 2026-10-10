#include "thermal_uvc_bridge.h"

#include <string.h>

int thermal_uvc_provider_start(void)
{
	return -1;
}

void thermal_uvc_provider_stop(void)
{
}

int thermal_uvc_provider_get_marker_max_temp(const char *marker_name,
					     float *temp_celsius)
{
	(void)marker_name;
	(void)temp_celsius;
	return -1;
}

int thermal_uvc_provider_copy_latest_raw(uint16_t *pixels, size_t capacity,
					 size_t *count, int *width, int *height,
					 unsigned int *frame_no)
{
	(void)pixels;
	(void)capacity;
	(void)width;
	(void)height;
	(void)frame_no;
	if (count)
		*count = 0;
	return -1;
}

int thermal_uvc_provider_get_status(thermal_provider_status_t *status)
{
	if (!status)
		return -1;

	memset(status, 0, sizeof(*status));
	strncpy(status->last_error, "IRtek thermal bridge disabled",
		sizeof(status->last_error) - 1);
	return -1;
}

int thermal_uvc_provider_copy_preview_bgr(thermal_preview_frame_t *frame)
{
	if (!frame)
		return -1;
	frame->err = THERMAL_PREVIEW_ERR_NO_RAW;
	return -1;
}

int thermal_uvc_provider_get_native_size(int *width, int *height)
{
	(void)width;
	(void)height;
	return -1;
}

int thermal_uvc_provider_get_frame_stats(thermal_frame_stats_t *stats)
{
	if (!stats)
		return -1;

	memset(stats, 0, sizeof(*stats));
	return -1;
}

int thermal_uvc_provider_apply_thermal_settings(void)
{
	return 0;
}

int thermal_uvc_provider_apply_image_settings(void)
{
	return 0;
}

int thermal_uvc_provider_calibrate(void)
{
	return -1;
}

int thermal_uvc_provider_get_lens_info(thermal_lens_info_t *info)
{
	(void)info;
	return -1;
}

int thermal_uvc_provider_get_preset_palettes(thermal_preset_palette_t *out,
					     int max_count, int *out_count)
{
	(void)out;
	(void)max_count;
	(void)out_count;
	return -1;
}

int thermal_uvc_provider_get_preset_palette_colors(int ref_no, int *colors,
						   int max_colors,
						   int *color_count)
{
	(void)ref_no;
	(void)colors;
	(void)max_colors;
	(void)color_count;
	return -1;
}

int thermal_uvc_provider_get_live_gain(float *min_k, float *max_k)
{
	(void)min_k;
	(void)max_k;
	return -1;
}

int thermal_uvc_provider_get_live_palette(int *colors, int max_colors,
					  int *color_count)
{
	(void)colors;
	(void)max_colors;
	(void)color_count;
	return -1;
}
