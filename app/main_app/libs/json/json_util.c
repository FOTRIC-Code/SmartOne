#include "json_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t json_create_object(void)
{
	return cJSON_CreateObject();
}

json_t json_create_array(void)
{
	return cJSON_CreateArray();
}

int json_set_int(json_t json, const char *key, int val)
{
	cJSON *item;

	if (!json || !key)
		return -1;
	item = cJSON_GetObjectItem(json, key);
	if (item)
		cJSON_SetNumberValue(item, val);
	else
		cJSON_AddNumberToObject(json, key, val);
	return 0;
}

int json_set_float(json_t json, const char *key, float val)
{
	cJSON *item;

	if (!json || !key)
		return -1;
	item = cJSON_GetObjectItem(json, key);
	if (item)
		cJSON_SetNumberValue(item, (double)val);
	else
		cJSON_AddNumberToObject(json, key, (double)val);
	return 0;
}

int json_set_string(json_t json, const char *key, const char *val)
{
	cJSON *item;

	if (!json || !key)
		return -1;
	item = cJSON_GetObjectItem(json, key);
	if (item)
		cJSON_SetValuestring(item, val ? val : "");
	else
		cJSON_AddStringToObject(json, key, val ? val : "");
	return 0;
}

int json_set_bool(json_t json, const char *key, int val)
{
	if (!json || !key)
		return -1;
	cJSON_AddBoolToObject(json, key, val);
	return 0;
}

int json_array_add_item(json_t array, json_t item)
{
	if (!array || !item)
		return -1;
	cJSON_AddItemToArray(array, item);
	return 0;
}

int json_array_add_int(json_t array, int val)
{
	cJSON *item;

	if (!array)
		return -1;
	item = cJSON_CreateNumber(val);
	if (!item)
		return -1;
	cJSON_AddItemToArray(array, item);
	return 0;
}

int json_array_add_string(json_t array, const char *val)
{
	cJSON *item;

	if (!array)
		return -1;
	item = cJSON_CreateString(val ? val : "");
	if (!item)
		return -1;
	cJSON_AddItemToArray(array, item);
	return 0;
}

int json_load_file(const char *path, json_t *out_json)
{
	char *data;

	if (!path || !out_json)
		return -1;
	data = json_read_file(path);
	if (!data)
		return -1;
	*out_json = cJSON_Parse(data);
	free(data);
	if (!*out_json)
		return -1;
	return 0;
}

int json_save_file(const char *path, json_t json)
{
	return json ? json_write_file(path, json) : -1;
}

char *json_to_string(json_t root)
{
	return root ? cJSON_PrintUnformatted(root) : NULL;
}

int json_write_file(const char *path, json_t root)
{
	FILE *fp;
	char *text;

	text = cJSON_Print(root);
	if (!text)
		return -1;
	fp = fopen(path, "wb");
	if (!fp) {
		free(text);
		return -1;
	}
	fputs(text, fp);
	fputc('\n', fp);
	fclose(fp);
	free(text);
	return 0;
}

char *json_read_file(const char *path)
{
	FILE *fp;
	long size;
	char *data;

	fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	size = ftell(fp);
	if (size < 0) {
		fclose(fp);
		return NULL;
	}
	rewind(fp);
	data = calloc(1, (size_t)size + 1);
	if (!data) {
		fclose(fp);
		return NULL;
	}
	if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
		free(data);
		fclose(fp);
		return NULL;
	}
	fclose(fp);
	return data;
}
