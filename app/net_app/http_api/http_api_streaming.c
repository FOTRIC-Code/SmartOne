#include "http_api_common.h"
#include "http_dto.h"
#include "libs/log/log.h"
#include "net_common/net_common.h"

static stream_info_cfg *find_stream(int id)
{
	int i;

	for (i = 0; i < g_config.stream_count; ++i) {
		if (g_config.streams[i].id == id)
			return &g_config.streams[i];
	}
	return NULL;
}

char *handle_streaming_channels_get(void)
{
	return create_response(EC_OK, "OK", streams_to_json());
}

char *handle_streaming_channel_get(int id)
{
	stream_info_cfg *s = find_stream(id);
	stream_info_dto dto;

	if (!s)
		return create_error(EC_FAIL, "Stream not found");
	stream_info_to_dto(s, &dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, stream_info_dto_fields,
					   stream_info_dto_fc));
}

char *handle_streaming_channel_status_get(int id)
{
	stream_info_cfg *s = find_stream(id);
	stream_status_dto dto;
	json_t data;
	char *response;

	if (!s)
		return create_error(EC_FAIL, "Stream not found");
	stream_status_to_dto(s, &dto);
	data = cfg_to_json(&dto, stream_status_dto_fields, stream_status_dto_fc);
	if (!data)
		return create_error(EC_MEMORY, "OOM");
	/* Runtime overlay — not part of persistent stream DTO fields. */
	cJSON_AddBoolToObject(data, "zlm_ready", g_system_status.zlm_ready);
	cJSON_AddNumberToObject(data, "zlm_http_port", ZLM_HTTP_PORT);
	cJSON_AddNumberToObject(data, "zlm_rtc_port", ZLM_RTC_PORT);
	LOGI("[HTTP] stream status id=%d readers=%d zlm_ready=%d", id,
	     s->connection_count, g_system_status.zlm_ready);
	response = create_response(EC_OK, "OK", data);
	return response;
}
