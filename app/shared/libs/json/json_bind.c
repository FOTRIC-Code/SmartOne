#include "json_bind.h"

#include "../file/file_util.h"
#include "../log/log.h"

#include <stdlib.h>
#include <string.h>

json_t cfg_to_json(const void *data, const cfg_field_t *fields, int count)
{
	int i;

	if (!data || !fields)
		return NULL;

	json_t json = json_create_object();
	if (!json)
		return NULL;

	for (i = 0; i < count; i++) {
		const cfg_field_t *f = &fields[i];
		const void *ptr = (const char *)data + f->offset;

		switch (f->type) {
		case CFG_TYPE_INT:
			json_set_int(json, f->key, *(const int *)ptr);
			break;
		case CFG_TYPE_FLOAT:
			json_set_float(json, f->key, *(const float *)ptr);
			break;
		case CFG_TYPE_STRING:
			json_set_string(json, f->key, (const char *)ptr);
			break;
		case CFG_TYPE_BOOL:
			json_set_bool(json, f->key, *(const int *)ptr);
			break;
		case CFG_TYPE_OBJECT: {
			json_t sub = cfg_to_json(ptr, f->sub_fields, f->sub_count);
			if (sub)
				cJSON_AddItemToObject(json, f->key, sub);
			break;
		}
		case CFG_TYPE_OBJ_ARRAY: {
			int cnt = *(const int *)((const char *)data + f->count_offset);
			json_t arr = json_create_array();
			int j;

			for (j = 0; j < cnt && j < f->max_items; j++) {
				const void *item =
					(const char *)ptr + (size_t)j * f->item_size;
				json_t sub =
					cfg_to_json(item, f->sub_fields, f->sub_count);
				if (sub)
					json_array_add_item(arr, sub);
			}
			cJSON_AddItemToObject(json, f->key, arr);
			break;
		}
		case CFG_TYPE_STR_ARRAY: {
			int cnt = *(const int *)((const char *)data + f->count_offset);
			json_t arr = json_create_array();
			int j;

			for (j = 0; j < cnt && j < f->max_items; j++) {
				const char *str =
					(const char *)ptr + (size_t)j * f->size;
				json_array_add_string(arr, str);
			}
			cJSON_AddItemToObject(json, f->key, arr);
			break;
		}
		case CFG_TYPE_INT_ARRAY: {
			int cnt = *(const int *)((const char *)data + f->count_offset);
			json_t arr = json_create_array();
			int j;

			for (j = 0; j < cnt && j < f->max_items; j++)
				json_array_add_int(arr, ((const int *)ptr)[j]);
			cJSON_AddItemToObject(json, f->key, arr);
			break;
		}
		}
	}
	return json;
}

int cfg_json_types_valid(json_t json, const cfg_field_t *fields, int count)
{
	int i;

	if (!json || !fields || !cJSON_IsObject(json))
		return 0;
	for (i = 0; i < count; ++i) {
		const cfg_field_t *f = &fields[i];
		cJSON *item = cJSON_GetObjectItem(json, f->key);
		int j;

		if (!item)
			continue;
		switch (f->type) {
		case CFG_TYPE_INT:
		case CFG_TYPE_BOOL:
			if (!cJSON_IsNumber(item) && !cJSON_IsBool(item))
				return 0;
			break;
		case CFG_TYPE_FLOAT:
			if (!cJSON_IsNumber(item))
				return 0;
			break;
		case CFG_TYPE_STRING:
			if (!cJSON_IsString(item) || !item->valuestring)
				return 0;
			break;
		case CFG_TYPE_OBJECT:
			if (!cJSON_IsObject(item) ||
			    !cfg_json_types_valid(item, f->sub_fields,
						  f->sub_count))
				return 0;
			break;
		case CFG_TYPE_OBJ_ARRAY:
			if (!cJSON_IsArray(item) ||
			    cJSON_GetArraySize(item) > f->max_items)
				return 0;
			for (j = 0; j < cJSON_GetArraySize(item); ++j) {
				cJSON *element = cJSON_GetArrayItem(item, j);
				if (!cfg_json_types_valid(element, f->sub_fields,
							  f->sub_count))
					return 0;
			}
			break;
		case CFG_TYPE_STR_ARRAY:
			if (!cJSON_IsArray(item) ||
			    cJSON_GetArraySize(item) > f->max_items)
				return 0;
			for (j = 0; j < cJSON_GetArraySize(item); ++j)
				if (!cJSON_IsString(
					    cJSON_GetArrayItem(item, j)))
					return 0;
			break;
		case CFG_TYPE_INT_ARRAY:
			if (!cJSON_IsArray(item) ||
			    cJSON_GetArraySize(item) > f->max_items)
				return 0;
			for (j = 0; j < cJSON_GetArraySize(item); ++j)
				if (!cJSON_IsNumber(
					    cJSON_GetArrayItem(item, j)))
					return 0;
			break;
		}
	}
	return 1;
}

int cfg_from_json(json_t json, void *data, const cfg_field_t *fields, int count)
{
	int i;

	if (!json || !data || !fields)
		return -1;

	for (i = 0; i < count; i++) {
		const cfg_field_t *f = &fields[i];
		void *ptr = (char *)data + f->offset;
		cJSON *item = cJSON_GetObjectItem(json, f->key);

		if (!item)
			continue;

		switch (f->type) {
		case CFG_TYPE_INT:
			if (cJSON_IsNumber(item))
				*(int *)ptr = item->valueint;
			else if (cJSON_IsBool(item))
				*(int *)ptr = cJSON_IsTrue(item) ? 1 : 0;
			break;
		case CFG_TYPE_FLOAT:
			if (cJSON_IsNumber(item))
				*(float *)ptr = (float)item->valuedouble;
			break;
		case CFG_TYPE_STRING:
			if (cJSON_IsString(item) && item->valuestring) {
				strncpy((char *)ptr, item->valuestring, f->size - 1);
				((char *)ptr)[f->size - 1] = '\0';
			}
			break;
		case CFG_TYPE_BOOL:
			if (cJSON_IsBool(item))
				*(int *)ptr = cJSON_IsTrue(item) ? 1 : 0;
			else if (cJSON_IsNumber(item))
				*(int *)ptr = item->valueint ? 1 : 0;
			break;
		case CFG_TYPE_OBJECT:
			if (cJSON_IsObject(item))
				cfg_from_json(item, ptr, f->sub_fields, f->sub_count);
			break;
		case CFG_TYPE_OBJ_ARRAY:
			if (cJSON_IsArray(item)) {
				int arr_sz = cJSON_GetArraySize(item);
				int j;

				if (arr_sz > f->max_items)
					arr_sz = f->max_items;
				*(int *)((char *)data + f->count_offset) = arr_sz;
				for (j = 0; j < arr_sz; j++) {
					cJSON *el = cJSON_GetArrayItem(item, j);
					void *dest =
						(char *)ptr + (size_t)j * f->item_size;
					if (el)
						cfg_from_json(el, dest, f->sub_fields,
							      f->sub_count);
				}
			}
			break;
		case CFG_TYPE_STR_ARRAY:
			if (cJSON_IsArray(item)) {
				int arr_sz = cJSON_GetArraySize(item);
				int j;

				if (arr_sz > f->max_items)
					arr_sz = f->max_items;
				*(int *)((char *)data + f->count_offset) = arr_sz;
				for (j = 0; j < arr_sz; j++) {
					cJSON *el = cJSON_GetArrayItem(item, j);
					if (el && cJSON_IsString(el) &&
					    el->valuestring) {
						char *dest =
							(char *)ptr + (size_t)j * f->size;
						strncpy(dest, el->valuestring,
							f->size - 1);
						dest[f->size - 1] = '\0';
					}
				}
			}
			break;
		case CFG_TYPE_INT_ARRAY:
			if (cJSON_IsArray(item)) {
				int arr_sz = cJSON_GetArraySize(item);
				int j;

				if (arr_sz > f->max_items)
					arr_sz = f->max_items;
				*(int *)((char *)data + f->count_offset) = arr_sz;
				for (j = 0; j < arr_sz; j++) {
					cJSON *el = cJSON_GetArrayItem(item, j);
					if (el && cJSON_IsNumber(el))
						((int *)ptr)[j] = el->valueint;
				}
			}
			break;
		}
	}
	return 0;
}

json_t cfg_array_to_json(const void *array, int item_count, size_t item_size,
			 const cfg_field_t *fields, int field_count)
{
	int i;
	json_t arr;

	if (!array || !fields)
		return NULL;

	arr = json_create_array();
	if (!arr)
		return NULL;

	for (i = 0; i < item_count; i++) {
		const void *item = (const char *)array + (size_t)i * item_size;
		json_t obj = cfg_to_json(item, fields, field_count);
		if (obj)
			json_array_add_item(arr, obj);
	}
	return arr;
}

int cfg_array_from_json(json_t json_arr, void *array, int max_count,
			size_t item_size, const cfg_field_t *fields,
			int field_count, int *out_count)
{
	int arr_sz;
	int i;

	if (!json_arr || !array || !fields || !cJSON_IsArray(json_arr))
		return -1;

	arr_sz = cJSON_GetArraySize(json_arr);
	if (arr_sz > max_count)
		arr_sz = max_count;

	for (i = 0; i < arr_sz; i++) {
		cJSON *el = cJSON_GetArrayItem(json_arr, i);
		void *dest = (char *)array + (size_t)i * item_size;
		if (el)
			cfg_from_json(el, dest, fields, field_count);
	}

	if (out_count)
		*out_count = arr_sz;
	return 0;
}

int cfg_load_file(const char *path, void *data, const cfg_field_t *fields,
		  int count)
{
	json_t json = NULL;

	if (!file_exists(path)) {
		LOGD("Config not found: %s, using defaults", path);
		return -1;
	}

	if (json_load_file(path, &json) != 0) {
		LOGW("Failed to load: %s", path);
		return -1;
	}

	if (cfg_from_json(json, data, fields, count) == 0)
		LOGD("Loaded config: %s", path);
	json_free(json);
	return 0;
}

int cfg_save_file(const char *path, const void *data, const cfg_field_t *fields,
		  int count)
{
	json_t json = cfg_to_json(data, fields, count);
	int ret;

	if (!json) {
		LOGW("Failed to serialize for: %s", path);
		return -1;
	}

	if (file_ensure_dir(path) != 0) {
		LOGW("Failed to create directory for: %s", path);
		json_free(json);
		return -1;
	}

	ret = json_save_file(path, json);
	json_free(json);

	if (ret == 0)
		LOGD("Saved config: %s", path);
	else
		LOGW("Failed to save: %s", path);
	return ret;
}
