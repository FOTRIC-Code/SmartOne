#include "video_process.h"

#include "image_convert.h"
#include "libs/binary/binary_writer.h"
#include "libs/log/log.h"
#include "net_common/net_common.h"
#include "zlm/h264_encoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Compose still BGR frames into a one-second-per-photo MP4 (all keyframes).
 * Minimal ISO BMFF writer: ftyp + mdat (AVCC samples) + moov, single chunk,
 * built with precomputed sizes so the file is written in one pass.
 */

#define STILLS_MP4_TIMESCALE 1000
#define STILLS_MP4_BITRATE 200000

typedef struct {
	unsigned char *data;
	size_t size;
} sample_t;

typedef struct {
	unsigned char *data;
	size_t size;
	size_t cap;
} membuf_t;

static int mem_reserve(membuf_t *buf, size_t need)
{
	unsigned char *p;
	size_t cap;

	if (buf->cap >= need)
		return 0;
	cap = buf->cap ? buf->cap : 4096;
	while (cap < need)
		cap *= 2;
	p = realloc(buf->data, cap);
	if (!p)
		return -1;
	buf->data = p;
	buf->cap = cap;
	return 0;
}

static int mem_put(membuf_t *buf, const void *data, size_t size)
{
	if (mem_reserve(buf, buf->size + size) != 0)
		return -1;
	memcpy(buf->data + buf->size, data, size);
	buf->size += size;
	return 0;
}

static int mem_put_zero(membuf_t *buf, size_t size)
{
	if (mem_reserve(buf, buf->size + size) != 0)
		return -1;
	memset(buf->data + buf->size, 0, size);
	buf->size += size;
	return 0;
}

static int mem_put_u8(membuf_t *buf, unsigned value)
{
	unsigned char b = (unsigned char)value;

	return mem_put(buf, &b, 1);
}

static int mem_put_be16(membuf_t *buf, unsigned value)
{
	unsigned char b[2];

	bin_write_be16(b, (uint16_t)value);
	return mem_put(buf, b, sizeof(b));
}

static int mem_put_be32(membuf_t *buf, uint32_t value)
{
	unsigned char b[4];

	bin_write_be32(b, value);
	return mem_put(buf, b, sizeof(b));
}

static int mem_put_tag(membuf_t *buf, const char tag[4])
{
	return mem_put(buf, tag, 4);
}

/* box_size includes the 8-byte header; all boxes here fit in 32-bit sizes. */
static int mem_put_box_header(membuf_t *buf, const char tag[4],
			      uint32_t box_size)
{
	if (mem_put_be32(buf, box_size) != 0)
		return -1;
	return mem_put_tag(buf, tag);
}

/* Unity matrix used by mvhd/tkhd. */
static int mem_put_matrix(membuf_t *buf)
{
	static const unsigned char matrix[36] = {
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x40, 0x00, 0x00, 0x00
	};

	return mem_put(buf, matrix, sizeof(matrix));
}

/* --- annexb parsing --- */

typedef struct {
	const unsigned char *data;
	size_t size;
} nal_t;

static int nal_next(const unsigned char *data, size_t size, size_t *pos,
		    nal_t *nal)
{
	size_t i = *pos;
	size_t start;
	size_t end;

	while (i + 3 <= size) {
		if (data[i] == 0 && data[i + 1] == 1 &&
		    (i + 2 >= size || data[i + 2] == 1))
			break;
		if (i + 3 < size && data[i] == 0 && data[i + 1] == 0 &&
		    data[i + 2] == 1)
			break;
		i++;
	}
	if (i + 3 > size)
		return -1;
	if (data[i + 2] == 1) {
		start = i + 3;
	} else {
		if (i + 4 > size || data[i + 3] != 1)
			return -1;
		start = i + 4;
	}
	end = start;
	while (end + 3 <= size) {
		if (data[end] == 0 && data[end + 1] == 0 &&
		    (data[end + 2] == 1 || data[end + 3] == 1))
			break;
		end++;
	}
	if (end + 3 > size)
		end = size;
	while (end > start && data[end - 1] == 0)
		end--;
	if (end == start)
		return -1;
	nal->data = data + start;
	nal->size = end - start;
	*pos = end;
	return 0;
}

static int collect_sample(const unsigned char *coded, int coded_size,
			  unsigned char **sps, size_t *sps_size,
			  unsigned char **pps, size_t *pps_size,
			  sample_t *sample)
{
	size_t pos = 0;
	nal_t nal;
	membuf_t out = { 0 };

	while (nal_next(coded, (size_t)coded_size, &pos, &nal) == 0) {
		unsigned type = nal.data[0] & 0x1fU;

		if (type == 7 && !*sps) {
			*sps = malloc(nal.size);
			if (!*sps)
				goto fail;
			memcpy(*sps, nal.data, nal.size);
			*sps_size = nal.size;
		} else if (type == 8 && !*pps) {
			*pps = malloc(nal.size);
			if (!*pps)
				goto fail;
			memcpy(*pps, nal.data, nal.size);
			*pps_size = nal.size;
		} else if (type == 5) {
			unsigned char len[4];

			bin_write_be32(len, (uint32_t)nal.size);
			if (mem_put(&out, len, 4) != 0 ||
			    mem_put(&out, nal.data, nal.size) != 0)
				goto fail;
		}
	}
	if (!out.size)
		goto fail;
	sample->data = out.data;
	sample->size = out.size;
	return 0;

fail:
	free(out.data);
	return -1;
}

/* --- moov construction --- */

static int put_mvhd(membuf_t *moov, uint32_t duration)
{
	mem_put_box_header(moov, "mvhd", 108);
	mem_put_be32(moov, 0); /* version/flags */
	mem_put_be32(moov, 0); /* creation_time */
	mem_put_be32(moov, 0); /* modification_time */
	mem_put_be32(moov, STILLS_MP4_TIMESCALE);
	mem_put_be32(moov, duration);
	mem_put_be32(moov, 0x00010000); /* rate */
	mem_put_be16(moov, 0x0100); /* volume */
	mem_put_be16(moov, 0);
	mem_put_zero(moov, 8); /* reserved */
	mem_put_matrix(moov);
	mem_put_zero(moov, 24); /* pre_defined */
	mem_put_be32(moov, 2); /* next_track_ID */
	return 0;
}

static int put_tkhd(membuf_t *trak, uint32_t duration, int width, int height)
{
	mem_put_box_header(trak, "tkhd", 92);
	mem_put_be32(trak, 0x000003); /* version 0, enabled | in_movie */
	mem_put_be32(trak, 0);
	mem_put_be32(trak, 0);
	mem_put_be32(trak, 1); /* track_ID */
	mem_put_be32(trak, 0);
	mem_put_be32(trak, duration);
	mem_put_zero(trak, 8);
	mem_put_be16(trak, 0); /* layer */
	mem_put_be16(trak, 0); /* alternate_group */
	mem_put_be16(trak, 0); /* volume (video track) */
	mem_put_be16(trak, 0);
	mem_put_matrix(trak);
	mem_put_be32(trak, (uint32_t)width << 16);
	mem_put_be32(trak, (uint32_t)height << 16);
	return 0;
}

static int put_mdhd(membuf_t *mdia, uint32_t duration)
{
	mem_put_box_header(mdia, "mdhd", 32);
	mem_put_be32(mdia, 0);
	mem_put_be32(mdia, 0);
	mem_put_be32(mdia, 0);
	mem_put_be32(mdia, STILLS_MP4_TIMESCALE);
	mem_put_be32(mdia, duration);
	mem_put_be16(mdia, 0x55c4); /* language: und */
	mem_put_be16(mdia, 0);
	return 0;
}

static int put_hdlr(membuf_t *mdia)
{
	static const char name[] = "VideoHandler";

	mem_put_box_header(mdia, "hdlr", 8 + 24 + (uint32_t)sizeof(name));
	mem_put_be32(mdia, 0);
	mem_put_be32(mdia, 0);
	mem_put_tag(mdia, "vide");
	mem_put_zero(mdia, 12);
	mem_put(mdia, name, sizeof(name));
	return 0;
}

static int put_vmhd(membuf_t *minf)
{
	mem_put_box_header(minf, "vmhd", 20);
	mem_put_be32(minf, 1); /* version 0, flags: single layer */
	mem_put_be16(minf, 0);
	mem_put_zero(minf, 6);
	return 0;
}

static int put_dinf(membuf_t *minf)
{
	mem_put_box_header(minf, "dinf", 36);
	mem_put_box_header(minf, "dref", 28);
	mem_put_be32(minf, 0);
	mem_put_be32(minf, 1);
	mem_put_box_header(minf, "url ", 12);
	mem_put_be32(minf, 1); /* self-contained */
	return 0;
}

static int put_avc1(membuf_t *stsd, int width, int height,
		    const unsigned char *sps, size_t sps_size,
		    const unsigned char *pps, size_t pps_size)
{
	uint32_t avcc_size = 8 + 6 + 2 + (uint32_t)sps_size + 1 + 2 +
			     (uint32_t)pps_size;
	uint32_t avc1_size = 8 + 78 + avcc_size;

	mem_put_box_header(stsd, "avc1", avc1_size);
	mem_put_zero(stsd, 6);
	mem_put_be16(stsd, 1); /* data_reference_index */
	mem_put_zero(stsd, 16); /* pre_defined + reserved */
	mem_put_be16(stsd, (unsigned)width);
	mem_put_be16(stsd, (unsigned)height);
	mem_put_be32(stsd, 0x00480000); /* horizresolution 72dpi */
	mem_put_be32(stsd, 0x00480000); /* vertresolution 72dpi */
	mem_put_be32(stsd, 0);
	mem_put_be16(stsd, 1); /* frame_count */
	mem_put_zero(stsd, 32); /* compressorname */
	mem_put_be16(stsd, 0x0018); /* depth */
	mem_put_be16(stsd, 0xffff); /* pre_defined */
	mem_put_box_header(stsd, "avcC", avcc_size);
	mem_put_u8(stsd, 1);
	mem_put_u8(stsd, sps[1]); /* profile */
	mem_put_u8(stsd, sps[2]); /* compatibility */
	mem_put_u8(stsd, sps[3]); /* level */
	mem_put_u8(stsd, 0xff); /* 4-byte sample lengths */
	mem_put_u8(stsd, 0xe1); /* one SPS */
	mem_put_be16(stsd, (unsigned)sps_size);
	mem_put(stsd, sps, sps_size);
	mem_put_u8(stsd, 1); /* one PPS */
	mem_put_be16(stsd, (unsigned)pps_size);
	mem_put(stsd, pps, pps_size);
	return 0;
}

static uint32_t build_moov(membuf_t *moov, const sample_t *samples,
			   int count, const unsigned char *sps,
			   size_t sps_size, const unsigned char *pps,
			   size_t pps_size, int width, int height,
			   uint32_t mdat_data_offset)
{
	membuf_t trak = { 0 };
	membuf_t mdia = { 0 };
	membuf_t minf = { 0 };
	membuf_t stbl = { 0 };
	membuf_t stsd = { 0 };
	uint32_t duration = (uint32_t)count * STILLS_MP4_TIMESCALE;
	int i;

	put_avc1(&stsd, width, height, sps, sps_size, pps, pps_size);

	/* stsd */
	{
		membuf_t box = { 0 };

		mem_put_box_header(&box, "stsd", 16 + (uint32_t)stsd.size);
		mem_put_be32(&box, 0);
		mem_put_be32(&box, 1);
		mem_put(&box, stsd.data, stsd.size);
		free(stsd.data);
		stsd = box;
	}

	/* stbl */
	mem_put_box_header(&stbl, "stts", 24);
	mem_put_be32(&stbl, 0);
	mem_put_be32(&stbl, 1);
	mem_put_be32(&stbl, (uint32_t)count);
	mem_put_be32(&stbl, STILLS_MP4_TIMESCALE);

	mem_put_box_header(&stbl, "stss", 16 + 4U * (uint32_t)count);
	mem_put_be32(&stbl, 0);
	mem_put_be32(&stbl, (uint32_t)count);
	for (i = 0; i < count; ++i)
		mem_put_be32(&stbl, (uint32_t)(i + 1));

	mem_put_box_header(&stbl, "stsc", 28);
	mem_put_be32(&stbl, 0);
	mem_put_be32(&stbl, 1);
	mem_put_be32(&stbl, 1);
	mem_put_be32(&stbl, (uint32_t)count);
	mem_put_be32(&stbl, 1);

	mem_put_box_header(&stbl, "stsz", 20 + 4U * (uint32_t)count);
	mem_put_be32(&stbl, 0);
	mem_put_be32(&stbl, 0);
	mem_put_be32(&stbl, (uint32_t)count);
	for (i = 0; i < count; ++i)
		mem_put_be32(&stbl, (uint32_t)samples[i].size);

	mem_put_box_header(&stbl, "stco", 20);
	mem_put_be32(&stbl, 0);
	mem_put_be32(&stbl, 1);
	mem_put_be32(&stbl, mdat_data_offset);

	/* prepend stsd */
	{
		membuf_t full = { 0 };

		mem_put(&full, stsd.data, stsd.size);
		mem_put(&full, stbl.data, stbl.size);
		free(stsd.data);
		free(stbl.data);
		stbl = full;
	}

	put_vmhd(&minf);
	put_dinf(&minf);

	/* stbl box */
	{
		membuf_t box = { 0 };

		mem_put_box_header(&box, "stbl", 8 + (uint32_t)stbl.size);
		mem_put(&box, stbl.data, stbl.size);
		free(stbl.data);
		stbl = box;
	}
	mem_put(&minf, stbl.data, stbl.size);
	free(stbl.data);

	/* minf box */
	{
		membuf_t box = { 0 };

		mem_put_box_header(&box, "minf", 8 + (uint32_t)minf.size);
		mem_put(&box, minf.data, minf.size);
		free(minf.data);
		minf = box;
	}

	put_mdhd(&mdia, duration);
	put_hdlr(&mdia);
	mem_put(&mdia, minf.data, minf.size);
	free(minf.data);

	/* mdia box */
	{
		membuf_t box = { 0 };

		mem_put_box_header(&box, "mdia", 8 + (uint32_t)mdia.size);
		mem_put(&box, mdia.data, mdia.size);
		free(mdia.data);
		mdia = box;
	}

	put_tkhd(&trak, duration, width, height);
	mem_put(&trak, mdia.data, mdia.size);
	free(mdia.data);

	/* trak box */
	{
		membuf_t box = { 0 };

		mem_put_box_header(&box, "trak", 8 + (uint32_t)trak.size);
		mem_put(&box, trak.data, trak.size);
		free(trak.data);
		trak = box;
	}

	put_mvhd(moov, duration);
	mem_put(moov, trak.data, trak.size);
	free(trak.data);
	return 8 + (uint32_t)moov->size;
}

int video_process_compose_stills_mp4(const video_still_frame_t *frames,
				     int count, const char *path)
{
	sample_t samples[PHOTO_FRAMES_MAX];
	unsigned char *sps = NULL;
	unsigned char *pps = NULL;
	unsigned char *scaled = NULL;
	unsigned char *i420 = NULL;
	size_t sps_size = 0;
	size_t pps_size = 0;
	h264_encoder_t *enc = NULL;
	membuf_t moov = { 0 };
	FILE *fp = NULL;
	char tmp_path[NET_LEN_128];
	uint32_t mdat_size;
	uint32_t ftyp_size = 24;
	int width;
	int height;
	int i;
	int ret = -1;

	if (!frames || count <= 0 || count > PHOTO_FRAMES_MAX || !path ||
	    !path[0])
		return -1;

	width = image_align16(frames[0].width);
	height = image_align16(frames[0].height);
	if (width <= 0 || height <= 0)
		return -1;

	memset(samples, 0, sizeof(samples));
	enc = h264_encoder_create(width, height, 1, STILLS_MP4_BITRATE, 1);
	if (!enc)
		goto out;

	scaled = malloc((size_t)width * (size_t)height * 3U);
	i420 = malloc((size_t)width * (size_t)height * 3U / 2U);
	if (!scaled || !i420)
		goto out;

	for (i = 0; i < count; ++i) {
		const unsigned char *coded;
		int coded_size = 0;

		if (!frames[i].bgr || frames[i].width <= 0 ||
		    frames[i].height <= 0)
			goto out;
		if (frames[i].width == width && frames[i].height == height)
			image_bgr_to_i420(frames[i].bgr, width, height, i420);
		else {
			image_scale_bgr(frames[i].bgr, frames[i].width,
					frames[i].height, scaled, width,
					height);
			image_bgr_to_i420(scaled, width, height, i420);
		}
		if (h264_encoder_encode(enc, i420, 1, &coded, &coded_size) != 0)
			goto out;
		if (collect_sample(coded, coded_size, &sps, &sps_size, &pps,
				   &pps_size, &samples[i]) != 0)
			goto out;
	}
	if (!sps || !pps)
		goto out;

	mdat_size = 8;
	for (i = 0; i < count; ++i)
		mdat_size += (uint32_t)samples[i].size;

	/* ftyp + mdat header occupy mdat_data_offset bytes before samples. */
	build_moov(&moov, samples, count, sps, sps_size, pps, pps_size, width,
		   height, ftyp_size + 8);
	if (!moov.size)
		goto out;

	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
	fp = fopen(tmp_path, "wb");
	if (!fp)
		goto out;

	/* ftyp */
	{
		unsigned char hdr[8];

		bin_write_be32(hdr, ftyp_size);
		memcpy(hdr + 4, "ftyp", 4);
		if (fwrite(hdr, 1, 8, fp) != 8)
			goto out;
		if (fwrite("isom", 1, 4, fp) != 4)
			goto out;
		bin_write_be32(hdr, 1);
		if (fwrite(hdr, 1, 4, fp) != 4)
			goto out;
		if (fwrite("isom", 1, 4, fp) != 4 ||
		    fwrite("avc1", 1, 4, fp) != 4)
			goto out;

		bin_write_be32(hdr, mdat_size);
		memcpy(hdr + 4, "mdat", 4);
		if (fwrite(hdr, 1, 8, fp) != 8)
			goto out;
	}
	for (i = 0; i < count; ++i)
		if (fwrite(samples[i].data, 1, samples[i].size, fp) !=
		    samples[i].size)
			goto out;

	/* moov box header then body */
	{
		if (mem_reserve(&moov, moov.size + 8) != 0)
			goto out;
		memmove(moov.data + 8, moov.data, moov.size);
		bin_write_be32(moov.data, (uint32_t)moov.size + 8);
		memcpy(moov.data + 4, "moov", 4);
		moov.size += 8;
		if (fwrite(moov.data, 1, moov.size, fp) != moov.size)
			goto out;
	}

	if (fclose(fp) != 0) {
		fp = NULL;
		goto out;
	}
	fp = NULL;
	if (rename(tmp_path, path) != 0)
		goto out;
	ret = 0;

out:
	if (fp)
		fclose(fp);
	if (ret != 0)
		remove(tmp_path);
	free(moov.data);
	free(sps);
	free(pps);
	free(scaled);
	free(i420);
	for (i = 0; i < count; ++i)
		free(samples[i].data);
	h264_encoder_destroy(enc);
	if (ret == 0)
		LOGI("[VIDEO] stills MP4 composed: %s (%d photos)", path,
		     count);
	else
		LOGE("[VIDEO] stills MP4 compose failed: %s", path);
	return ret;
}
