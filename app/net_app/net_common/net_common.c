#include "net_common.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

net_app_config_t g_config;
system_status_t g_system_status;

static void set_str(char *dst, size_t size, const char *src)
{
	if (!dst || size == 0)
		return;
	snprintf(dst, size, "%s", src ? src : "");
}

/*
 * Two units within radio range must not advertise the same SSID, otherwise the
 * user cannot tell which box they are provisioning. The serial tail is short
 * enough to stay readable on a phone's Wi-Fi list and is printed on the label.
 */
void net_common_build_default_hotspot_ssid(char *dst, size_t size,
					   const char *serial_no)
{
	size_t serial_len;

	if (!dst || size == 0)
		return;

	if (!serial_no || serial_no[0] == '\0') {
		snprintf(dst, size, "%s", NET_DEFAULT_DEVICE_NAME);
		return;
	}

	serial_len = strlen(serial_no);
	snprintf(dst, size, "%s-%s", NET_DEFAULT_DEVICE_NAME,
		 serial_len > 4 ? serial_no + serial_len - 4 : serial_no);
}

void net_common_set_default_hotspot_ssid(provision_info_t *provision,
					 const char *serial_no)
{
	char hotspot_ssid[sizeof(provision->hotspot_ssid)];

	if (!provision)
		return;

	net_common_build_default_hotspot_ssid(hotspot_ssid, sizeof(hotspot_ssid),
					      serial_no);
	set_str(provision->hotspot_ssid, sizeof(provision->hotspot_ssid),
		hotspot_ssid);
}

void net_common_set_runtime_defaults(void)
{
	memset(&g_system_status, 0, sizeof(g_system_status));

	g_config.stream_count = 1;
	g_config.streams[0].id = 101;
	set_str(g_config.streams[0].codec_type,
		sizeof(g_config.streams[0].codec_type), "h264");
	g_config.streams[0].bit_rate = 400000;
	g_config.streams[0].width = 640;
	/* Stream at 25fps; GOP=6 ≈ 0.24s for low live latency. */
	g_config.streams[0].max_frame_rate = 25;
	g_config.streams[0].gop_size = 6;
	g_config.streams[0].h264_profile = 1;
	set_str(g_config.streams[0].quality_control_type,
		sizeof(g_config.streams[0].quality_control_type), "MDQ");
}

int net_record_dir_ready(void)
{
	return access(NET_ALARM_RECORD_READY_FLAG, F_OK) == 0;
}

