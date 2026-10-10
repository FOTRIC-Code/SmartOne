#include "photo_album.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	unsigned char *bgr;
	int width;
	int height;
} photo_slot_t;

typedef struct {
	photo_slot_t slots[PHOTO_ALBUM_CAPACITY];
	int count;
} photo_set_t;

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static photo_set_t s_current;
static photo_set_t s_ready;

static void set_clear(photo_set_t *set)
{
	int i;

	for (i = 0; i < PHOTO_ALBUM_CAPACITY; ++i) {
		free(set->slots[i].bgr);
		set->slots[i].bgr = NULL;
		set->slots[i].width = 0;
		set->slots[i].height = 0;
	}
	set->count = 0;
}

void photo_album_reset(void)
{
	pthread_mutex_lock(&s_lock);
	set_clear(&s_current);
	set_clear(&s_ready);
	pthread_mutex_unlock(&s_lock);
}

void photo_album_add(const unsigned char *bgr, int width, int height)
{
	photo_slot_t *slot;
	size_t size;

	if (!bgr || width <= 0 || height <= 0)
		return;
	size = (size_t)width * (size_t)height * 3U;

	pthread_mutex_lock(&s_lock);
	if (s_current.count < PHOTO_ALBUM_CAPACITY) {
		slot = &s_current.slots[s_current.count];
		if (slot->width != width || slot->height != height) {
			free(slot->bgr);
			slot->bgr = malloc(size);
		}
		if (slot->bgr) {
			memcpy(slot->bgr, bgr, size);
			slot->width = width;
			slot->height = height;
			s_current.count++;
		}
	}
	pthread_mutex_unlock(&s_lock);
}

void photo_album_finish_round(void)
{
	photo_set_t tmp;

	pthread_mutex_lock(&s_lock);
	tmp = s_ready;
	s_ready = s_current;
	s_current = tmp;
	s_current.count = 0;
	pthread_mutex_unlock(&s_lock);
}

int photo_album_copy(photo_frame_t *out, int max_count)
{
	int count = 0;
	int i;

	if (!out || max_count <= 0)
		return 0;

	pthread_mutex_lock(&s_lock);
	for (i = 0; i < s_ready.count && count < max_count; ++i) {
		const photo_slot_t *slot = &s_ready.slots[i];
		size_t size;

		if (!slot->bgr)
			continue;
		size = (size_t)slot->width * (size_t)slot->height * 3U;
		out[count].bgr = malloc(size);
		if (!out[count].bgr)
			break;
		memcpy(out[count].bgr, slot->bgr, size);
		out[count].width = slot->width;
		out[count].height = slot->height;
		count++;
	}
	pthread_mutex_unlock(&s_lock);
	return count;
}

void photo_album_free_copy(photo_frame_t *frames, int count)
{
	int i;

	if (!frames)
		return;
	for (i = 0; i < count; ++i)
		free(frames[i].bgr);
}
