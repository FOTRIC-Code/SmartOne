#ifndef NET_APP_LIBS_JSON_UTIL_H
#define NET_APP_LIBS_JSON_UTIL_H

#include "cJSON.h"
#include <stddef.h>

typedef cJSON *json_t;

static inline void json_free(json_t j)
{
	if (j)
		cJSON_Delete(j);
}

static inline const char *json_get_string(json_t root, const char *key,
					 const char *fallback)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
	return cJSON_IsString(item) && item->valuestring ? item->valuestring :
							   fallback;
}

static inline int json_get_int(json_t root, const char *key, int fallback)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
	return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static inline int json_get_bool(json_t root, const char *key, int fallback)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
	return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}

static inline double json_get_double(json_t root, const char *key,
				     double fallback)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
	return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

char *json_to_string(json_t root);
int json_write_file(const char *path, json_t root);
char *json_read_file(const char *path);

json_t json_create_object(void);
json_t json_create_array(void);
int json_set_int(json_t json, const char *key, int val);
int json_set_float(json_t json, const char *key, float val);
int json_set_string(json_t json, const char *key, const char *val);
int json_set_bool(json_t json, const char *key, int val);
int json_array_add_item(json_t array, json_t item);
int json_array_add_int(json_t array, int val);
int json_array_add_string(json_t array, const char *val);
int json_load_file(const char *path, json_t *out_json);
int json_save_file(const char *path, json_t json);

#endif
