#include "thermal_data_provider.h"
#include "thermal_uvc_bridge.h"

#include "video_process/image_convert.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int thermal_provider_start(void)
{
	return thermal_uvc_provider_start();
}

void thermal_provider_stop(void)
{
	thermal_uvc_provider_stop();
}

int thermal_get_global_max_temp(float *temp)
{
	return thermal_uvc_provider_get_marker_max_temp(NULL, temp);
}

int thermal_copy_latest_raw(uint16_t *pixels, size_t capacity, size_t *count,
			    int *width, int *height, unsigned int *frame_no)
{
	return thermal_uvc_provider_copy_latest_raw(pixels, capacity, count,
						    width, height, frame_no);
}

int thermal_get_status(thermal_provider_status_t *status)
{
	return thermal_uvc_provider_get_status(status);
}

int thermal_provider_copy_preview_bgr(thermal_preview_frame_t *frame)
{
	return thermal_uvc_provider_copy_preview_bgr(frame);
}

const char *thermal_preview_err_str(thermal_preview_err_t err)
{
	switch (err) {
	case THERMAL_PREVIEW_OK:
		return "ok";
	case THERMAL_PREVIEW_ERR_ARG:
		return "bad_arg";
	case THERMAL_PREVIEW_ERR_NO_RAW:
		return "no_raw";
	case THERMAL_PREVIEW_ERR_BUF:
		return "buf_small";
	case THERMAL_PREVIEW_ERR_RENDER:
		return "render_size";
	}
	return "unknown";
}

int thermal_provider_get_native_size(int *width, int *height)
{
	return thermal_uvc_provider_get_native_size(width, height);
}

int thermal_provider_get_frame_stats(thermal_frame_stats_t *stats)
{
	return thermal_uvc_provider_get_frame_stats(stats);
}

int thermal_provider_capture_jpeg(thermal_snapshot_t *snapshot)
{
	thermal_preview_frame_t preview = { 0 };
	unsigned char *bgr = NULL;
	size_t pixel_size;

	if (!snapshot)
		return -1;
	memset(snapshot, 0, sizeof(*snapshot));
	if (thermal_provider_copy_preview_bgr(&preview) != 0 ||
	    preview.width <= 0 || preview.height <= 0 ||
	    (size_t)preview.width > SIZE_MAX / (size_t)preview.height / 3U)
		return -1;

	pixel_size = (size_t)preview.width * (size_t)preview.height * 3U;
	bgr = malloc(pixel_size);
	if (!bgr)
		return -1;
	preview.data = bgr;
	preview.capacity = pixel_size;
	if (thermal_provider_copy_preview_bgr(&preview) != 0)
		goto fail;
	if (image_bgr_to_jpeg_mem(bgr, preview.width, preview.height, 85,
				  &snapshot->data, &snapshot->size) != 0)
		goto fail;

	free(bgr);
	snapshot->frame_no = preview.frame_no;
	return 0;

fail:
	free(bgr);
	return -1;
}

void thermal_provider_release_snapshot(thermal_snapshot_t *snapshot)
{
	if (!snapshot)
		return;
	free(snapshot->data);
	memset(snapshot, 0, sizeof(*snapshot));
}

int thermal_provider_apply_thermal_settings(void)
{
	return thermal_uvc_provider_apply_thermal_settings();
}

int thermal_provider_apply_image_settings(void)
{
	return thermal_uvc_provider_apply_image_settings();
}

int thermal_provider_calibrate(void)
{
	return thermal_uvc_provider_calibrate();
}

int thermal_provider_get_lens_info(thermal_lens_info_t *info)
{
	return thermal_uvc_provider_get_lens_info(info);
}

int thermal_provider_get_preset_palettes(thermal_preset_palette_t *out,
					 int max_count, int *out_count)
{
	return thermal_uvc_provider_get_preset_palettes(out, max_count,
							out_count);
}

int thermal_provider_get_preset_palette_colors(int ref_no, int *colors,
					       int max_colors, int *color_count)
{
	return thermal_uvc_provider_get_preset_palette_colors(ref_no, colors,
							      max_colors,
							      color_count);
}

int thermal_provider_get_live_gain(float *min_k, float *max_k)
{
	return thermal_uvc_provider_get_live_gain(min_k, max_k);
}

int thermal_provider_get_live_palette(int *colors, int max_colors,
				      int *color_count)
{
	return thermal_uvc_provider_get_live_palette(colors, max_colors,
						     color_count);
}
