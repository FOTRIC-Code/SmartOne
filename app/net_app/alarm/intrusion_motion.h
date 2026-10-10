#ifndef NET_APP_INTRUSION_MOTION_H
#define NET_APP_INTRUSION_MOTION_H

/* This second has a motion value from the previous second's raw frame. */
#define INTRUSION_MOTION_READY 0
/* No accepted raw frame to copy. */
#define INTRUSION_MOTION_UNAVAILABLE -1
/* Frame stored as the baseline; nothing to compare yet. */
#define INTRUSION_MOTION_BASELINE -2

void intrusion_motion_reset(void);
/* motion is written only when the return value is INTRUSION_MOTION_READY. */
int intrusion_motion_sample(int *motion);

#endif
