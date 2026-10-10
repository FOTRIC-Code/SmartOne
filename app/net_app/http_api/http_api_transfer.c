#include "http_api_common.h"

#include "main_process/alarm_transfer.h"

#include <stdio.h>

char *handle_system_ftp_get(void)
{
	ftp_info_cfg *config = config_get_ftp_info();
	ftp_info_dto dto;

	if (!config)
		return create_error(EC_FAIL, "Config unavailable");
	ftp_info_to_dto(config, &dto);
	dto.pwd[0] = '\0';
	return create_response(
		EC_OK, "OK",
		cfg_to_json(&dto, ftp_info_dto_fields, ftp_info_dto_fc));
}

char *handle_system_ftp_put(const char *body, int len)
{
	ftp_info_cfg *current = config_get_ftp_info();
	ftp_info_cfg updated;
	ftp_info_dto dto;

	if (!body || len <= 0 || !current)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	ftp_info_to_dto(current, &dto);
	if (parse_body_to_dto(body, len, &dto, ftp_info_dto_fields,
			      ftp_info_dto_fc) != 0 ||
	    !ftp_info_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid FTP config");
	if (dto.pwd[0] == '\0')
		snprintf(dto.pwd, sizeof(dto.pwd), "%s", current->pwd);
	ftp_info_from_dto(&dto, &updated);
	if (config_set_ftp_info(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save FTP config");
	return create_response(EC_OK, "OK", NULL);
}

char *handle_system_ftp_test(const char *body, int len)
{
	ftp_info_cfg *current = config_get_ftp_info();
	ftp_info_dto dto;

	if (!current)
		return create_error(EC_FAIL, "Config unavailable");
	ftp_info_to_dto(current, &dto);
	if (body && len > 0 &&
	    parse_body_to_dto(body, len, &dto, ftp_info_dto_fields,
			      ftp_info_dto_fc) != 0)
		return create_error(EC_INVALID_PARAM, "Invalid FTP config");
	if (dto.pwd[0] == '\0')
		snprintf(dto.pwd, sizeof(dto.pwd), "%s", current->pwd);
	if (!ftp_info_dto_is_valid(&dto) || alarm_ftp_test(&dto) != 0)
		return create_error(EC_FAIL, "FTP connection failed");
	dto.pwd[0] = '\0';
	return create_response(
		EC_OK, "OK",
		cfg_to_json(&dto, ftp_info_dto_fields, ftp_info_dto_fc));
}

char *handle_system_webhook_get(void)
{
	webhook_info_cfg *config = config_get_webhook();
	webhook_info_dto dto;

	if (!config)
		return create_error(EC_FAIL, "Config unavailable");
	webhook_info_to_dto(config, &dto);
	return create_response(
		EC_OK, "OK",
		cfg_to_json(&dto, webhook_info_dto_fields,
			    webhook_info_dto_fc));
}

char *handle_system_webhook_put(const char *body, int len)
{
	webhook_info_cfg *current = config_get_webhook();
	webhook_info_cfg updated;
	webhook_info_dto dto;

	if (!body || len <= 0 || !current)
		return create_error(EC_INVALID_PARAM, "Invalid request body");
	webhook_info_to_dto(current, &dto);
	if (parse_body_to_dto(body, len, &dto, webhook_info_dto_fields,
			      webhook_info_dto_fc) != 0 ||
	    !webhook_info_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid webhook config");
	webhook_info_from_dto(&dto, &updated);
	if (config_set_webhook(&updated) != 0)
		return create_error(EC_FAIL, "Failed to save webhook config");
	return create_response(EC_OK, "OK", NULL);
}
