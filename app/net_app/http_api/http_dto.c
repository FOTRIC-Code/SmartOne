#include "http_dto.h"

#include "http_api_common.h"
#include "libs/log/log.h"
#include "libs/network/network_manager.h"
#include "libs/unit/unit_convert.h"
#include "net_common/net_enums.h"
#include <arpa/inet.h>
#include <math.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FC_DEF(name) const int name##_fc = CFG_FIELD_COUNT(name##_fields)

#define COPY_STR(dst, src) \
	snprintf((dst), sizeof(dst), "%s", (src) ? (src) : "")

static int record_duration_ok(int sec)
{
	return sec == 30 || sec == 60;
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

/* ==================== Field descriptors ==================== */

const cfg_field_t device_info_dto_fields[] = {
	CFG_STRING(device_info_dto, name, "name"),
	CFG_STRING(device_info_dto, model, "model"),
	CFG_STRING(device_info_dto, spec, "spec"),
	CFG_STRING(device_info_dto, serial_no, "serial_no"),
	CFG_STRING(device_info_dto, id, "id"),
	CFG_STRING(device_info_dto, brand, "brand"),
	CFG_STRING(device_info_dto, company, "company"),
	CFG_STRING(device_info_dto, firmware_name, "firmware_name"),
	CFG_STRING(device_info_dto, firmware_version, "firmware_version"),
};
FC_DEF(device_info_dto);

const cfg_field_t time_info_dto_fields[] = {
	CFG_INT(time_info_dto, time_mode, "time_mode"),
	CFG_STRING(time_info_dto, local_time, "local_time"),
	CFG_STRING(time_info_dto, time_zone, "time_zone"),
};
FC_DEF(time_info_dto);

const cfg_field_t ntp_info_dto_fields[] = {
	CFG_STRING(ntp_info_dto, host_name, "host_name"),
	CFG_INT(ntp_info_dto, port_no, "port_no"),
	CFG_INT(ntp_info_dto, syn_interval, "syn_interval"),
};
FC_DEF(ntp_info_dto);

const cfg_field_t network_info_dto_fields[] = {
	CFG_INT(network_info_dto, id, "id"),
	CFG_STRING(network_info_dto, name, "name"),
	CFG_BOOL(network_info_dto, dhcp, "dhcp"),
	CFG_STRING(network_info_dto, ip, "ip"),
	CFG_STRING(network_info_dto, netmask, "netmask"),
	CFG_STR_ARRAY(network_info_dto, gateway, gateway_count, "gateway",
		      NET_MAX_ARRAY),
	CFG_STR_ARRAY(network_info_dto, dns, dns_count, "dns", NET_MAX_ARRAY),
};
FC_DEF(network_info_dto);

const cfg_field_t port_info_dto_fields[] = {
	CFG_INT(port_info_dto, http_port_no, "http_port_no"),
	CFG_INT(port_info_dto, rtsp_port_no, "rtsp_port_no"),
	CFG_INT(port_info_dto, rtp_port_no, "rtp_port_no"),
};
FC_DEF(port_info_dto);

const cfg_field_t intrusion_alarm_dto_fields[] = {
	CFG_BOOL(intrusion_alarm_dto, enabled, "enabled"),
	CFG_INT(intrusion_alarm_dto, no_repeat_minutes, "no_repeat_minutes"),
	CFG_INT(intrusion_alarm_dto, sensitivity, "sensitivity"),
	CFG_INT(intrusion_alarm_dto, record_duration_sec, "record_duration_sec"),
};
FC_DEF(intrusion_alarm_dto);

const cfg_field_t high_temp_alarm_dto_fields[] = {
	CFG_BOOL(high_temp_alarm_dto, enabled, "enabled"),
	CFG_INT(high_temp_alarm_dto, no_repeat_minutes, "no_repeat_minutes"),
	CFG_FLOAT(high_temp_alarm_dto, alarm_temp, "alarm_temp"),
	CFG_INT(high_temp_alarm_dto, record_duration_sec, "record_duration_sec"),
};
FC_DEF(high_temp_alarm_dto);

const cfg_field_t body_temp_alarm_dto_fields[] = {
	CFG_BOOL(body_temp_alarm_dto, enabled, "enabled"),
	CFG_INT(body_temp_alarm_dto, sensitivity, "sensitivity"),
};
FC_DEF(body_temp_alarm_dto);

const cfg_field_t alarm_trigger_dto_fields[] = {
	CFG_INT(alarm_trigger_dto, type, "type"),
	CFG_BOOL(alarm_trigger_dto, trigger_ftp_enabled, "trigger_ftp_enabled"),
	CFG_BOOL(alarm_trigger_dto, trigger_webhook_enabled,
		 "trigger_webhook_enabled"),
	CFG_BOOL(alarm_trigger_dto, trigger_record_enabled,
		 "trigger_record_enabled"),
};
FC_DEF(alarm_trigger_dto);

const cfg_field_t ftp_info_dto_fields[] = {
	CFG_BOOL(ftp_info_dto, use_ssl, "use_ssl"),
	CFG_STRING(ftp_info_dto, ip, "ip"),
	CFG_INT(ftp_info_dto, port_no, "port_no"),
	CFG_STRING(ftp_info_dto, user_name, "user_name"),
	CFG_STRING(ftp_info_dto, pwd, "pwd"),
	CFG_BOOL(ftp_info_dto, enable_annoy, "enable_annoy"),
	CFG_STRING(ftp_info_dto, path_template, "path_template"),
};
FC_DEF(ftp_info_dto);

const cfg_field_t webhook_info_dto_fields[] = {
	CFG_STRING(webhook_info_dto, url, "url"),
};
FC_DEF(webhook_info_dto);

const cfg_field_t shutter_mode_dto_fields[] = {
	CFG_INT(shutter_mode_dto, mode, "mode"),
};
FC_DEF(shutter_mode_dto);

const cfg_field_t shutter_time_dto_fields[] = {
	CFG_INT(shutter_time_dto, interval, "interval"),
};
FC_DEF(shutter_time_dto);

const cfg_field_t thermal_range_dto_fields[] = {
	CFG_INT(thermal_range_dto, range_id, "range_id"),
};
FC_DEF(thermal_range_dto);

const cfg_field_t thermal_params_dto_fields[] = {
	CFG_FLOAT(thermal_params_dto, emissivity, "emissivity"),
	CFG_FLOAT(thermal_params_dto, distance, "distance"),
	CFG_FLOAT(thermal_params_dto, refl_temp, "refl_temp"),
	CFG_FLOAT(thermal_params_dto, atm_temp, "atm_temp"),
	CFG_FLOAT(thermal_params_dto, rel_humidity, "rel_humidity"),
};
FC_DEF(thermal_params_dto);

const cfg_field_t thermal_unit_dto_fields[] = {
	CFG_INT(thermal_unit_dto, temp_unit, "temp_unit"),
	CFG_INT(thermal_unit_dto, distance_unit, "distance_unit"),
};
FC_DEF(thermal_unit_dto);

const cfg_field_t image_params_dto_fields[] = {
	CFG_INT(image_params_dto, contrast, "contrast"),
	CFG_INT(image_params_dto, brightness, "brightness"),
	CFG_BOOL(image_params_dto, dde_enable, "dde_enable"),
	CFG_INT(image_params_dto, dde_value, "dde_value"),
};
FC_DEF(image_params_dto);

const cfg_field_t image_flip_dto_fields[] = {
	CFG_INT(image_flip_dto, mode, "mode"),
	CFG_INT(image_flip_dto, rotation, "rotation"),
};
FC_DEF(image_flip_dto);

const cfg_field_t color_dist_dto_fields[] = {
	CFG_INT(color_dist_dto, type, "type"),
};
FC_DEF(color_dist_dto);

const cfg_field_t palette_dto_fields[] = {
	CFG_INT(palette_dto, ref_no, "ref_no"),
	CFG_INT_ARRAY(palette_dto, colors, color_count, "colors",
		      NET_MAX_PALETTE_COLORS),
	CFG_BOOL(palette_dto, inverted, "inverted"),
};
FC_DEF(palette_dto);

const cfg_field_t palette_list_item_dto_fields[] = {
	CFG_INT(palette_list_item_dto, ref_no, "ref_no"),
	CFG_STRING(palette_list_item_dto, name, "name"),
};
FC_DEF(palette_list_item_dto);

const cfg_field_t gain_mode_dto_fields[] = {
	CFG_INT(gain_mode_dto, mode, "mode"),
	CFG_FLOAT(gain_mode_dto, max, "max"),
	CFG_FLOAT(gain_mode_dto, min, "min"),
	CFG_BOOL(gain_mode_dto, fixedSpan, "fixedSpan"),
	CFG_FLOAT(gain_mode_dto, span, "span"),
};
FC_DEF(gain_mode_dto);

const cfg_field_t stream_info_dto_fields[] = {
	CFG_INT(stream_info_dto, id, "id"),
	CFG_INT(stream_info_dto, bit_rate, "bit_rate"),
	CFG_STRING(stream_info_dto, codec_type, "codec_type"),
	CFG_BOOL(stream_info_dto, disable_skip, "disable_skip"),
	CFG_BOOL(stream_info_dto, enable_rc, "enable_rc"),
	CFG_INT(stream_info_dto, gop_size, "gop_size"),
	CFG_INT(stream_info_dto, h264_profile, "h264_profile"),
	CFG_INT(stream_info_dto, initial_qp, "initial_qp"),
	CFG_INT(stream_info_dto, max_frame_rate, "max_frame_rate"),
	CFG_INT(stream_info_dto, maximum_qp, "maximum_qp"),
	CFG_STRING(stream_info_dto, quality_control_type, "quality_control_type"),
	CFG_INT(stream_info_dto, resolution_width, "resolution_width"),
	CFG_INT(stream_info_dto, resolution_height, "resolution_height"),
};
FC_DEF(stream_info_dto);

const cfg_field_t stream_status_dto_fields[] = {
	CFG_INT(stream_status_dto, id, "id"),
	CFG_INT(stream_status_dto, connection_count, "connection_count"),
};
FC_DEF(stream_status_dto);

const cfg_field_t system_status_dto_fields[] = {
	CFG_BOOL(system_status_dto, streaming, "streaming"),
	CFG_INT(system_status_dto, provision_state, "provision_state"),
	CFG_INT(system_status_dto, network_state, "network_state"),
	CFG_INT(system_status_dto, network_mode, "network_mode"),
	CFG_INT(system_status_dto, thermal_source, "thermal_source"),
	CFG_STRING(system_status_dto, ssid, "ssid"),
	CFG_STRING(system_status_dto, ip, "ip"),
};
FC_DEF(system_status_dto);

/* ==================== cfg <-> dto ==================== */

void device_info_to_dto(const hsi_info_cfg *s, device_info_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	COPY_STR(d->name, s->name);
	COPY_STR(d->model, s->model);
	COPY_STR(d->spec, s->spec);
	COPY_STR(d->serial_no, s->serial_no);
	COPY_STR(d->id, s->id);
	COPY_STR(d->brand, s->brand);
	COPY_STR(d->company, s->company);
	COPY_STR(d->firmware_name, s->firmware_name);
	COPY_STR(d->firmware_version, s->firmware_version);
}

void device_info_from_hsi_info_to_dto(const hsi_info_cfg *s,
				     const recipe_info_cfg *r,
				     device_info_dto *d)
{
	if (!s || !r || !d)
		return;

	memset(d, 0, sizeof(*d));
	COPY_STR(d->name, s->name);
	COPY_STR(d->model, s->model);
	COPY_STR(d->spec, s->spec);
	COPY_STR(d->serial_no, s->serial_no);
	COPY_STR(d->id, s->id);
	COPY_STR(d->brand, r->brand);
	COPY_STR(d->company, r->company);
	COPY_STR(d->firmware_name, s->firmware_name);
	COPY_STR(d->firmware_version, s->firmware_version);
}

void time_info_to_dto(const time_info_cfg *s, time_info_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->time_mode = s->time_mode;
	COPY_STR(d->local_time, s->local_time);
	COPY_STR(d->time_zone, s->time_zone);
}

void time_info_from_dto(const time_info_dto *d, time_info_cfg *s)
{
	if (!d || !s)
		return;

	s->time_mode = d->time_mode;
	COPY_STR(s->local_time, d->local_time);
	COPY_STR(s->time_zone, d->time_zone);
}

void ntp_info_to_dto(const ntp_info_cfg *s, ntp_info_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	COPY_STR(d->host_name, s->host_name);
	d->port_no = s->port_no;
	d->syn_interval = s->syn_interval;
}

void ntp_info_from_dto(const ntp_info_dto *d, ntp_info_cfg *s)
{
	if (!d || !s)
		return;

	COPY_STR(s->host_name, d->host_name);
	s->port_no = d->port_no;
	s->syn_interval = d->syn_interval;
}

void network_info_to_dto(const network_info_cfg *s, network_info_dto *d)
{
	int i;

	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->id = s->id;
	COPY_STR(d->name, s->name);
	d->dhcp = s->dhcp;
	COPY_STR(d->ip, s->ip);
	COPY_STR(d->netmask, s->netmask);
	d->gateway_count = s->gateway_count;
	for (i = 0; i < s->gateway_count && i < NET_MAX_ARRAY; ++i)
		COPY_STR(d->gateway[i], s->gateway[i]);
	d->dns_count = s->dns_count;
	for (i = 0; i < s->dns_count && i < NET_MAX_ARRAY; ++i)
		COPY_STR(d->dns[i], s->dns[i]);
}

void network_info_from_dto(const network_info_dto *d, network_info_cfg *s)
{
	int i;

	if (!d || !s)
		return;

	s->id = d->id;
	COPY_STR(s->name, d->name);
	s->dhcp = d->dhcp;
	COPY_STR(s->ip, d->ip);
	COPY_STR(s->netmask, d->netmask);
	s->gateway_count = d->gateway_count;
	for (i = 0; i < d->gateway_count && i < NET_MAX_ARRAY; ++i)
		COPY_STR(s->gateway[i], d->gateway[i]);
	s->dns_count = d->dns_count;
	for (i = 0; i < d->dns_count && i < NET_MAX_ARRAY; ++i)
		COPY_STR(s->dns[i], d->dns[i]);
}

void port_info_to_dto(const port_info_cfg *s, port_info_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->http_port_no = s->http_port_no;
	d->rtsp_port_no = s->rtsp_port_no;
	d->rtp_port_no = s->rtp_port_no;
}

void port_info_from_dto(const port_info_dto *d, port_info_cfg *s)
{
	if (!d || !s)
		return;

	s->http_port_no = d->http_port_no;
	s->rtsp_port_no = d->rtsp_port_no;
	s->rtp_port_no = d->rtp_port_no;
}

void intrusion_alarm_to_dto(const intrusion_alarm_cfg *s,
			    intrusion_alarm_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->enabled = s->enabled;
	d->no_repeat_minutes = s->no_repeat_minutes;
	d->sensitivity = s->sensitivity;
	d->record_duration_sec = s->record_duration_sec;
}

void intrusion_alarm_from_dto(const intrusion_alarm_dto *d,
			      intrusion_alarm_cfg *s)
{
	if (!d || !s)
		return;

	s->enabled = d->enabled;
	s->no_repeat_minutes = d->no_repeat_minutes;
	s->sensitivity = d->sensitivity;
	s->record_duration_sec = d->record_duration_sec;
}

void high_temp_alarm_to_dto(const high_temp_alarm_cfg *s,
			    high_temp_alarm_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->enabled = s->enabled;
	d->no_repeat_minutes = s->no_repeat_minutes;
	d->alarm_temp = s->alarm_temp;
	d->record_duration_sec = s->record_duration_sec;
}

void high_temp_alarm_from_dto(const high_temp_alarm_dto *d,
			      high_temp_alarm_cfg *s)
{
	if (!d || !s)
		return;

	s->enabled = d->enabled;
	s->no_repeat_minutes = d->no_repeat_minutes;
	s->alarm_temp = d->alarm_temp;
	s->record_duration_sec = d->record_duration_sec;
}

void body_temp_alarm_to_dto(const body_temp_alarm_cfg *s,
			    body_temp_alarm_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->enabled = s->enabled;
	d->sensitivity = s->sensitivity;
}

void body_temp_alarm_from_dto(const body_temp_alarm_dto *d,
			      body_temp_alarm_cfg *s)
{
	if (!d || !s)
		return;

	s->enabled = d->enabled;
	s->sensitivity = d->sensitivity;
}

void alarm_trigger_to_dto(const alarm_trigger_cfg *s, int type,
			  alarm_trigger_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->type = type;
	d->trigger_ftp_enabled = s->trigger_ftp_enabled;
	d->trigger_webhook_enabled = s->trigger_webhook_enabled;
	d->trigger_record_enabled = s->trigger_record_enabled;
}

void alarm_trigger_from_dto(const alarm_trigger_dto *d, alarm_trigger_cfg *s)
{
	if (!d || !s)
		return;

	s->trigger_ftp_enabled = d->trigger_ftp_enabled;
	s->trigger_webhook_enabled = d->trigger_webhook_enabled;
	s->trigger_record_enabled = d->trigger_record_enabled;
}

void ftp_info_to_dto(const ftp_info_cfg *s, ftp_info_dto *d)
{
	if (s && d)
		*d = *s;
}

void ftp_info_from_dto(const ftp_info_dto *d, ftp_info_cfg *s)
{
	if (d && s)
		*s = *d;
}

void webhook_info_to_dto(const webhook_info_cfg *s, webhook_info_dto *d)
{
	if (s && d)
		*d = *s;
}

void webhook_info_from_dto(const webhook_info_dto *d, webhook_info_cfg *s)
{
	if (d && s)
		*s = *d;
}

void thermal_settings_to_shutter_mode_dto(const thermal_settings_cfg *s,
					  shutter_mode_dto *d)
{
	if (s && d)
		d->mode = s->shutter_mode;
}

void thermal_settings_from_shutter_mode_dto(const shutter_mode_dto *d,
					    thermal_settings_cfg *s)
{
	if (d && s)
		s->shutter_mode = d->mode;
}

void thermal_settings_to_shutter_time_dto(const thermal_settings_cfg *s,
					  shutter_time_dto *d)
{
	if (s && d)
		d->interval = s->shutter_interval_min;
}

void thermal_settings_from_shutter_time_dto(const shutter_time_dto *d,
					    thermal_settings_cfg *s)
{
	if (d && s)
		s->shutter_interval_min = d->interval;
}

void thermal_settings_to_range_dto(const thermal_settings_cfg *s,
				   thermal_range_dto *d)
{
	if (s && d)
		d->range_id = s->range_id;
}

void thermal_settings_from_range_dto(const thermal_range_dto *d,
				     thermal_settings_cfg *s)
{
	if (d && s)
		s->range_id = d->range_id;
}

void thermal_settings_to_params_dto(const thermal_settings_cfg *s,
				    thermal_params_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->emissivity = s->emissivity;
	d->distance = s->distance;
	d->refl_temp = s->refl_temp;
	d->atm_temp = s->atm_temp;
	d->rel_humidity = s->rel_humidity;
}

void thermal_settings_from_params_dto(const thermal_params_dto *d,
				      thermal_settings_cfg *s)
{
	if (!d || !s)
		return;

	s->emissivity = d->emissivity;
	s->distance = d->distance;
	s->refl_temp = d->refl_temp;
	s->atm_temp = d->atm_temp;
	s->rel_humidity = d->rel_humidity;
}

void thermal_settings_to_unit_dto(const thermal_settings_cfg *s,
				  thermal_unit_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->temp_unit = s->temp_unit;
	d->distance_unit = s->distance_unit;
}

void thermal_settings_from_unit_dto(const thermal_unit_dto *d,
				    thermal_settings_cfg *s)
{
	if (!d || !s)
		return;

	s->temp_unit = d->temp_unit;
	s->distance_unit = d->distance_unit;
}

void image_settings_to_params_dto(const image_settings_cfg *s,
				  image_params_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->contrast = s->contrast;
	d->brightness = s->brightness;
	d->dde_enable = s->dde_enable;
	d->dde_value = s->dde_value;
}

void image_settings_from_params_dto(const image_params_dto *d,
				    image_settings_cfg *s)
{
	if (!d || !s)
		return;

	s->contrast = d->contrast;
	s->brightness = d->brightness;
	s->dde_enable = d->dde_enable;
	s->dde_value = d->dde_value;
}

void image_settings_to_flip_dto(const image_settings_cfg *s,
				image_flip_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->mode = s->flip_mode;
	d->rotation = s->rotation;
}

void image_settings_from_flip_dto(const image_flip_dto *d,
				  image_settings_cfg *s)
{
	if (!d || !s)
		return;

	s->flip_mode = d->mode;
	s->rotation = d->rotation;
}

void thermal_settings_to_color_dist_dto(const thermal_settings_cfg *s,
					color_dist_dto *d)
{
	if (!s || !d)
		return;
	memset(d, 0, sizeof(*d));
	d->type = s->color_dist;
}

void thermal_settings_from_color_dist_dto(const color_dist_dto *d,
					  thermal_settings_cfg *s)
{
	if (d && s)
		s->color_dist = d->type;
}

void thermal_settings_to_palette_dto(const thermal_settings_cfg *s,
				     palette_dto *d)
{
	int count;

	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->ref_no = s->palette_ref_no;
	d->inverted = s->palette_inverted ? 1 : 0;
	if (s->palette_ref_no == NET_CUSTOM_PALETTE) {
		count = s->palette_color_count;
		if (count < 0)
			count = 0;
		if (count > NET_MAX_PALETTE_COLORS)
			count = NET_MAX_PALETTE_COLORS;
		d->color_count = count;
		if (count > 0)
			memcpy(d->colors, s->palette_colors,
			       sizeof(int) * (size_t)count);
	}
	if (d->inverted && d->color_count > 0)
		reverse_palette_colors(d->colors, d->color_count);
}

void thermal_settings_from_palette_dto(const palette_dto *d,
				       thermal_settings_cfg *s)
{
	int count;

	if (!d || !s)
		return;

	s->palette_ref_no = d->ref_no;
	s->palette_inverted = d->inverted ? 1 : 0;
	if (d->ref_no != NET_CUSTOM_PALETTE) {
		s->palette_color_count = 0;
		return;
	}

	count = d->color_count;
	if (count < 0)
		count = 0;
	if (count > NET_MAX_PALETTE_COLORS)
		count = NET_MAX_PALETTE_COLORS;
	s->palette_color_count = count;
	if (count > 0)
		memcpy(s->palette_colors, d->colors, sizeof(int) * (size_t)count);
}

void thermal_settings_to_gain_dto(const thermal_settings_cfg *s,
				  gain_mode_dto *d)
{
	int tu;

	if (!s || !d)
		return;

	tu = g_config.thermal.temp_unit;
	memset(d, 0, sizeof(*d));
	d->mode = s->gain_mode;
	d->max = temp_kelvin_to_unit(s->gain_max, tu);
	d->min = temp_kelvin_to_unit(s->gain_min, tu);
	d->fixedSpan = s->gain_fixed_span ? 1 : 0;
	d->span = temp_kelvin_delta_to_unit(s->gain_span, tu);
}

void thermal_settings_from_gain_dto(const gain_mode_dto *d,
				    thermal_settings_cfg *s)
{
	int tu;

	if (!d || !s)
		return;

	tu = g_config.thermal.temp_unit;
	s->gain_mode = d->mode;
	s->gain_max = temp_unit_to_kelvin(d->max, tu);
	s->gain_min = temp_unit_to_kelvin(d->min, tu);
	s->gain_fixed_span = d->fixedSpan ? 1 : 0;
	s->gain_span = temp_unit_delta_to_kelvin(d->span, tu);
}

void gain_mode_dto_set_live_kelvin(gain_mode_dto *d, float min_k, float max_k)
{
	int tu;

	if (!d)
		return;
	tu = g_config.thermal.temp_unit;
	d->min = temp_kelvin_to_unit(min_k, tu);
	d->max = temp_kelvin_to_unit(max_k, tu);
}

void stream_info_to_dto(const stream_info_cfg *s, stream_info_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->id = s->id;
	d->bit_rate = s->bit_rate;
	COPY_STR(d->codec_type, s->codec_type);
	d->disable_skip = s->disable_skip;
	d->enable_rc = s->enable_rc;
	d->gop_size = s->gop_size;
	d->h264_profile = s->h264_profile;
	d->initial_qp = s->initial_qp;
	d->max_frame_rate = s->max_frame_rate;
	d->maximum_qp = s->maximum_qp;
	COPY_STR(d->quality_control_type, s->quality_control_type);
	d->resolution_width = s->width;
	d->resolution_height = s->height;
}

void stream_status_to_dto(const stream_info_cfg *s, stream_status_dto *d)
{
	if (!s || !d)
		return;

	memset(d, 0, sizeof(*d));
	d->id = s->id;
	d->connection_count = s->connection_count;
}

void system_status_to_dto(system_status_dto *d)
{
	network_iface_t link;
	int have_ip;

	if (!d)
		return;

	memset(d, 0, sizeof(*d));

	d->streaming = g_system_status.streaming;
	d->provision_state = g_config.provision.provision_state;
	/* Radio role is a persisted user choice; reachability is not. */
	d->network_mode = g_config.provision.network_mode;
	d->thermal_source = NET_THERMAL_SOURCE_UVC;

	/*
	 * network_state must be probed live: main_app owns the radio and only
	 * persists the provisioning snapshot, so the stored value goes stale
	 * as soon as the link changes (and net_app loads it once at startup).
	 */
	have_ip = network_read_iface(NETWORK_IFACE_WLAN, &link) == 0 &&
		  link.ip[0] != '\0';
	if (d->network_mode == NET_NETWORK_MODE_AP)
		d->network_state = have_ip ? NET_NETWORK_STATE_ONLINE :
					     NET_NETWORK_STATE_OFFLINE;
	else
		d->network_state = (have_ip && link.carrier) ?
					   NET_NETWORK_STATE_ONLINE :
					   NET_NETWORK_STATE_OFFLINE;

	if (have_ip)
		COPY_STR(d->ip, link.ip);
	else if (g_config.networks[0].ip[0])
		COPY_STR(d->ip, g_config.networks[0].ip);

	/* network_state already says whether the STA link is up, so report the
	 * saved SSID unconditionally — the UI needs the name to say "X 离线". */
	if (d->network_mode == NET_NETWORK_MODE_AP)
		COPY_STR(d->ssid, g_config.provision.hotspot_ssid);
	else
		COPY_STR(d->ssid, g_config.provision.ssid);
}

/* ==================== validators ==================== */

int str_contains_windows_folder_illegal_chars(const char *s)
{
	if (!s)
		return 0;

	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;

		if (c < 0x20 || c == 0x7F)
			return 1;
		switch (c) {
		case '\\':
		case '/':
		case ':':
		case '*':
		case '?':
		case '"':
		case '<':
		case '>':
		case '|':
			return 1;
		default:
			break;
		}
	}
	return 0;
}

int time_info_dto_is_valid(const time_info_dto *dto)
{
	return dto && (dto->time_mode == NET_TIME_MODE_LOCAL ||
		       dto->time_mode == NET_TIME_MODE_NTP);
}

int ntp_info_dto_is_valid(const ntp_info_dto *dto)
{
	if (!dto || dto->host_name[0] == '\0')
		return 0;
	if (dto->port_no <= 0 || dto->port_no > 65535)
		return 0;
	if (dto->syn_interval <= 0)
		return 0;
	return 1;
}

/* ==================== network DTO validation (ported from Holly) ==================== */

static int is_valid_ipv4(const char *s)
{
	struct in_addr tmp;
	return s && s[0] && inet_pton(AF_INET, s, &tmp) == 1;
}

static int is_valid_netmask(const char *s)
{
	struct in_addr tmp;
	uint32_t mask;

	if (!s || !s[0] || inet_pton(AF_INET, s, &tmp) != 1)
		return 0;
	mask = ntohl(tmp.s_addr);
	return mask == 0 || (~mask & (~mask + 1)) == 0;
}

static int is_valid_ifname(const char *s)
{
	size_t len;
	size_t i;

	if (!s || !s[0])
		return 0;
	len = strlen(s);
	if (len >= IFNAMSIZ)
		return 0;
	for (i = 0; i < len; i++) {
		char c = s[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		      (c >= '0' && c <= '9') || c == '_' || c == '.'))
			return 0;
	}
	return 1;
}

static int parse_ipv4_mask(const char *ip, const char *netmask,
			   uint32_t *ip_h, uint32_t *mask_h)
{
	struct in_addr ia, ma;
	const char *mask_s = (netmask && netmask[0]) ? netmask : "255.255.255.0";

	if (!ip || !ip[0] || inet_pton(AF_INET, ip, &ia) != 1)
		return -1;
	if (inet_pton(AF_INET, mask_s, &ma) != 1)
		return -1;
	*ip_h = ntohl(ia.s_addr);
	*mask_h = ntohl(ma.s_addr);
	return 0;
}

static int ipv4_same_subnet(const char *a, const char *b, const char *netmask)
{
	uint32_t ah, bh, mask;

	if (parse_ipv4_mask(a, netmask, &ah, &mask) != 0)
		return 0;
	if (parse_ipv4_mask(b, netmask, &bh, &mask) != 0)
		return 0;
	return (ah & mask) == (bh & mask);
}

static int ipv4_is_network_or_broadcast(const char *ip, const char *netmask)
{
	uint32_t ip_h, mask_h, net, bcast;

	if (parse_ipv4_mask(ip, netmask, &ip_h, &mask_h) != 0)
		return 1;
	if (mask_h == 0xffffffffu || mask_h == 0xfffffffeu)
		return 0;
	net = ip_h & mask_h;
	bcast = net | ~mask_h;
	return ip_h == net || ip_h == bcast;
}

static int ipv4_is_unusable_host(const char *ip)
{
	struct in_addr a;

	if (!ip || !ip[0] || inet_pton(AF_INET, ip, &a) != 1)
		return 1;
	return a.s_addr == 0 || a.s_addr == 0xffffffffu;
}

int network_info_dto_is_valid(const network_info_dto *dto)
{
	int i;

	if (!dto) {
		LOGE("network_info: null dto");
		return 0;
	}
	if (!is_valid_ifname(dto->name)) {
		LOGE("network_info: invalid ifname '%s'", dto->name);
		return 0;
	}
	if (dto->dhcp != 0 && dto->dhcp != 1) {
		LOGE("network_info: invalid dhcp=%d", dto->dhcp);
		return 0;
	}

	if (!dto->dhcp) {
		const char *mask = dto->netmask;

		if (!is_valid_ipv4(dto->ip) || ipv4_is_unusable_host(dto->ip)) {
			LOGE("network_info: invalid ip '%s'", dto->ip);
			return 0;
		}
		if (mask[0] && !is_valid_netmask(mask)) {
			LOGE("network_info: invalid netmask '%s'", mask);
			return 0;
		}
		if (dto->gateway_count < 1 || !is_valid_ipv4(dto->gateway[0]) ||
		    ipv4_is_unusable_host(dto->gateway[0])) {
			LOGE("network_info: invalid or missing gateway (count=%d gw0='%s')",
			     dto->gateway_count,
			     dto->gateway_count > 0 ? dto->gateway[0] : "");
			return 0;
		}
		if (ipv4_is_network_or_broadcast(dto->ip, mask)) {
			LOGE("network_info: ip '%s' is network/broadcast (mask='%s')",
			     dto->ip, mask[0] ? mask : "255.255.255.0");
			return 0;
		}
		if (ipv4_is_network_or_broadcast(dto->gateway[0], mask)) {
			LOGE("network_info: gateway '%s' is network/broadcast (mask='%s')",
			     dto->gateway[0], mask[0] ? mask : "255.255.255.0");
			return 0;
		}
		if (strcmp(dto->ip, dto->gateway[0]) == 0) {
			LOGE("network_info: gateway equals ip '%s'", dto->ip);
			return 0;
		}
		if (!ipv4_same_subnet(dto->ip, dto->gateway[0], mask)) {
			LOGE("network_info: gateway '%s' not in same subnet as ip '%s' mask '%s'",
			     dto->gateway[0], dto->ip,
			     mask[0] ? mask : "255.255.255.0");
			return 0;
		}
	}

	for (i = 0; i < dto->gateway_count && i < NET_MAX_ARRAY; i++) {
		if (dto->gateway[i][0] &&
		    (!is_valid_ipv4(dto->gateway[i]) ||
		     ipv4_is_unusable_host(dto->gateway[i]))) {
			LOGE("network_info: invalid gateway[%d]='%s'",
			     i, dto->gateway[i]);
			return 0;
		}
	}
	for (i = 0; i < dto->dns_count && i < NET_MAX_ARRAY; i++) {
		if (dto->dns[i][0] &&
		    (!is_valid_ipv4(dto->dns[i]) ||
		     ipv4_is_unusable_host(dto->dns[i]))) {
			LOGE("network_info: invalid dns[%d]='%s'",
			     i, dto->dns[i]);
			return 0;
		}
	}
	return 1;
}

int port_info_dto_is_valid(const port_info_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->http_port_no <= 0 || dto->http_port_no > 65535 ||
	    dto->rtsp_port_no <= 0 || dto->rtsp_port_no > 65535 ||
	    dto->rtp_port_no <= 0 || dto->rtp_port_no > 65535)
		return 0;
	if (dto->http_port_no == dto->rtsp_port_no ||
	    dto->http_port_no == dto->rtp_port_no ||
	    dto->rtsp_port_no == dto->rtp_port_no)
		return 0;
	return 1;
}

int intrusion_alarm_dto_is_valid(const intrusion_alarm_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->no_repeat_minutes < 0)
		return 0;
	if (dto->sensitivity < 0 || dto->sensitivity > 2)
		return 0;
	if (!record_duration_ok(dto->record_duration_sec))
		return 0;
	return 1;
}

int high_temp_alarm_dto_is_valid(const high_temp_alarm_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->no_repeat_minutes < 0 || !isfinite(dto->alarm_temp))
		return 0;
	if (!record_duration_ok(dto->record_duration_sec))
		return 0;
	return 1;
}

int body_temp_alarm_dto_is_valid(const body_temp_alarm_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->sensitivity < NET_ALARM_SENSITIVITY_LOW ||
	    dto->sensitivity > NET_ALARM_SENSITIVITY_HIGH)
		return 0;
	return 1;
}

int alarm_trigger_dto_is_valid(const alarm_trigger_dto *dto, int path_type)
{
	if (!dto || net_alarm_type_index(path_type) < 0)
		return 0;
	return dto->type == path_type;
}

int ftp_info_dto_is_valid(const ftp_info_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->ip[0] == '\0')
		return 1;
	return dto->port_no > 0 && dto->port_no <= 65535;
}

int webhook_info_dto_is_valid(const webhook_info_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->url[0] == '\0')
		return 1;
	return strncmp(dto->url, "http://", 7) == 0 ||
	       strncmp(dto->url, "https://", 8) == 0;
}

int shutter_mode_dto_is_valid(const shutter_mode_dto *dto)
{
	return dto && (dto->mode == NET_SHUTTER_AUTOMATIC ||
		       dto->mode == NET_SHUTTER_MANUAL);
}

int shutter_time_dto_is_valid(const shutter_time_dto *dto)
{
	return dto && dto->interval >= 1 && dto->interval <= 65535;
}

int thermal_range_dto_is_valid(const thermal_range_dto *dto)
{
	/* Range ids are 1-based (lens range_map). */
	return dto && dto->range_id >= 1 && dto->range_id < 256;
}

int thermal_params_dto_is_valid(const thermal_params_dto *dto)
{
	if (!dto)
		return 0;
	if (!isfinite(dto->emissivity) || dto->emissivity < 0.01f ||
	    dto->emissivity > 1.0f)
		return 0;
	if (!isfinite(dto->distance) || dto->distance < 0.0f)
		return 0;
	/* Wire unit is Kelvin. */
	if (!isfinite(dto->refl_temp) || dto->refl_temp <= 0.0f ||
	    dto->refl_temp > 1000.0f)
		return 0;
	if (!isfinite(dto->atm_temp) || dto->atm_temp <= 0.0f ||
	    dto->atm_temp > 1000.0f)
		return 0;
	if (!isfinite(dto->rel_humidity) || dto->rel_humidity < 0.0f ||
	    dto->rel_humidity > 1.0f)
		return 0;
	return 1;
}

int thermal_unit_dto_is_valid(const thermal_unit_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->temp_unit < NET_TEMP_UNIT_CELSIUS ||
	    dto->temp_unit > NET_TEMP_UNIT_FAHRENHEIT)
		return 0;
	return dto->distance_unit == NET_DISTANCE_UNIT_METER ||
	       dto->distance_unit == NET_DISTANCE_UNIT_FEET;
}

int image_params_dto_is_valid(const image_params_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->contrast < 0 || dto->contrast > 100 ||
	    dto->brightness < 0 || dto->brightness > 100)
		return 0;
	if (dto->dde_enable != 0 && dto->dde_enable != 1)
		return 0;
	return dto->dde_value >= 0 && dto->dde_value <= 100;
}

int image_flip_dto_is_valid(const image_flip_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->mode < NET_IMAGE_FLIP_NONE || dto->mode > NET_IMAGE_FLIP_CENTER)
		return 0;
	return dto->rotation == 0 || dto->rotation == 90 ||
	       dto->rotation == 180 || dto->rotation == 270;
}

int color_dist_dto_is_valid(const color_dist_dto *dto)
{
	if (!dto)
		return 0;
	return dto->type == NET_THERMAL_T_TWB ||
	       dto->type == NET_THERMAL_TEMP_LINEAR ||
	       dto->type == NET_THERMAL_AD_LINEAR;
}

int palette_dto_is_valid(const palette_dto *dto)
{
	if (!dto)
		return 0;
	if (dto->inverted != 0 && dto->inverted != 1)
		return 0;
	if (dto->ref_no == NET_CUSTOM_PALETTE)
		return dto->color_count > 0 &&
		       dto->color_count <= NET_MAX_PALETTE_COLORS;
	return dto->ref_no >= 1 && dto->ref_no < NET_CUSTOM_PALETTE;
}

static int gain_kelvin_range_is_valid(int mode, float min_k, float max_k)
{
	if (mode != NET_THERMAL_FIXED && mode != NET_THERMAL_AUTO)
		return 0;
	if (!(min_k < max_k) || min_k < 50.0f || max_k > 1200.0f)
		return 0;
	if (mode == NET_THERMAL_FIXED && (max_k - min_k) < 0.05f)
		return 0;
	return 1;
}

int gain_mode_dto_is_valid(const gain_mode_dto *dto)
{
	int tu;
	float min_k;
	float max_k;
	float span_k;

	if (!dto)
		return 0;
	if (dto->mode != NET_THERMAL_FIXED && dto->mode != NET_THERMAL_AUTO)
		return 0;
	if (dto->fixedSpan != 0 && dto->fixedSpan != 1)
		return 0;

	tu = g_config.thermal.temp_unit;
	/* FIXED + fixedSpan: width is span; endpoints come from mid±span/2. */
	if (dto->mode == NET_THERMAL_FIXED && dto->fixedSpan) {
		span_k = temp_unit_delta_to_kelvin(dto->span, tu);
		return span_k >= 0.05f;
	}

	min_k = temp_unit_to_kelvin(dto->min, tu);
	max_k = temp_unit_to_kelvin(dto->max, tu);
	return gain_kelvin_range_is_valid(dto->mode, min_k, max_k);
}

/* ==================== array helpers ==================== */

json_t networks_to_json(const network_info_cfg *nets, int count)
{
	network_info_dto dtos[NET_MAX_NETWORKS];
	int i;

	for (i = 0; i < count && i < NET_MAX_NETWORKS; ++i)
		network_info_to_dto(&nets[i], &dtos[i]);
	return cfg_array_to_json(dtos, count, sizeof(network_info_dto),
				 network_info_dto_fields, network_info_dto_fc);
}

int networks_from_json(const char *body, int len, network_info_cfg *out_nets,
		       int max_count, int *out_count)
{
	network_info_dto dtos[NET_MAX_NETWORKS];
	cJSON *root;
	int ret;
	int i;

	if (!body || !out_nets || !out_count || max_count <= 0)
		return -1;

	root = cJSON_ParseWithLength(body, (size_t)len);
	if (!root || !cJSON_IsArray(root)) {
		cJSON_Delete(root);
		return -1;
	}

	ret = cfg_array_from_json(root, dtos, max_count, sizeof(network_info_dto),
				  network_info_dto_fields,
				  network_info_dto_fc, out_count);
	cJSON_Delete(root);
	if (ret != 0 || *out_count <= 0)
		return -1;

	for (i = 0; i < *out_count; ++i) {
		if (!network_info_dto_is_valid(&dtos[i]))
			return -1;
		network_info_from_dto(&dtos[i], &out_nets[i]);
	}
	return 0;
}

json_t streams_to_json(void)
{
	stream_info_dto dtos[NET_MAX_STREAMS];
	int i;
	int count = g_config.stream_count;

	if (count > NET_MAX_STREAMS)
		count = NET_MAX_STREAMS;
	for (i = 0; i < count; ++i)
		stream_info_to_dto(&g_config.streams[i], &dtos[i]);
	return cfg_array_to_json(dtos, count, sizeof(stream_info_dto),
				 stream_info_dto_fields, stream_info_dto_fc);
}

json_t lens_info_to_json(const thermal_lens_info_t *info)
{
	json_t root;
	json_t ranges;
	int i;

	if (!info)
		return NULL;

	root = json_create_object();
	if (!root)
		return NULL;
	json_set_string(root, "name", info->name);
	json_set_float(root, "focal", info->focal);
	json_set_float(root, "vfov", info->vfov);
	json_set_float(root, "hfov", info->hfov);
	json_set_string(root, "band", info->band);

	ranges = json_create_array();
	if (ranges) {
		for (i = 0; i < info->ranges_count; ++i) {
			json_t item = json_create_object();
			json_t range = json_create_object();

			if (!item || !range) {
				json_free(item);
				json_free(range);
				continue;
			}
			json_set_int(item, "id", info->range_map[i].id);
			json_set_float(range, "min", info->range_map[i].min);
			json_set_float(range, "max", info->range_map[i].max);
			cJSON_AddItemToObject(item, "range", range);
			json_array_add_item(ranges, item);
		}
		cJSON_AddItemToObject(root, "range_map", ranges);
	}
	return root;
}
