#ifndef NET_APP_RTC_UTIL_H
#define NET_APP_RTC_UTIL_H

#include <time.h>

#define RTC_DEVICE "/dev/rtc0"

/* 0 if this board has no /dev/rtc0 (Parkia Cube). */
int rtc_is_present(void);

int rtc_read(time_t *utc);
int rtc_write(time_t utc);
int rtc_sync_system_time(void);

#endif
