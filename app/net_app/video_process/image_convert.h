#ifndef NET_APP_IMAGE_CONVERT_H
#define NET_APP_IMAGE_CONVERT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int image_align16(int value);
void image_scale_bgr(const unsigned char *src, int src_w, int src_h,
		     unsigned char *dst, int dst_w, int dst_h);
/*
 * flip then rotate (clockwise degrees). flip_mode: net_enum_image_flip_mode_t
 * (1=up-down, 2=left-right, 3=center). rotation: 0/90/180/270.
 * dst must hold width*height*3 bytes; out dims swap on 90/270.
 */
void image_transform_bgr(const unsigned char *src, int width, int height,
			 int flip_mode, int rotation, unsigned char *dst,
			 int *out_w, int *out_h);
void image_bgr_to_i420(const unsigned char *bgr, int width, int height,
		       unsigned char *i420);
/* Encodes BGR as JPEG into a malloc'd buffer; caller frees *out_data. */
int image_bgr_to_jpeg_mem(const unsigned char *bgr, int width, int height,
			  int quality, unsigned char **out_data,
			  size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif
