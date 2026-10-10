#ifndef NET_APP_PHOTO_ALBUM_H
#define NET_APP_PHOTO_ALBUM_H

#include <stddef.h>
#include <stdint.h>

/*
 * Body-temp monitor photos (BGR preview frames), organized in discrete
 * rounds: photos accumulate in the current round; photo_album_finish_round()
 * retires them into the ready buffer that the alarm linkage worker reads
 * when composing an MP4, and the current round starts empty again.
 */

#define PHOTO_ALBUM_CAPACITY 10

typedef struct {
	unsigned char *bgr;
	int width;
	int height;
} photo_frame_t;

void photo_album_reset(void);
void photo_album_add(const unsigned char *bgr, int width, int height);
void photo_album_finish_round(void);
/* Copies the last finished round in capture order; release with photo_album_free_copy. */
int photo_album_copy(photo_frame_t *out, int max_count);
void photo_album_free_copy(photo_frame_t *frames, int count);

#endif
