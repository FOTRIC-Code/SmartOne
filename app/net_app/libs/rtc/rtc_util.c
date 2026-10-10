#include "rtc_util.h"

#include "libs/log/log.h"
#include "libs/time/time_util.h"

#include <fcntl.h>
#include <linux/rtc.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int s_probed;
static int s_present;

int rtc_is_present(void)
{
	if (s_probed)
		return s_present;

	s_probed = 1;
	s_present = access(RTC_DEVICE, F_OK) == 0;
	if (!s_present)
		LOGI("RTC not present (%s); time is RAM-only until NTP or next set",
		     RTC_DEVICE);
	return s_present;
}

int rtc_read(time_t *utc)
{
	struct rtc_time rtc_tm;
	struct tm tm_val;
	int fd;
	int ret;

	if (!utc || !rtc_is_present())
		return -1;

	fd = open(RTC_DEVICE, O_RDONLY);
	if (fd < 0) {
		s_present = 0;
		return -1;
	}

	memset(&rtc_tm, 0, sizeof(rtc_tm));
	ret = ioctl(fd, RTC_RD_TIME, &rtc_tm);
	close(fd);
	if (ret < 0) {
		LOGW("RTC_RD_TIME ioctl failed");
		return -1;
	}

	memset(&tm_val, 0, sizeof(tm_val));
	tm_val.tm_sec = rtc_tm.tm_sec;
	tm_val.tm_min = rtc_tm.tm_min;
	tm_val.tm_hour = rtc_tm.tm_hour;
	tm_val.tm_mday = rtc_tm.tm_mday;
	tm_val.tm_mon = rtc_tm.tm_mon;
	tm_val.tm_year = rtc_tm.tm_year;
	*utc = timegm(&tm_val);
	return 0;
}

int rtc_write(time_t utc)
{
	struct tm tm_val;
	struct rtc_time rtc_tm;
	int fd;
	int ret;

	if (!rtc_is_present())
		return 0;

	fd = open(RTC_DEVICE, O_RDWR);
	if (fd < 0) {
		s_present = 0;
		return 0;
	}

	gmtime_r(&utc, &tm_val);
	memset(&rtc_tm, 0, sizeof(rtc_tm));
	rtc_tm.tm_sec = tm_val.tm_sec;
	rtc_tm.tm_min = tm_val.tm_min;
	rtc_tm.tm_hour = tm_val.tm_hour;
	rtc_tm.tm_mday = tm_val.tm_mday;
	rtc_tm.tm_mon = tm_val.tm_mon;
	rtc_tm.tm_year = tm_val.tm_year;
	rtc_tm.tm_wday = tm_val.tm_wday;
	rtc_tm.tm_yday = tm_val.tm_yday;

	ret = ioctl(fd, RTC_SET_TIME, &rtc_tm);
	close(fd);
	if (ret < 0) {
		LOGW("RTC_SET_TIME ioctl failed");
		return -1;
	}

	LOGI("RTC hardware clock updated");
	return 0;
}

int rtc_sync_system_time(void)
{
	time_t rtc_utc;

	if (!rtc_is_present())
		return -1;
	if (rtc_read(&rtc_utc) != 0)
		return -1;
	if (time_set_epoch(rtc_utc) != 0)
		return -1;
	LOGI("System time set from RTC: %ld", (long)rtc_utc);
	return 0;
}
