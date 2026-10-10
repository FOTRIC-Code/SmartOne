#ifndef NET_APP_H264_ENCODER_H
#define NET_APP_H264_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct h264_encoder h264_encoder_t;

h264_encoder_t *h264_encoder_create(int width, int height, int fps,
				    int bitrate, int gop);
void h264_encoder_destroy(h264_encoder_t *enc);
int h264_encoder_encode(h264_encoder_t *enc, const unsigned char *i420,
			int force_key, const unsigned char **out_data,
			int *out_size);

#ifdef __cplusplus
}
#endif

#endif
