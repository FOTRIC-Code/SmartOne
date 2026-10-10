#include "http_api_common.h"

#include "libs/json/json_bind.h"

#include <stdlib.h>
#include <string.h>

char *create_response(int code, const char *message, json_t data)
{
	cJSON *root = cJSON_CreateObject();
	char *text;

	cJSON_AddNumberToObject(root, "code", code);
	cJSON_AddStringToObject(root, "message",
				message ? message : (code == 0 ? "OK" : "Error"));
	if (data)
		cJSON_AddItemToObject(root, "data", data);
	text = json_to_string(root);
	cJSON_Delete(root);
	return text;
}

char *create_error(int code, const char *message)
{
	return create_response(code, message ? message : "Error", NULL);
}

int parse_body_to_dto(const char *data, int len, void *dto,
		      const cfg_field_t *fields, int fc)
{
	char *buf;
	cJSON *json;
	int ret;

	if (!data || len < 0 || !dto || !fields || fc <= 0)
		return -1;

	buf = (char *)malloc((size_t)len + 1);
	if (!buf)
		return -1;
	memcpy(buf, data, (size_t)len);
	buf[len] = '\0';
	json = cJSON_Parse(buf);
	free(buf);
	if (!json)
		return -1;
	ret = cfg_json_types_valid(json, fields, fc) ?
		      cfg_from_json(json, dto, fields, fc) :
		      -1;
	cJSON_Delete(json);
	return ret;
}
