#include "image_convert.h"

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

int image_align16(int value)
{
	if (value <= 0)
		return 16;
	return (value + 15) & ~15;
}

void image_scale_bgr(const unsigned char *src, int src_w, int src_h,
		     unsigned char *dst, int dst_w, int dst_h)
{
	int y;
	int x;

	if (!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0)
		return;

	if (src_w == dst_w && src_h == dst_h) {
		memcpy(dst, src, (size_t)src_w * src_h * 3);
		return;
	}

	for (y = 0; y < dst_h; ++y) {
		int src_y = y * src_h / dst_h;
		const unsigned char *src_row = src + (size_t)src_y * src_w * 3;
		unsigned char *dst_row = dst + (size_t)y * dst_w * 3;

		for (x = 0; x < dst_w; ++x) {
			int src_x = x * src_w / dst_w;
			const unsigned char *px = src_row + src_x * 3;

			dst_row[x * 3] = px[0];
			dst_row[x * 3 + 1] = px[1];
			dst_row[x * 3 + 2] = px[2];
		}
	}
}

void image_transform_bgr(const unsigned char *src, int width, int height,
			 int flip_mode, int rotation, unsigned char *dst,
			 int *out_w, int *out_h)
{
	int flip_h = (flip_mode == 2 || flip_mode == 3);
	int flip_v = (flip_mode == 1 || flip_mode == 3);
	int dw;
	int dh;
	int dx;
	int dy;

	if (!src || !dst || width <= 0 || height <= 0)
		return;
	if (rotation != 90 && rotation != 180 && rotation != 270)
		rotation = 0;

	dw = (rotation == 90 || rotation == 270) ? height : width;
	dh = (rotation == 90 || rotation == 270) ? width : height;

	for (dy = 0; dy < dh; ++dy) {
		for (dx = 0; dx < dw; ++dx) {
			int ux;
			int uy;
			int sx;
			int sy;

			switch (rotation) {
			case 90:
				ux = dy;
				uy = height - 1 - dx;
				break;
			case 180:
				ux = width - 1 - dx;
				uy = height - 1 - dy;
				break;
			case 270:
				ux = width - 1 - dy;
				uy = dx;
				break;
			default:
				ux = dx;
				uy = dy;
				break;
			}
			sx = flip_h ? width - 1 - ux : ux;
			sy = flip_v ? height - 1 - uy : uy;
			memcpy(dst + ((size_t)dy * dw + dx) * 3,
			       src + ((size_t)sy * width + sx) * 3, 3);
		}
	}

	if (out_w)
		*out_w = dw;
	if (out_h)
		*out_h = dh;
}

void image_bgr_to_i420(const unsigned char *bgr, int width, int height,
		       unsigned char *i420)
{
	unsigned char *y_plane;
	unsigned char *u_plane;
	unsigned char *v_plane;
	int y;
	int x;

	if (!bgr || !i420 || width <= 0 || height <= 0)
		return;

	y_plane = i420;
	u_plane = i420 + width * height;
	v_plane = u_plane + (width * height) / 4;

	/* BT.601 limited-range, 2x2 blocks. Skip clamps: the formulas already
	 * land in 16-235 / 16-240 for 8-bit RGB. */
	for (y = 0; y + 1 < height; y += 2) {
		const unsigned char *row0 = bgr + (size_t)y * width * 3;
		const unsigned char *row1 = row0 + width * 3;
		unsigned char *y0 = y_plane + y * width;
		unsigned char *y1 = y0 + width;
		unsigned char *u = u_plane + (y / 2) * (width / 2);
		unsigned char *v = v_plane + (y / 2) * (width / 2);

		for (x = 0; x + 1 < width; x += 2) {
			int b00 = row0[0], g00 = row0[1], r00 = row0[2];
			int b01 = row0[3], g01 = row0[4], r01 = row0[5];
			int b10 = row1[0], g10 = row1[1], r10 = row1[2];
			int b11 = row1[3], g11 = row1[4], r11 = row1[5];
			int r = (r00 + r01 + r10 + r11) >> 2;
			int g = (g00 + g01 + g10 + g11) >> 2;
			int b = (b00 + b01 + b10 + b11) >> 2;

			y0[0] = (unsigned char)(((66 * r00 + 129 * g00 + 25 * b00 + 128) >> 8) + 16);
			y0[1] = (unsigned char)(((66 * r01 + 129 * g01 + 25 * b01 + 128) >> 8) + 16);
			y1[0] = (unsigned char)(((66 * r10 + 129 * g10 + 25 * b10 + 128) >> 8) + 16);
			y1[1] = (unsigned char)(((66 * r11 + 129 * g11 + 25 * b11 + 128) >> 8) + 16);
			*u++ = (unsigned char)(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
			*v++ = (unsigned char)(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);

			row0 += 6;
			row1 += 6;
			y0 += 2;
			y1 += 2;
		}
	}
}

/* libjpeg's default error handler exits the process; longjmp back instead. */
typedef struct {
	struct jpeg_error_mgr pub;
	jmp_buf jump;
} jpeg_throw_t;

static void jpeg_throw_exit(j_common_ptr cinfo)
{
	jpeg_throw_t *err = (jpeg_throw_t *)cinfo->err;

	(*cinfo->err->output_message)(cinfo);
	longjmp(err->jump, 1);
}

int image_bgr_to_jpeg_mem(const unsigned char *bgr, int width, int height,
			  int quality, unsigned char **out_data,
			  size_t *out_size)
{
	struct jpeg_compress_struct cinfo;
	jpeg_throw_t jerr;
	unsigned char *rgb = NULL;
	unsigned char *mem = NULL;
	unsigned long mem_size = 0;
	int ret = -1;
	int y;

	if (!bgr || width <= 0 || height <= 0 || !out_data || !out_size)
		return -1;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_throw_exit;
	if (setjmp(jerr.jump)) {
		jpeg_destroy_compress(&cinfo);
		free(rgb);
		free(mem);
		return -1;
	}

	rgb = malloc((size_t)width * (size_t)height * 3U);
	if (!rgb)
		return -1;
	for (y = 0; y < height; ++y) {
		const unsigned char *src = bgr + (size_t)y * width * 3;
		unsigned char *dst = rgb + (size_t)y * width * 3;
		int x;

		for (x = 0; x < width; ++x) {
			dst[x * 3] = src[x * 3 + 2];
			dst[x * 3 + 1] = src[x * 3 + 1];
			dst[x * 3 + 2] = src[x * 3];
		}
	}

	jpeg_create_compress(&cinfo);
	jpeg_mem_dest(&cinfo, &mem, &mem_size);
	cinfo.image_width = (JDIMENSION)width;
	cinfo.image_height = (JDIMENSION)height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality > 0 ? quality : 85, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	while (cinfo.next_scanline < cinfo.image_height) {
		JSAMPROW row = rgb +
			(size_t)cinfo.next_scanline * (size_t)width * 3U;

		jpeg_write_scanlines(&cinfo, &row, 1);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	free(rgb);
	*out_data = mem;
	*out_size = (size_t)mem_size;
	ret = 0;
	return ret;
}
