#ifndef NET_APP_BODY_TEMP_HISTORY_H
#define NET_APP_BODY_TEMP_HISTORY_H

#include <stdint.h>

#define BODY_TEMP_HISTORY_CAPACITY 10
#define BODY_TEMP_MINUTE_CAPACITY 60
#define BODY_TEMP_MINUTE_MIN_VALID 20

typedef struct body_temp_minute {
	float readings[BODY_TEMP_MINUTE_CAPACITY];
	int count;
} body_temp_minute_t;

typedef struct body_temp_sample {
	float temperature;
	uint64_t timestamp_us;
} body_temp_sample_t;

typedef struct body_temp_history {
	body_temp_sample_t samples[BODY_TEMP_HISTORY_CAPACITY];
	int count;
} body_temp_history_t;

void body_temp_minute_reset(body_temp_minute_t *minute);
void body_temp_minute_add(body_temp_minute_t *minute, float temperature);
int body_temp_minute_median(const body_temp_minute_t *minute, float *median);

void body_temp_history_reset(body_temp_history_t *history);
void body_temp_history_add(body_temp_history_t *history, float temperature,
			   uint64_t timestamp_us);
int body_temp_history_trend(const body_temp_history_t *history,
			    float *baseline, float *slope_per_min);

#endif
