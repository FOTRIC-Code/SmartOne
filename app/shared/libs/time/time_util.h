#ifndef PARKIA_LIBS_TIME_UTIL_H
#define PARKIA_LIBS_TIME_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define TIME_FMT_DATETIME "%Y-%m-%d %H:%M:%S"
#define TIME_FMT_LOG "%Y-%m-%d %H:%M:%S%z"
#define TIME_FMT_COMPACT "%Y%m%d%H%M%S"
#define TIME_FMT_FILE "%Y%m%d_%H%M%S"

uint64_t time_now_us(void);
time_t time_now_sec(void);

int time_format_local_sec(time_t sec, const char *fmt, char *buf, size_t size);
int time_format_local_us(uint64_t us, const char *fmt, char *buf, size_t size);
int time_format_now(const char *fmt, char *buf, size_t size);
unsigned time_millis_of_us(uint64_t us);

/* Human zone like "UTC+08:00". Always applied so wall-clock strings are local. */
void time_tz_set_utc(void);
void time_tz_set(const char *human_tz);
void time_tz_apply(const char *human_tz);

/* Parse TIME_FMT_DATETIME as local wall clock in the current TZ → UTC epoch. */
int time_parse_datetime(const char *datetime, time_t *utc);
int time_set_epoch(time_t utc);
int time_set_datetime(const char *datetime, time_t *utc);

#endif
