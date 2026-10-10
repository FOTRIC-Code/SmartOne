#include "log.h"
#include "libs/time/time_util.h"
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef APP_DEFAULT_LOG_PATH
#define APP_DEFAULT_LOG_PATH "/tmp/app.log"
#endif

#ifndef APP_LOG_NAME
#define APP_LOG_NAME "app"
#endif

static char log_path[128] = APP_DEFAULT_LOG_PATH;
static int  log_max_size  = 32 * 1024;
static int  log_enabled   = 1;

static FILE *log_fp = NULL;
static FILE *console_fp = NULL;
static int console_resolved = 0;
static log_level_t log_cur_level = LOG_LEVEL_DEBUG;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *level_str[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

static void log_rotate(void)
{
    struct stat st;

    if (stat(log_path, &st) != 0)
        return;

    if (st.st_size < log_max_size)
        return;

    fclose(log_fp);

    char bak[256];
    snprintf(bak, sizeof(bak), "%s.bak", log_path);

    rename(log_path, bak);

    log_fp = fopen(log_path, "w");
}

void log_init(void)
{
	log_apply_config(1, LOG_LEVEL_DEBUG, log_max_size, log_path);
}

void log_apply_config(int enabled, int level, int max_file_size, const char *file_path)
{
    pthread_mutex_lock(&log_mutex);

    log_enabled = enabled;
    log_cur_level = (log_level_t)level;
    log_max_size = max_file_size;

    if (file_path && file_path[0] != '\0') {
        strncpy(log_path, file_path, sizeof(log_path) - 1);
        log_path[sizeof(log_path) - 1] = '\0';
    }

    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }

    if (log_enabled) {
        log_fp = fopen(log_path, "a+");
        if (!log_fp)
            printf("open log file fail: %s\n", log_path);
    }

    pthread_mutex_unlock(&log_mutex);
}

void log_close(void)
{
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

void log_set_level(log_level_t level)
{
    log_cur_level = level;
}

static const char *extract_filename(const char *path)
{
    const char *name = strrchr(path, '/');
    return name ? name + 1 : path;
}

/*
 * main_app launches us from a backgrounded daemon, so stdout is no longer the
 * serial port. Mirror to /dev/console in that case; when stdout already is a
 * tty the printf below is the same console and must not be duplicated.
 */
static void console_open_once(void)
{
    if (console_resolved)
        return;

    console_resolved = 1;
    if (isatty(fileno(stdout)))
        return;

    console_fp = fopen("/dev/console", "w");
}

void log_write(log_level_t level, const char *file, const char *func, int line, const char *fmt, ...)
{
    if (!log_enabled)
        return;

    if (level < log_cur_level)
        return;

    pthread_mutex_lock(&log_mutex);

    if (!log_fp) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    log_rotate();

    char timebuf[64];

    time_format_now(TIME_FMT_LOG, timebuf, sizeof(timebuf));

    const char *filename = extract_filename(file);
    char filebuf[64];
    strncpy(filebuf, filename, sizeof(filebuf) - 1);
    filebuf[sizeof(filebuf) - 1] = '\0';
    char *dot = strrchr(filebuf, '.');
    if (dot) *dot = '\0';

    console_open_once();

    /* date [level] [app] [file] [func] message */
    fprintf(log_fp, "%s [%s] [%s] [%s] [%s] ", timebuf, level_str[level],
            APP_LOG_NAME, filebuf, func);

    printf("%s [%s] [%s] [%s] [%s] ", timebuf, level_str[level], APP_LOG_NAME,
           filebuf, func);

    if (console_fp)
        fprintf(console_fp, "%s [%s] [%s] [%s] [%s] ", timebuf,
                level_str[level], APP_LOG_NAME, filebuf, func);

    va_list args;

    va_start(args, fmt);
    vfprintf(log_fp, fmt, args);
    va_end(args);

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    if (console_fp) {
        va_start(args, fmt);
        vfprintf(console_fp, fmt, args);
        va_end(args);
    }

    fprintf(log_fp, "\n");
    printf("\n");

    fflush(log_fp);
    fflush(stdout);

    if (console_fp) {
        fprintf(console_fp, "\n");
        fflush(console_fp);
    }

    pthread_mutex_unlock(&log_mutex);
}