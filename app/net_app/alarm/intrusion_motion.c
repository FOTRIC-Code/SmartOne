#include "intrusion_motion.h"

#include "thermal/thermal_data_provider.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INTRUSION_SAMPLE_STRIDE 8
#define INTRUSION_DIFF_THRESHOLD 24

static uint16_t *s_current;
static size_t s_current_cap;
static uint16_t *s_previous;
static size_t s_previous_cap;
static size_t s_previous_count;
static int s_previous_width;
static int s_previous_height;
static unsigned int s_previous_frame_no;
static int s_has_previous;

static int motion_percent(const uint16_t *current, const uint16_t *previous,
			  size_t count)
{
	size_t sampled = 0;
	size_t changed = 0;
	size_t i;

	if (!current || !previous || count == 0)
		return 0;

	for (i = 0; i < count; i += INTRUSION_SAMPLE_STRIDE) {
		unsigned int diff = current[i] > previous[i] ?
					    current[i] - previous[i] :
					    previous[i] - current[i];

		sampled++;
		if (diff >= INTRUSION_DIFF_THRESHOLD)
			changed++;
	}
	if (sampled == 0)
		return 0;
	return (int)((changed * 100 + sampled / 2) / sampled);
}

static int keep_as_previous(size_t count, int width, int height,
			    unsigned int frame_no)
{
	if (s_previous_cap < count) {
		uint16_t *grown = realloc(s_previous, count * sizeof(*s_previous));

		if (!grown)
			return -1;
		s_previous = grown;
		s_previous_cap = count;
	}
	memcpy(s_previous, s_current, count * sizeof(*s_current));
	s_previous_count = count;
	s_previous_width = width;
	s_previous_height = height;
	s_previous_frame_no = frame_no;
	s_has_previous = 1;
	return 0;
}

static int copy_latest(size_t *count, int *width, int *height,
		       unsigned int *frame_no)
{
	if (thermal_copy_latest_raw(s_current, s_current_cap, count, width,
				    height, frame_no) == 0)
		return 0;
	if (*count == 0 || *count <= s_current_cap)
		return -1;

	{
		uint16_t *grown = realloc(s_current, *count * sizeof(*s_current));

		if (!grown)
			return -1;
		s_current = grown;
		s_current_cap = *count;
	}
	return thermal_copy_latest_raw(s_current, s_current_cap, count, width,
				       height, frame_no);
}

void intrusion_motion_reset(void)
{
	s_has_previous = 0;
	s_previous_count = 0;
	s_previous_frame_no = 0;
}

int intrusion_motion_sample(int *motion)
{
	size_t count = 0;
	int width = 0;
	int height = 0;
	unsigned int frame_no = 0;
	int comparable;

	if (!motion)
		return INTRUSION_MOTION_UNAVAILABLE;
	if (copy_latest(&count, &width, &height, &frame_no) != 0 || count == 0)
		return INTRUSION_MOTION_UNAVAILABLE;

	comparable = s_has_previous && s_previous_frame_no != frame_no &&
		     s_previous_count == count && s_previous_width == width &&
		     s_previous_height == height;
	if (comparable)
		*motion = motion_percent(s_current, s_previous, count);
	if (keep_as_previous(count, width, height, frame_no) != 0)
		return INTRUSION_MOTION_UNAVAILABLE;
	return comparable ? INTRUSION_MOTION_READY : INTRUSION_MOTION_BASELINE;
}
