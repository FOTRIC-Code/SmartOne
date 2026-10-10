#include "binary_writer.h"

#include <string.h>

void bin_write_le16(uint8_t *dst, uint16_t value)
{
	dst[0] = (uint8_t)(value & 0xffU);
	dst[1] = (uint8_t)((value >> 8) & 0xffU);
}

void bin_write_le32(uint8_t *dst, uint32_t value)
{
	dst[0] = (uint8_t)(value & 0xffU);
	dst[1] = (uint8_t)((value >> 8) & 0xffU);
	dst[2] = (uint8_t)((value >> 16) & 0xffU);
	dst[3] = (uint8_t)((value >> 24) & 0xffU);
}

void bin_write_le64(uint8_t *dst, uint64_t value)
{
	bin_write_le32(dst, (uint32_t)(value & 0xffffffffULL));
	bin_write_le32(dst + 4, (uint32_t)(value >> 32));
}

void bin_write_f32_le(uint8_t *dst, float value)
{
	uint32_t bits;

	memcpy(&bits, &value, sizeof(bits));
	bin_write_le32(dst, bits);
}

void bin_write_be16(uint8_t *dst, uint16_t value)
{
	dst[0] = (uint8_t)((value >> 8) & 0xffU);
	dst[1] = (uint8_t)(value & 0xffU);
}

void bin_write_be32(uint8_t *dst, uint32_t value)
{
	dst[0] = (uint8_t)((value >> 24) & 0xffU);
	dst[1] = (uint8_t)((value >> 16) & 0xffU);
	dst[2] = (uint8_t)((value >> 8) & 0xffU);
	dst[3] = (uint8_t)(value & 0xffU);
}
