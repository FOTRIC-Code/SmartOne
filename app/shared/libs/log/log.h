#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

void log_init(void);
void log_set_level(log_level_t level);
void log_apply_config(int enabled, int level, int max_file_size, const char *file_path);
void log_close(void);

void log_write(
    log_level_t level,
    const char *file,
    const char *func,
    int line,
    const char *fmt,
    ...);

#define LOGD(fmt, ...) log_write(LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) log_write(LOG_LEVEL_INFO,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) log_write(LOG_LEVEL_WARN,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) log_write(LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __LOG_H__ */
