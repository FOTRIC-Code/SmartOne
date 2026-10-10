#ifndef NET_APP_LIBS_JSON_BIND_H
#define NET_APP_LIBS_JSON_BIND_H

#include "json_util.h"
#include <stddef.h>

typedef enum {
	CFG_TYPE_INT,
	CFG_TYPE_FLOAT,
	CFG_TYPE_STRING,
	CFG_TYPE_BOOL,
	CFG_TYPE_OBJECT,
	CFG_TYPE_OBJ_ARRAY,
	CFG_TYPE_STR_ARRAY,
	CFG_TYPE_INT_ARRAY,
} cfg_type_t;

typedef struct cfg_field {
	const char *key;
	cfg_type_t type;
	size_t offset;
	size_t size;
	const struct cfg_field *sub_fields;
	int sub_count;
	size_t item_size;
	size_t count_offset;
	int max_items;
} cfg_field_t;

#define CFG_FIELD_COUNT(arr) (int)(sizeof(arr) / sizeof((arr)[0]))

#define CFG_INT(st, f, k) \
	{ (k), CFG_TYPE_INT, offsetof(st, f), sizeof(int), NULL, 0, 0, 0, 0 }

#define CFG_FLOAT(st, f, k) \
	{ (k), CFG_TYPE_FLOAT, offsetof(st, f), sizeof(float), NULL, 0, 0, 0, 0 }

#define CFG_DOUBLE(st, f, k) CFG_FLOAT(st, f, k)

#define CFG_STRING(st, f, k) \
	{ (k), CFG_TYPE_STRING, offsetof(st, f), sizeof(((st *)0)->f), NULL, 0, 0, 0, 0 }

#define CFG_BOOL(st, f, k) \
	{ (k), CFG_TYPE_BOOL, offsetof(st, f), sizeof(int), NULL, 0, 0, 0, 0 }

#define CFG_OBJECT(st, f, k, sub, sub_cnt) \
	{ (k), CFG_TYPE_OBJECT, offsetof(st, f), sizeof(((st *)0)->f), (sub), \
	  (sub_cnt), 0, 0, 0 }

#define CFG_OBJ_ARRAY(st, arr_f, cnt_f, k, item_t, sub, sub_cnt, max) \
	{ (k), CFG_TYPE_OBJ_ARRAY, offsetof(st, arr_f), sizeof(item_t), (sub), \
	  (sub_cnt), sizeof(item_t), offsetof(st, cnt_f), (max) }

#define CFG_STR_ARRAY(st, arr_f, cnt_f, k, max) \
	{ (k), CFG_TYPE_STR_ARRAY, offsetof(st, arr_f), \
	  sizeof(((st *)0)->arr_f[0]), NULL, 0, sizeof(((st *)0)->arr_f[0]), \
	  offsetof(st, cnt_f), (max) }

#define CFG_INT_ARRAY(st, arr_f, cnt_f, k, max) \
	{ (k), CFG_TYPE_INT_ARRAY, offsetof(st, arr_f), sizeof(int), NULL, 0, \
	  sizeof(int), offsetof(st, cnt_f), (max) }

json_t cfg_to_json(const void *data, const cfg_field_t *fields, int count);
int cfg_from_json(json_t json, void *data, const cfg_field_t *fields, int count);
int cfg_json_types_valid(json_t json, const cfg_field_t *fields, int count);
int cfg_load_file(const char *path, void *data, const cfg_field_t *fields,
		  int count);
int cfg_save_file(const char *path, const void *data,
		  const cfg_field_t *fields, int count);
json_t cfg_array_to_json(const void *array, int item_count, size_t item_size,
			 const cfg_field_t *fields, int field_count);
int cfg_array_from_json(json_t json_arr, void *array, int max_count,
			size_t item_size, const cfg_field_t *fields,
			int field_count, int *out_count);

#endif
