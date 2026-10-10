#include "ntp_client.h"

#include "libs/log/log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define NTP_EPOCH_OFFSET 2208988800UL

typedef struct {
	uint8_t li_vn_mode;
	uint8_t stratum;
	uint8_t poll;
	int8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint32_t ref_id;
	uint32_t ref_ts_sec;
	uint32_t ref_ts_frac;
	uint32_t orig_ts_sec;
	uint32_t orig_ts_frac;
	uint32_t rx_ts_sec;
	uint32_t rx_ts_frac;
	uint32_t tx_ts_sec;
	uint32_t tx_ts_frac;
} ntp_packet_t;

int ntp_query(const char *server, int port, time_t *out_time)
{
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct timeval tv;
	ntp_packet_t pkt;
	char port_str[8];
	uint32_t tx_sec;
	ssize_t n;
	int fd;
	int ret;

	if (!server || !out_time)
		return -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	snprintf(port_str, sizeof(port_str), "%d",
		 port > 0 ? port : NTP_DEFAULT_PORT);

	ret = getaddrinfo(server, port_str, &hints, &res);
	if (ret != 0 || !res) {
		LOGW("NTP: cannot resolve %s: %s", server, gai_strerror(ret));
		return -1;
	}

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		LOGW("NTP: socket failed");
		freeaddrinfo(res);
		return -1;
	}

	tv.tv_sec = NTP_TIMEOUT_SEC;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	memset(&pkt, 0, sizeof(pkt));
	pkt.li_vn_mode = (0 << 6) | (4 << 3) | 3;

	if (sendto(fd, &pkt, sizeof(pkt), 0, res->ai_addr, res->ai_addrlen) <
	    0) {
		LOGW("NTP: sendto %s failed: %s", server, strerror(errno));
		close(fd);
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);

	memset(&pkt, 0, sizeof(pkt));
	n = recvfrom(fd, &pkt, sizeof(pkt), 0, NULL, NULL);
	close(fd);
	if (n < (ssize_t)sizeof(pkt)) {
		LOGW("NTP: response too short or timeout from %s", server);
		return -1;
	}

	tx_sec = ntohl(pkt.tx_ts_sec);
	if (tx_sec == 0) {
		LOGW("NTP: invalid response from %s", server);
		return -1;
	}

	*out_time = (time_t)(tx_sec - NTP_EPOCH_OFFSET);
	return 0;
}

int ntp_query_with_fallback(const char *primary, int port,
			    const char *const *fallbacks, time_t *out_time,
			    const char **out_server)
{
	int i;

	if (!out_time)
		return -1;
	if (port <= 0)
		port = NTP_DEFAULT_PORT;

	if (primary && primary[0] != '\0') {
		if (ntp_query(primary, port, out_time) == 0) {
			if (out_server)
				*out_server = primary;
			return 0;
		}
	}

	if (fallbacks) {
		for (i = 0; fallbacks[i] != NULL; i++) {
			LOGI("NTP: trying fallback server %s:%d", fallbacks[i],
			     port);
			if (ntp_query(fallbacks[i], port, out_time) == 0) {
				if (out_server)
					*out_server = fallbacks[i];
				return 0;
			}
		}
	}

	return -1;
}
