#include "body_temp_history.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BODY_TEMP_BASELINE_SAMPLES 3
#define BODY_TEMP_PAIR_CAPACITY \
	(BODY_TEMP_HISTORY_CAPACITY * (BODY_TEMP_HISTORY_CAPACITY - 1) / 2)

static int compare_float(const void *a, const void *b)
{
	float fa = *(const float *)a;
	float fb = *(const float *)b;

	return (fa > fb) - (fa < fb);
}

/* Sorts values in place. */
static float median_in_place(float *values, int count)
{
	qsort(values, (size_t)count, sizeof(*values), compare_float);
	if (count % 2)
		return values[count / 2];
	return (values[count / 2 - 1] + values[count / 2]) * 0.5f;
}

void body_temp_minute_reset(body_temp_minute_t *minute)
{
	if (minute)
		memset(minute, 0, sizeof(*minute));
}

void body_temp_minute_add(body_temp_minute_t *minute, float temperature)
{
	if (!minute || !isfinite(temperature) ||
	    minute->count >= BODY_TEMP_MINUTE_CAPACITY)
		return;
	minute->readings[minute->count++] = temperature;
}

int body_temp_minute_median(const body_temp_minute_t *minute, float *median)
{
	float sorted[BODY_TEMP_MINUTE_CAPACITY];

	if (!minute || !median || minute->count < BODY_TEMP_MINUTE_MIN_VALID)
		return -1;
	memcpy(sorted, minute->readings, sizeof(float) * (size_t)minute->count);
	*median = median_in_place(sorted, minute->count);
	return 0;
}

/*
 * Discrete monitoring rounds: samples are appended until the window is full
 * (one round of BODY_TEMP_HISTORY_CAPACITY minutes), then the caller
 * evaluates the round and resets for the next one. No sliding overlap.
 */

void body_temp_history_reset(body_temp_history_t *history)
{
	if (history)
		memset(history, 0, sizeof(*history));
}

void body_temp_history_add(body_temp_history_t *history, float temperature,
			   uint64_t timestamp_us)
{
	if (!history || history->count >= BODY_TEMP_HISTORY_CAPACITY)
		return;
	history->samples[history->count].temperature = temperature;
	history->samples[history->count].timestamp_us = timestamp_us;
	history->count++;
}

/*
 * Theil-Sen estimator: the slope is the median of all pairwise slopes, so up
 * to ~29% outlier samples cannot move it. The baseline is the median of the
 * first few samples for the same reason.
 */
int body_temp_history_trend(const body_temp_history_t *history,
			    float *baseline, float *slope_per_min)
{
	float slopes[BODY_TEMP_PAIR_CAPACITY];
	float head[BODY_TEMP_BASELINE_SAMPLES];
	int pairs = 0;
	int i, j;

	if (!history || !baseline || !slope_per_min ||
	    history->count < BODY_TEMP_HISTORY_CAPACITY)
		return -1;

	for (i = 0; i < history->count; ++i) {
		for (j = i + 1; j < history->count; ++j) {
			const body_temp_sample_t *a = &history->samples[i];
			const body_temp_sample_t *b = &history->samples[j];
			double dx;

			if (b->timestamp_us <= a->timestamp_us)
				continue;
			dx = (double)(b->timestamp_us - a->timestamp_us) /
			     60000000.0;
			slopes[pairs++] =
				(float)((b->temperature - a->temperature) / dx);
		}
	}
	if (pairs == 0)
		return -1;

	for (i = 0; i < BODY_TEMP_BASELINE_SAMPLES; ++i)
		head[i] = history->samples[i].temperature;
	*baseline = median_in_place(head, BODY_TEMP_BASELINE_SAMPLES);
	*slope_per_min = median_in_place(slopes, pairs);
	return 0;
}
