#include "led/ble_led.h"

#include "libs/log/log.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LED_SYSFS_PATH "/sys/class/leds/work-led"
#define LED_BRIGHTNESS LED_SYSFS_PATH "/brightness"
#define LED_TRIGGER LED_SYSFS_PATH "/trigger"

static pthread_t s_blink_thread;
static volatile bool s_thread_running;
static volatile ble_led_state_t s_current_state;
static volatile bool s_state_changed;
static pthread_mutex_t s_led_mutex = PTHREAD_MUTEX_INITIALIZER;

static int write_sysfs(const char *path, const char *value)
{
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		return -1;
	write(fd, value, strlen(value));
	close(fd);
	return 0;
}

static void led_on(void)
{
	write_sysfs(LED_TRIGGER, "none");
	write_sysfs(LED_BRIGHTNESS, "1");
}

static void led_off(void)
{
	write_sysfs(LED_TRIGGER, "none");
	write_sysfs(LED_BRIGHTNESS, "0");
}

static bool interruptible_sleep_ms(int ms)
{
	while (ms > 0 && s_thread_running && !s_state_changed) {
		int chunk = ms > 50 ? 50 : ms;
		usleep(chunk * 1000);
		ms -= chunk;
	}
	return s_thread_running && !s_state_changed;
}

static void blink_loop(int on_ms, int off_ms)
{
	while (s_thread_running && !s_state_changed) {
		write_sysfs(LED_BRIGHTNESS, "1");
		if (!interruptible_sleep_ms(on_ms))
			return;
		write_sysfs(LED_BRIGHTNESS, "0");
		if (!interruptible_sleep_ms(off_ms))
			return;
	}
}

static void blink_n_loop(int n, int on_ms, int off_ms, int pause_ms)
{
	while (s_thread_running && !s_state_changed) {
		int i;
		write_sysfs(LED_TRIGGER, "none");
		for (i = 0; i < n; i++) {
			if (!s_thread_running || s_state_changed)
				return;
			write_sysfs(LED_BRIGHTNESS, "1");
			if (!interruptible_sleep_ms(on_ms))
				return;
			write_sysfs(LED_BRIGHTNESS, "0");
			if (i < n - 1) {
				if (!interruptible_sleep_ms(off_ms))
					return;
			}
		}
		if (!interruptible_sleep_ms(pause_ms))
			return;
	}
}

static void *led_thread_func(void *arg)
{
	(void)arg;

	while (s_thread_running) {
		ble_led_state_t state;

		pthread_mutex_lock(&s_led_mutex);
		state = s_current_state;
		s_state_changed = false;
		pthread_mutex_unlock(&s_led_mutex);

		write_sysfs(LED_TRIGGER, "none");

		switch (state) {
		case BLE_LED_WAIT_PROVISION:
			blink_loop(1000, 1000);
			break;

		case BLE_LED_PROVISIONING:
			blink_loop(200, 200);
			break;

		case BLE_LED_ONLINE:
			led_on();
			while (s_thread_running && !s_state_changed)
				usleep(100 * 1000);
			break;

		case BLE_LED_OFFLINE:
			blink_n_loop(2, 150, 150, 2000);
			break;

		case BLE_LED_FAILED:
			blink_n_loop(3, 150, 150, 2000);
			break;
		}
	}

	led_off();
	return NULL;
}

int ble_led_init(void)
{
	if (access(LED_SYSFS_PATH, F_OK) != 0) {
		LOGW("LED: %s not found, LED control disabled",
		     LED_SYSFS_PATH);
		return -1;
	}

	s_current_state = BLE_LED_WAIT_PROVISION;
	s_state_changed = false;
	s_thread_running = true;

	if (pthread_create(&s_blink_thread, NULL, led_thread_func, NULL) != 0) {
		LOGE("LED: thread create failed");
		s_thread_running = false;
		return -1;
	}

	LOGI("LED: initialized at %s", LED_SYSFS_PATH);
	return 0;
}

void ble_led_deinit(void)
{
	if (!s_thread_running)
		return;

	s_thread_running = false;
	s_state_changed = true;
	pthread_join(s_blink_thread, NULL);
	led_off();
	LOGI("LED: deinitialized");
}

int ble_led_set_state(ble_led_state_t state)
{
	static const char *state_names[] = {
		"wait_provision", "provisioning", "online", "offline", "failed"
	};

	LOGI("LED state: %s", state_names[state]);

	pthread_mutex_lock(&s_led_mutex);
	if (s_current_state != state) {
		s_current_state = state;
		s_state_changed = true;
	}
	pthread_mutex_unlock(&s_led_mutex);

	return 0;
}
