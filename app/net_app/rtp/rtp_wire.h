#ifndef NET_APP_RTP_WIRE_H
#define NET_APP_RTP_WIRE_H

#include "net_common/net_common.h"

#include <stddef.h>
#include <stdint.h>

#define RTP_WIRE_TCP_HEADER_SIZE 8U
#define RTP_WIRE_RTP_HEADER_SIZE 8U
#define RTP_WIRE_FRAME_HEADER_SIZE 16U
#define RTP_WIRE_ALARM_PREFIX_SIZE 16U
#define RTP_WIRE_MESSAGE_LENGTH_SIZE 4U

#define RTP_WIRE_MARKER 0x24U
#define RTP_WIRE_RTP_VERSION 2U
#define RTP_WIRE_RTP_PAYLOAD_TYPE 96U

#define RTP_WIRE_MAX_ALARM_DETAIL_SIZE 12U
#define RTP_WIRE_MAX_ALARM_VIDEO_PATH_SIZE NET_LEN_128
#define RTP_WIRE_MAX_ALARM_BODY_SIZE \
	(RTP_WIRE_ALARM_PREFIX_SIZE + RTP_WIRE_MAX_ALARM_DETAIL_SIZE + \
	 RTP_WIRE_MESSAGE_LENGTH_SIZE + ALARM_EVENT_MESSAGE_MAX + \
	 RTP_WIRE_MESSAGE_LENGTH_SIZE + RTP_WIRE_MAX_ALARM_VIDEO_PATH_SIZE)
#define RTP_WIRE_MAX_ALARM_FRAME_SIZE \
	(RTP_WIRE_TCP_HEADER_SIZE + RTP_WIRE_RTP_HEADER_SIZE + \
	 RTP_WIRE_FRAME_HEADER_SIZE + RTP_WIRE_MAX_ALARM_BODY_SIZE)

typedef enum rtp_wire_message_type {
	RTP_WIRE_MESSAGE_THERMAL_STREAM = 1,
	RTP_WIRE_MESSAGE_ALARM = 2,
} rtp_wire_message_type_t;

/*
 * These are host-order descriptions of each wire header. Encoding functions
 * below perform the required little-/big-endian conversion explicitly.
 */
typedef struct rtp_wire_tcp_header {
	uint32_t payload_size;
} rtp_wire_tcp_header_t;

typedef struct rtp_wire_rtp_header {
	uint16_t sequence;
	uint32_t timestamp_ms;
	uint8_t marker;
	uint8_t payload_type;
} rtp_wire_rtp_header_t;

typedef struct rtp_wire_frame_header {
	uint16_t message_type;
	uint16_t flags;
	uint32_t body_size;
	uint64_t timestamp_us;
} rtp_wire_frame_header_t;

typedef struct rtp_wire_alarm_prefix {
	uint32_t alarm_type;
	uint32_t info_size;
	uint64_t timestamp_us;
} rtp_wire_alarm_prefix_t;

int rtp_wire_encode_tcp_header(const rtp_wire_tcp_header_t *header,
			       uint8_t out[RTP_WIRE_TCP_HEADER_SIZE]);
int rtp_wire_encode_rtp_header(const rtp_wire_rtp_header_t *header,
			       uint8_t out[RTP_WIRE_RTP_HEADER_SIZE]);
int rtp_wire_encode_frame_header(const rtp_wire_frame_header_t *header,
				 uint8_t out[RTP_WIRE_FRAME_HEADER_SIZE]);
int rtp_wire_alarm_body_size(const alarm_event_t *event, size_t *size);
int rtp_wire_encode_alarm_body(const alarm_event_t *event, uint8_t *out,
			       size_t capacity, size_t *written);
int rtp_wire_encode_alarm_frame(const alarm_event_t *event, uint16_t sequence,
				uint8_t *out, size_t capacity,
				size_t *written);

#endif
