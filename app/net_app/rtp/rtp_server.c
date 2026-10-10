#include "rtp_server.h"
#include "rtp_wire.h"

#include "libs/log/log.h"
#include "net_common/net_common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct rtp_client {
	int fd;
	int active;
	int alarm_ready;
	uint16_t sequence;
	char session_id[32];
	char request[512];
	size_t request_len;
} rtp_client_t;

static int s_listen_fd = -1;
static rtp_client_t s_clients[RTP_MAX_CLIENTS];
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t s_thread;
static int s_running;
static int s_thread_started;

static void *rtp_server_thread(void *opaque);

static int send_all(int fd, const uint8_t *data, size_t length);

static void set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int try_accept(void)
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int fd;
	int i;

	fd = accept(s_listen_fd, (struct sockaddr *)&addr, &len);
	if (fd < 0)
		return -1;
	set_nonblock(fd);

	pthread_mutex_lock(&s_lock);
	for (i = 0; i < RTP_MAX_CLIENTS; ++i) {
		if (!s_clients[i].active) {
			memset(&s_clients[i], 0, sizeof(s_clients[i]));
			s_clients[i].fd = fd;
			s_clients[i].active = 1;
			snprintf(s_clients[i].session_id,
				 sizeof(s_clients[i].session_id),
				 "alarm-%d", i);
			LOGI("RTP client %d connected from %s:%d",
			     i, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			pthread_mutex_unlock(&s_lock);
			return 0;
		}
	}
	pthread_mutex_unlock(&s_lock);
	LOGW("RTP server full, rejecting connection");
	close(fd);
	return -1;
}

static void handle_session_handshake(rtp_client_t *client)
{
	int n;
	const char *resp;

	if (!client || client->alarm_ready)
		return;
	if (client->request_len >= sizeof(client->request) - 1) {
		resp = "HTTP/1.1 431 Request Header Fields Too Large\r\n"
		       "Content-Length: 0\r\n\r\n";
		send_all(client->fd, (const uint8_t *)resp, strlen(resp));
		close(client->fd);
		client->active = 0;
		return;
	}
	n = (int)recv(client->fd, client->request + client->request_len,
		      sizeof(client->request) - client->request_len - 1,
		      MSG_DONTWAIT);
	if (n == 0) {
		close(client->fd);
		client->active = 0;
		return;
	}
	if (n < 0)
		return;
	client->request_len += (size_t)n;
	client->request[client->request_len] = '\0';
	if (!strstr(client->request, "\r\n\r\n"))
		return;

	if (strstr(client->request, "GET /session") != NULL &&
	    strstr(client->request, "role=alarm") != NULL) {
		resp = "HTTP/1.1 200 OK\r\n"
		       "Content-Length: 0\r\n"
		       "\r\n";
		if (send_all(client->fd, (const uint8_t *)resp,
			     strlen(resp)) == 0)
			client->alarm_ready = 1;
	} else {
		resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
		send_all(client->fd, (const uint8_t *)resp, strlen(resp));
		close(client->fd);
		client->active = 0;
	}
}

int rtp_server_start(int port)
{
	struct sockaddr_in addr;
	int opt = 1;

	if (s_running)
		return 0;

	if (port <= 0)
		port = NET_DEFAULT_RTP_PORT;

	s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (s_listen_fd < 0) {
		LOGE("RTP socket failed: %s", strerror(errno));
		return -1;
	}

	setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	set_nonblock(s_listen_fd);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(s_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOGE("RTP bind failed on port %d: %s", port, strerror(errno));
		close(s_listen_fd);
		s_listen_fd = -1;
		return -1;
	}

	if (listen(s_listen_fd, RTP_MAX_CLIENTS) < 0) {
		LOGE("RTP listen failed: %s", strerror(errno));
		close(s_listen_fd);
		s_listen_fd = -1;
		return -1;
	}

	memset(s_clients, 0, sizeof(s_clients));
	s_running = 1;
	if (pthread_create(&s_thread, NULL, rtp_server_thread, NULL) != 0) {
		LOGE("RTP server thread failed");
		s_running = 0;
		close(s_listen_fd);
		s_listen_fd = -1;
		return -1;
	}
	s_thread_started = 1;
	LOGI("RTP alarm server listening on port %d", port);
	return 0;
}

void rtp_server_stop(void)
{
	int i;

	if (!s_running && !s_thread_started)
		return;

	s_running = 0;
	if (s_thread_started) {
		pthread_join(s_thread, NULL);
		s_thread_started = 0;
	}
	pthread_mutex_lock(&s_lock);
	for (i = 0; i < RTP_MAX_CLIENTS; ++i) {
		if (s_clients[i].active) {
			close(s_clients[i].fd);
			s_clients[i].active = 0;
		}
	}
	pthread_mutex_unlock(&s_lock);

	if (s_listen_fd >= 0) {
		close(s_listen_fd);
		s_listen_fd = -1;
	}
	LOGI("RTP alarm server stopped");
}

static void rtp_server_poll(void)
{
	int i;

	if (!s_running)
		return;

	try_accept();
	pthread_mutex_lock(&s_lock);
	for (i = 0; i < RTP_MAX_CLIENTS; ++i) {
		if (s_clients[i].active)
			handle_session_handshake(&s_clients[i]);
	}
	pthread_mutex_unlock(&s_lock);
}

static void *rtp_server_thread(void *opaque)
{
	(void)opaque;
	while (s_running) {
		rtp_server_poll();
		usleep(10 * 1000);
	}
	return NULL;
}

static int send_all(int fd, const uint8_t *data, size_t length)
{
	size_t offset = 0;
	int retries = 0;

	while (offset < length) {
		ssize_t n = send(fd, data + offset, length - offset,
				 MSG_NOSIGNAL);
		if (n > 0) {
			offset += (size_t)n;
			retries = 0;
			continue;
		}
		if (n < 0 && (errno == EINTR))
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) &&
		    retries++ < 100) {
			usleep(1000);
			continue;
		}
		return -1;
	}
	return 0;
}

int rtp_push_alarm_event(const alarm_event_t *event)
{
	uint8_t frame[RTP_WIRE_MAX_ALARM_FRAME_SIZE];
	int client_fds[RTP_MAX_CLIENTS];
	int client_slots[RTP_MAX_CLIENTS];
	uint16_t client_sequences[RTP_MAX_CLIENTS];
	int client_count = 0;
	int i;
	int sent = 0;

	if (!s_running || !event)
		return 0;

	pthread_mutex_lock(&s_lock);
	for (i = 0; i < RTP_MAX_CLIENTS; ++i) {
		if (!s_clients[i].active || !s_clients[i].alarm_ready)
			continue;
		client_fds[client_count] = s_clients[i].fd;
		client_slots[client_count] = i;
		client_sequences[client_count] = s_clients[i].sequence++;
		client_count++;
	}
	pthread_mutex_unlock(&s_lock);

	for (i = 0; i < client_count; ++i) {
		size_t frame_size;

		if (rtp_wire_encode_alarm_frame(event, client_sequences[i],
						frame, sizeof(frame),
						&frame_size) != 0) {
			LOGW("RTP alarm frame encode failed");
			continue;
		}
		if (send_all(client_fds[i], frame, frame_size) != 0) {
			int slot = client_slots[i];

			pthread_mutex_lock(&s_lock);
			if (s_clients[slot].active &&
			    s_clients[slot].fd == client_fds[i]) {
				close(s_clients[slot].fd);
				memset(&s_clients[slot], 0,
				       sizeof(s_clients[slot]));
			}
			pthread_mutex_unlock(&s_lock);
			LOGW("RTP client %d disconnected", slot);
			continue;
		}
		sent++;
	}
	return sent;
}
