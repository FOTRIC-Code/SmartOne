#include "http_api_common.h"

#include "thermal/thermal_data_provider.h"

#include <stdio.h>
#include <string.h>

/* Image channel is fixed to the single thermal stream (101). */
#define IMAGE_CHANNEL_ID 101

char *handle_thermal_shutter_mode_get(void)
{
	HANDLE_GET(thermal_settings_cfg, shutter_mode_dto,
		   config_get_thermal_settings,
		   thermal_settings_to_shutter_mode_dto, shutter_mode_dto_fields,
		   shutter_mode_dto_fc);
}

char *handle_thermal_shutter_mode_put(const char *body, int len)
{
	shutter_mode_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	thermal_settings_to_shutter_mode_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, shutter_mode_dto_fields,
			      shutter_mode_dto_fc) != 0 ||
	    !shutter_mode_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	updated = *cur;
	thermal_settings_from_shutter_mode_dto(&dto, &updated);
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_thermal_shutter_time_get(void)
{
	HANDLE_GET(thermal_settings_cfg, shutter_time_dto,
		   config_get_thermal_settings,
		   thermal_settings_to_shutter_time_dto, shutter_time_dto_fields,
		   shutter_time_dto_fc);
}

char *handle_thermal_shutter_time_put(const char *body, int len)
{
	shutter_time_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	thermal_settings_to_shutter_time_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, shutter_time_dto_fields,
			      shutter_time_dto_fc) != 0 ||
	    !shutter_time_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	updated = *cur;
	thermal_settings_from_shutter_time_dto(&dto, &updated);
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_thermal_calibrate_put(void)
{
	if (thermal_provider_calibrate() != 0)
		return create_error(EC_FAIL, "Thermal device not connected");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_thermal_range_get(void)
{
	HANDLE_GET(thermal_settings_cfg, thermal_range_dto,
		   config_get_thermal_settings, thermal_settings_to_range_dto,
		   thermal_range_dto_fields, thermal_range_dto_fc);
}

char *handle_thermal_range_put(const char *body, int len)
{
	thermal_range_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	thermal_settings_to_range_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, thermal_range_dto_fields,
			      thermal_range_dto_fc) != 0 ||
	    !thermal_range_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	updated = *cur;
	thermal_settings_from_range_dto(&dto, &updated);
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_thermal_params_get(void)
{
	HANDLE_GET(thermal_settings_cfg, thermal_params_dto,
		   config_get_thermal_settings, thermal_settings_to_params_dto,
		   thermal_params_dto_fields, thermal_params_dto_fc);
}

char *handle_thermal_params_put(const char *body, int len)
{
	thermal_params_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	thermal_settings_to_params_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, thermal_params_dto_fields,
			      thermal_params_dto_fc) != 0 ||
	    !thermal_params_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	updated = *cur;
	thermal_settings_from_params_dto(&dto, &updated);
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_thermal_unit_get(void)
{
	HANDLE_GET(thermal_settings_cfg, thermal_unit_dto,
		   config_get_thermal_settings, thermal_settings_to_unit_dto,
		   thermal_unit_dto_fields, thermal_unit_dto_fc);
}

char *handle_thermal_unit_put(const char *body, int len)
{
	thermal_unit_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	thermal_settings_to_unit_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, thermal_unit_dto_fields,
			      thermal_unit_dto_fc) != 0 ||
	    !thermal_unit_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	updated = *cur;
	thermal_settings_from_unit_dto(&dto, &updated);
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_thermal_lens_info_get(void)
{
	thermal_lens_info_t info;

	if (thermal_provider_get_lens_info(&info) != 0)
		return create_error(EC_FAIL, "Thermal device not connected");
	return create_response(EC_OK, "OK", lens_info_to_json(&info));
}

char *handle_thermal_color_dist_get(void)
{
	HANDLE_GET(thermal_settings_cfg, color_dist_dto,
		   config_get_thermal_settings,
		   thermal_settings_to_color_dist_dto, color_dist_dto_fields,
		   color_dist_dto_fc);
}

char *handle_thermal_color_dist_put(const char *body, int len)
{
	color_dist_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	memset(&dto, 0, sizeof(dto));
	if (parse_body_to_dto(body, len, &dto, color_dist_dto_fields,
			      color_dist_dto_fc) != 0)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	if (!color_dist_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid enum value");
	updated = *cur;
	thermal_settings_from_color_dist_dto(&dto, &updated);
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "Color distribution updated", NULL);
}

static void reverse_palette_colors(int *colors, int count)
{
	int i;

	for (i = 0; i < count / 2; i++) {
		int tmp = colors[i];

		colors[i] = colors[count - 1 - i];
		colors[count - 1 - i] = tmp;
	}
}

char *handle_thermal_palette_get(void)
{
	palette_dto dto;
	thermal_settings_cfg *cur = config_get_thermal_settings();
	int color_count = 0;

	if (!cur)
		return create_error(EC_FAIL, "Config unavailable");
	memset(&dto, 0, sizeof(dto));
	thermal_settings_to_palette_dto(cur, &dto);
	if (cur->palette_ref_no != NET_CUSTOM_PALETTE &&
	    thermal_provider_get_preset_palette_colors(cur->palette_ref_no,
						       dto.colors,
						       NET_MAX_PALETTE_COLORS,
						       &color_count) == 0 &&
	    color_count > 0) {
		dto.color_count = color_count;
		if (dto.inverted)
			reverse_palette_colors(dto.colors, dto.color_count);
	}
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, palette_dto_fields,
					   palette_dto_fc));
}

char *handle_thermal_palette_put(const char *body, int len)
{
	palette_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	memset(&dto, 0, sizeof(dto));
	if (parse_body_to_dto(body, len, &dto, palette_dto_fields,
			      palette_dto_fc) != 0)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	if (!palette_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid enum value");
	updated = *cur;
	thermal_settings_from_palette_dto(&dto, &updated);
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "Palette updated", NULL);
}

char *handle_thermal_palettes_get(void)
{
	thermal_preset_palette_t live[32];
	int live_count = 0;
	json_t arr = json_create_array();
	int i;

	if (!arr)
		return create_error(EC_MEMORY, "Out of memory");

	if (thermal_provider_get_preset_palettes(live, 32, &live_count) == 0 &&
	    live_count > 0) {
		for (i = 0; i < live_count; i++) {
			palette_list_item_dto item;

			memset(&item, 0, sizeof(item));
			item.ref_no = live[i].ref_no;
			snprintf(item.name, sizeof(item.name), "%s",
				 live[i].name);
			json_array_add_item(arr,
					    cfg_to_json(&item,
							palette_list_item_dto_fields,
							palette_list_item_dto_fc));
		}
		return create_response(EC_OK, "OK", arr);
	}

	return create_response(EC_OK, "OK", arr);
}

char *handle_thermal_gain_get(void)
{
	gain_mode_dto dto;
	thermal_settings_cfg *cur = config_get_thermal_settings();
	float live_min = 0.0f;
	float live_max = 0.0f;

	if (!cur)
		return create_error(EC_FAIL, "Config unavailable");
	memset(&dto, 0, sizeof(dto));
	thermal_settings_to_gain_dto(cur, &dto);
	if (dto.mode == NET_THERMAL_AUTO &&
	    thermal_provider_get_live_gain(&live_min, &live_max) == 0)
		gain_mode_dto_set_live_kelvin(&dto, live_min, live_max);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, gain_mode_dto_fields,
					   gain_mode_dto_fc));
}

char *handle_thermal_gain_put(const char *body, int len)
{
	gain_mode_dto dto;
	thermal_settings_cfg updated;
	thermal_settings_cfg *cur = config_get_thermal_settings();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	memset(&dto, 0, sizeof(dto));
	if (parse_body_to_dto(body, len, &dto, gain_mode_dto_fields,
			      gain_mode_dto_fc) != 0)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	if (!gain_mode_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid enum value");
	updated = *cur;
	thermal_settings_from_gain_dto(&dto, &updated);
	/*
	 * FIXED + fixedSpan: endpoints = (max+min)/2 ± span/2, in Kelvin
	 * after the wire unit conversion. Runtime only consumes min/max.
	 */
	if (updated.gain_mode == NET_THERMAL_FIXED && updated.gain_fixed_span &&
	    updated.gain_span > 0.0f) {
		float mid = (updated.gain_max + updated.gain_min) * 0.5f;
		float half = updated.gain_span * 0.5f;

		updated.gain_max = mid + half;
		updated.gain_min = mid - half;
	}
	if (config_set_thermal_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "Gain mode updated", NULL);
}

static int image_channel_ok(int id)
{
	return id == IMAGE_CHANNEL_ID;
}

char *handle_image_flip_get(int id)
{
	if (!image_channel_ok(id))
		return create_error(EC_INVALID_PARAM, "Invalid channel id");
	HANDLE_GET(image_settings_cfg, image_flip_dto, config_get_image_settings,
		   image_settings_to_flip_dto, image_flip_dto_fields,
		   image_flip_dto_fc);
}

char *handle_image_flip_put(int id, const char *body, int len)
{
	image_flip_dto dto;
	image_settings_cfg updated;
	image_settings_cfg *cur = config_get_image_settings();

	if (!image_channel_ok(id))
		return create_error(EC_INVALID_PARAM, "Invalid channel id");
	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	image_settings_to_flip_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, image_flip_dto_fields,
			      image_flip_dto_fc) != 0 ||
	    !image_flip_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	updated = *cur;
	image_settings_from_flip_dto(&dto, &updated);
	if (config_set_image_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_image_params_get(int id)
{
	if (!image_channel_ok(id))
		return create_error(EC_INVALID_PARAM, "Invalid channel id");
	HANDLE_GET(image_settings_cfg, image_params_dto,
		   config_get_image_settings, image_settings_to_params_dto,
		   image_params_dto_fields, image_params_dto_fc);
}

char *handle_image_params_put(int id, const char *body, int len)
{
	image_params_dto dto;
	image_settings_cfg updated;
	image_settings_cfg *cur = config_get_image_settings();

	if (!image_channel_ok(id))
		return create_error(EC_INVALID_PARAM, "Invalid channel id");
	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	image_settings_to_params_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, image_params_dto_fields,
			      image_params_dto_fc) != 0 ||
	    !image_params_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	updated = *cur;
	image_settings_from_params_dto(&dto, &updated);
	if (config_set_image_settings(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}
