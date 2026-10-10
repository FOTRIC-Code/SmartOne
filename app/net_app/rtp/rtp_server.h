#ifndef NET_APP_RTP_SERVER_H
#define NET_APP_RTP_SERVER_H

#include "net_common/net_common.h"

#include <stdint.h>

#define RTP_MAX_CLIENTS 4

int rtp_server_start(int port);
void rtp_server_stop(void);
int rtp_push_alarm_event(const alarm_event_t *event);

#endif
