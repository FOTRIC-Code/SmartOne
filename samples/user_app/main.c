#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signo)
{
	(void)signo;
	g_running = 0;
}

static void write_log(const char *fmt, ...)
{
	FILE *fp;
	va_list ap;
	time_t now;
	struct tm tm_now;
	char ts[32];

	now = time(NULL);
	localtime_r(&now, &tm_now);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

	fp = fopen(APP_LOG_PATH, "a");
	if (!fp)
		return;

	fprintf(fp, "[%s] ", ts);
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	fputc('\n', fp);
	fclose(fp);
}

int main(int argc, char **argv)
{
	int heartbeat = 0;

	(void)argc;
	(void)argv;

	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	write_log("user_app started, pid=%ld", (long)getpid());

	while (g_running) {
		if ((heartbeat % 5) == 0)
			write_log("heartbeat=%d", heartbeat / 5);

		sleep(1);
		heartbeat++;
	}

	write_log("user_app stopping");
	return 0;
}
