#define H264E_SVC_API 0
#define H264E_MAX_THREADS 0
#if !defined(__i386__) && !defined(__x86_64__) && !defined(_M_IX86) && !defined(_M_X64)
#ifndef H264E_ENABLE_SSE2
#define H264E_ENABLE_SSE2 0
#endif
#endif
#define MINIH264_IMPLEMENTATION
#include "minih264/minih264e.h"

#include "h264_encoder.h"

#include <stdlib.h>
#include <string.h>

struct h264_encoder {
	H264E_persist_t *persist;
	H264E_scratch_t *scratch;
	int width;
	int height;
	int fps;
	int bitrate;
	int gop;
};

h264_encoder_t *h264_encoder_create(int width, int height, int fps,
				    int bitrate, int gop)
{
	H264E_create_param_t param;
	h264_encoder_t *enc;
	int persist_size = 0;
	int scratch_size = 0;

	if (width <= 0 || height <= 0 || (width % 16) != 0 || (height % 16) != 0)
		return NULL;

	enc = calloc(1, sizeof(*enc));
	if (!enc)
		return NULL;

	memset(&param, 0, sizeof(param));
	param.width = width;
	param.height = height;
	param.gop = gop > 0 ? gop : fps;
	/* Input I420 is rebuilt every frame; allow the encoder to scratch it. */
	param.const_input_flag = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
	param.enableNEON = 1;
#endif

	if (H264E_sizeof(&param, &persist_size, &scratch_size) != 0)
		goto fail;

	enc->persist = malloc((size_t)persist_size);
	enc->scratch = malloc((size_t)scratch_size);
	if (!enc->persist || !enc->scratch)
		goto fail;

	if (H264E_init(enc->persist, &param) != 0)
		goto fail;

	enc->width = width;
	enc->height = height;
	enc->fps = fps > 0 ? fps : 25;
	enc->bitrate = bitrate > 0 ? bitrate : 400000;
	enc->gop = param.gop;
	return enc;

fail:
	h264_encoder_destroy(enc);
	return NULL;
}

void h264_encoder_destroy(h264_encoder_t *enc)
{
	if (!enc)
		return;
	free(enc->persist);
	free(enc->scratch);
	free(enc);
}

int h264_encoder_encode(h264_encoder_t *enc, const unsigned char *i420,
			int force_key, const unsigned char **out_data,
			int *out_size)
{
	H264E_run_param_t run;
	H264E_io_yuv_t yuv;
	unsigned char *coded = NULL;
	int coded_size = 0;
	int y_size;

	if (!enc || !i420 || !out_data || !out_size)
		return -1;

	y_size = enc->width * enc->height;
	memset(&run, 0, sizeof(run));
	/* Speeds 8 and 10 disable the deblock filter (blocky thermal gradients). */
	run.encode_speed = 9;
	run.frame_type = force_key ? H264E_FRAME_TYPE_KEY : H264E_FRAME_TYPE_DEFAULT;
	run.desired_frame_bytes = enc->bitrate / (8 * enc->fps);
	if (run.desired_frame_bytes < 800)
		run.desired_frame_bytes = 800;
	run.qp_min = 22;
	run.qp_max = 42;

	memset(&yuv, 0, sizeof(yuv));
	yuv.yuv[0] = (unsigned char *)i420;
	yuv.yuv[1] = (unsigned char *)i420 + y_size;
	yuv.yuv[2] = (unsigned char *)i420 + y_size + y_size / 4;
	yuv.stride[0] = enc->width;
	yuv.stride[1] = enc->width / 2;
	yuv.stride[2] = enc->width / 2;

	if (H264E_encode(enc->persist, enc->scratch, &run, &yuv, &coded,
			 &coded_size) != 0)
		return -1;

	*out_data = coded;
	*out_size = coded_size;
	return 0;
}
