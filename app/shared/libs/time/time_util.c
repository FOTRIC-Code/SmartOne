#define _GNU_SOURCE

#include "time_util.h"

#include "libs/log/log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

uint64_t time_now_us(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

time_t time_now_sec(void)
{
	return (time_t)(time_now_us() / 1000000ULL);
}

int time_format_local_sec(time_t sec, const char *fmt, char *buf, size_t size)
{
	struct tm tm_buf;
	size_t n;

	if (!fmt || !buf || size == 0)
		return -1;
	if (!localtime_r(&sec, &tm_buf))
		return -1;
	n = strftime(buf, size, fmt, &tm_buf);
	return n > 0 ? (int)n : -1;
}

int time_format_local_us(uint64_t us, const char *fmt, char *buf, size_t size)
{
	return time_format_local_sec((time_t)(us / 1000000ULL), fmt, buf, size);
}

int time_format_now(const char *fmt, char *buf, size_t size)
{
	return time_format_local_us(time_now_us(), fmt, buf, size);
}

unsigned time_millis_of_us(uint64_t us)
{
	return (unsigned)((us / 1000ULL) % 1000ULL);
}

void time_tz_set_utc(void)
{
	setenv("TZ", "UTC", 1);
	tzset();
	LOGI("Timezone: UTC (no configured zone)");
}

void time_tz_set(const char *human_tz)
{
	char posix_tz[64];
	char *sign;

	if (!human_tz || human_tz[0] == '\0')
		return;

	/*
	 * POSIX TZ sign is inverted vs common convention:
	 *   "UTC+8" means UTC minus 8 (west), "UTC-8" means UTC plus 8 (east)
	 * Config stores human-readable form like "UTC+08:00".
	 */
	snprintf(posix_tz, sizeof(posix_tz), "%s", human_tz);
	sign = strchr(posix_tz, '+');
	if (sign) {
		*sign = '-';
	} else {
		sign = strchr(posix_tz, '-');
		if (sign && sign != posix_tz)
			*sign = '+';
	}

	setenv("TZ", posix_tz, 1);
	tzset();
	LOGI("Timezone config: %s -> POSIX TZ: %s", human_tz, posix_tz);
}

void time_tz_apply(const char *human_tz)
{
	if (human_tz && human_tz[0] != '\0')
		time_tz_set(human_tz);
	else
		time_tz_set_utc();
}

int time_parse_datetime(const char *datetime, time_t *utc)
{
	struct tm tm_val;
	char *parsed;

	if (!datetime || datetime[0] == '\0' || !utc)
		return -1;

	memset(&tm_val, 0, sizeof(tm_val));
	tm_val.tm_isdst = -1;
	parsed = strptime(datetime, TIME_FMT_DATETIME, &tm_val);
	LOGI("Parsed datetime %s: %s", datetime, parsed ? "ok" : "failed");
	if (!parsed)
		return -1;
	*utc = mktime(&tm_val);
	if (*utc == (time_t)-1)
		return -1;
	return 0;
}

int time_set_epoch(time_t utc)
{
	struct timeval tv = { .tv_sec = utc, .tv_usec = 0 };

	if (settimeofday(&tv, NULL) != 0) {
		LOGW("Failed to set system time: %s", strerror(errno));
		return -1;
	}
	return 0;
}

int time_set_datetime(const char *datetime, time_t *utc)
{
	time_t parsed;

	if (time_parse_datetime(datetime, &parsed) != 0)
		return -1;
	if (time_set_epoch(parsed) != 0)
		return -1;
	LOGI("System time set to: %s", datetime);
	if (utc)
		*utc = parsed;
	return 0;
}
