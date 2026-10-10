#ifndef NET_APP_NTP_CLIENT_H
#define NET_APP_NTP_CLIENT_H

#include <time.h>

#define NTP_DEFAULT_PORT 123
#define NTP_TIMEOUT_SEC 8

int ntp_query(const char *server, int port, time_t *out_time);
int ntp_query_with_fallback(const char *primary, int port,
			    const char *const *fallbacks, time_t *out_time,
			    const char **out_server);

#endif
