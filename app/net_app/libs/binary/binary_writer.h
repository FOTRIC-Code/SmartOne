#ifndef NET_APP_LIBS_BINARY_BINARY_WRITER_H
#define NET_APP_LIBS_BINARY_BINARY_WRITER_H

#include <stdint.h>

void bin_write_le16(uint8_t *dst, uint16_t value);
void bin_write_le32(uint8_t *dst, uint32_t value);
void bin_write_le64(uint8_t *dst, uint64_t value);
void bin_write_f32_le(uint8_t *dst, float value);
void bin_write_be16(uint8_t *dst, uint16_t value);
void bin_write_be32(uint8_t *dst, uint32_t value);

#endif
