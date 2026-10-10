#include "alarm/alarm_monitor.h"
#include "thermal/thermal_data_provider.h"
#include "http_api/http_api.h"
#include "led/led_manager.h"
#include "libs/log/log.h"
#include "main_process/main_process.h"
#include "net_common/net_common.h"
#include "rtp/rtp_server.h"
#include "video_process/video_process.h"

#include <signal.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signo)
{
	(void)signo;
	g_running = 0;
	g_system_status.exit_flag = 1;
}

int main(void)
{
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	log_init();
	LOGI("%s %s starting", NET_APP_NAME, NET_APP_VERSION);

	if (config_init() != 0) {
		LOGE("config_init failed");
		return 1;
	}

	{
		int retries = 10;
		int port = g_config.provision.http_port_no;

		while (http_api_start(port) != 0 && --retries > 0) {
			LOGW("http_api_start retry in 1s (%d left)", retries);
			sleep(1);
		}
		if (retries <= 0) {
			LOGE("http_api_start failed after retries");
			config_uninit();
			return 1;
		}
	}

	main_process_init();
	if (thermal_provider_start() != 0)
		LOGW("Thermal UVC provider start failed, alarms will use stale data");

	if (video_process_start() != 0)
		LOGW("Video process start failed, streaming unavailable");

	if (rtp_server_start(g_config.port_info.rtp_port_no) != 0)
		LOGW("RTP server start failed, alarm push unavailable");
	if (alarm_monitor_start() != 0)
		LOGW("Alarm monitor start failed");

	while (g_running && !g_system_status.exit_flag)
		usleep(100 * 1000);

	alarm_monitor_stop();
	main_process_uninit();
	video_process_stop();
	thermal_provider_stop();
	rtp_server_stop();
	http_api_stop();
	net_led_deinit();
	config_uninit();
	log_close();
	LOGI("%s stopping", NET_APP_NAME);
	return 0;
}
