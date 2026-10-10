#ifndef APP_SHARED_PLATFORM_CPU_SERIAL_H
#define APP_SHARED_PLATFORM_CPU_SERIAL_H

#include <stddef.h>

/*
 * Rockchip OTP id@a (16 bytes) via nvmem sysfs → hex string.
 * out must hold at least CPU_SERIAL_HEX_LEN + 1 bytes.
 * Returns 0 on success, -1 on failure.
 */
#define CPU_SERIAL_HEX_LEN 32

int cpu_serial_read(char *out, size_t out_size);

#endif
