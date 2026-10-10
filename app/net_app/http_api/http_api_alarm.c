#include "http_api_common.h"

#include <stdio.h>

char *handle_intrusion_alarm_get(void)
{
	intrusion_alarm_cfg *cfg = config_get_intrusion_alarm();
	intrusion_alarm_dto dto;

	if (!cfg)
		return create_error(EC_FAIL, "Config unavailable");
	intrusion_alarm_to_dto(cfg, &dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, intrusion_alarm_dto_fields,
					   intrusion_alarm_dto_fc));
}

char *handle_intrusion_alarm_put(const char *body, int len)
{
	intrusion_alarm_dto dto;
	intrusion_alarm_cfg updated;
	intrusion_alarm_cfg *cur = config_get_intrusion_alarm();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	intrusion_alarm_to_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, intrusion_alarm_dto_fields,
			      intrusion_alarm_dto_fc) != 0 ||
	    !intrusion_alarm_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	intrusion_alarm_from_dto(&dto, &updated);
	if (config_set_intrusion_alarm(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_high_temp_alarm_get(void)
{
	high_temp_alarm_cfg *cfg = config_get_high_temp_alarm();
	high_temp_alarm_dto dto;

	if (!cfg)
		return create_error(EC_FAIL, "Config unavailable");
	high_temp_alarm_to_dto(cfg, &dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, high_temp_alarm_dto_fields,
					   high_temp_alarm_dto_fc));
}

char *handle_high_temp_alarm_put(const char *body, int len)
{
	high_temp_alarm_dto dto;
	high_temp_alarm_cfg updated;
	high_temp_alarm_cfg *cur = config_get_high_temp_alarm();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	high_temp_alarm_to_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, high_temp_alarm_dto_fields,
			      high_temp_alarm_dto_fc) != 0 ||
	    !high_temp_alarm_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	high_temp_alarm_from_dto(&dto, &updated);
	if (config_set_high_temp_alarm(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_body_temp_alarm_get(void)
{
	body_temp_alarm_cfg *cfg = config_get_body_temp_alarm();
	body_temp_alarm_dto dto;

	if (!cfg)
		return create_error(EC_FAIL, "Config unavailable");
	body_temp_alarm_to_dto(cfg, &dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, body_temp_alarm_dto_fields,
					   body_temp_alarm_dto_fc));
}

char *handle_body_temp_alarm_put(const char *body, int len)
{
	body_temp_alarm_dto dto;
	body_temp_alarm_cfg updated;
	body_temp_alarm_cfg *cur = config_get_body_temp_alarm();

	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	body_temp_alarm_to_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, body_temp_alarm_dto_fields,
			      body_temp_alarm_dto_fc) != 0 ||
	    !body_temp_alarm_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	body_temp_alarm_from_dto(&dto, &updated);
	if (config_set_body_temp_alarm(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_alarm_trigger_get(int type)
{
	alarm_trigger_cfg *cfg;
	alarm_trigger_dto dto;

	if (net_alarm_type_index(type) < 0)
		return create_error(EC_INVALID_PARAM, "Invalid alarm type");
	cfg = config_get_alarm_trigger(type);
	if (!cfg)
		return create_error(EC_FAIL, "Config unavailable");
	alarm_trigger_to_dto(cfg, type, &dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, alarm_trigger_dto_fields,
					   alarm_trigger_dto_fc));
}

char *handle_alarm_trigger_put(int type, const char *body, int len)
{
	alarm_trigger_dto dto;
	alarm_trigger_cfg updated;
	alarm_trigger_cfg *cur;

	if (net_alarm_type_index(type) < 0)
		return create_error(EC_INVALID_PARAM, "Invalid alarm type");
	cur = config_get_alarm_trigger(type);
	if (!body || len <= 0 || !cur)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	alarm_trigger_to_dto(cur, type, &dto);
	if (parse_body_to_dto(body, len, &dto, alarm_trigger_dto_fields,
			      alarm_trigger_dto_fc) != 0 ||
	    !alarm_trigger_dto_is_valid(&dto, type))
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	if (dto.trigger_ftp_enabled &&
	    (!config_get_ftp_info() || !config_get_ftp_info()->ip[0]))
		return create_error(EC_INVALID_PARAM, "FTP target is not configured");
	if (dto.trigger_webhook_enabled &&
	    (!config_get_webhook() || !config_get_webhook()->url[0]))
		return create_error(EC_INVALID_PARAM,
				    "Webhook target is not configured");
	alarm_trigger_from_dto(&dto, &updated);
	if (config_set_alarm_trigger(type, &updated) != 0)
		return create_error(EC_FAIL, "Failed to save");
	return create_response(EC_OK, "OK", NULL);
}
