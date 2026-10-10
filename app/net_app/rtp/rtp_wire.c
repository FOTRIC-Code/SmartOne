#include "rtp_wire.h"

#include "libs/binary/binary_writer.h"
#include "net_common/net_enums.h"

#include <string.h>

static int alarm_detail_size(int type, size_t *size)
{
	if (!size)
		return -1;

	switch (type) {
	case NET_ALARM_INTRUSION:
		*size = 4;
		return 0;
	case NET_ALARM_HIGH_TEMP:
		*size = 8;
		return 0;
	case NET_ALARM_BODY_TEMP:
		*size = 12;
		return 0;
	default:
		return -1;
	}
}

int rtp_wire_encode_tcp_header(const rtp_wire_tcp_header_t *header,
			       uint8_t out[RTP_WIRE_TCP_HEADER_SIZE])
{
	if (!header || !out)
		return -1;

	memset(out, 0, RTP_WIRE_TCP_HEADER_SIZE);
	out[0] = RTP_WIRE_MARKER;
	bin_write_be32(out + 4, header->payload_size);
	return 0;
}

int rtp_wire_encode_rtp_header(const rtp_wire_rtp_header_t *header,
			       uint8_t out[RTP_WIRE_RTP_HEADER_SIZE])
{
	if (!header || !out || header->payload_type > 0x7fU)
		return -1;

	memset(out, 0, RTP_WIRE_RTP_HEADER_SIZE);
	out[0] = (uint8_t)(RTP_WIRE_RTP_VERSION << 6);
	out[1] = (uint8_t)((header->marker ? 0x80U : 0U) |
			   header->payload_type);
	bin_write_be16(out + 2, header->sequence);
	bin_write_be32(out + 4, header->timestamp_ms);
	return 0;
}

int rtp_wire_encode_frame_header(const rtp_wire_frame_header_t *header,
				 uint8_t out[RTP_WIRE_FRAME_HEADER_SIZE])
{
	if (!header || !out)
		return -1;

	bin_write_le16(out, header->message_type);
	bin_write_le16(out + 2, header->flags);
	bin_write_le32(out + 4, header->body_size);
	bin_write_le64(out + 8, header->timestamp_us);
	return 0;
}

int rtp_wire_alarm_body_size(const alarm_event_t *event, size_t *size)
{
	size_t detail_size;
	size_t message_size;
	size_t video_path_size;

	if (!event || !size ||
	    alarm_detail_size(event->type, &detail_size) != 0)
		return -1;

	message_size = strnlen(event->message, ALARM_EVENT_MESSAGE_MAX);
	video_path_size = strnlen(event->video_path,
				  RTP_WIRE_MAX_ALARM_VIDEO_PATH_SIZE);
	*size = RTP_WIRE_ALARM_PREFIX_SIZE + detail_size +
		RTP_WIRE_MESSAGE_LENGTH_SIZE + message_size +
		RTP_WIRE_MESSAGE_LENGTH_SIZE + video_path_size;
	return 0;
}

int rtp_wire_encode_alarm_body(const alarm_event_t *event, uint8_t *out,
			       size_t capacity, size_t *written)
{
	rtp_wire_alarm_prefix_t prefix;
	size_t body_size;
	size_t detail_size;
	size_t message_size;
	size_t video_path_size;
	size_t offset = RTP_WIRE_ALARM_PREFIX_SIZE;

	if (!event || !out || !written ||
	    alarm_detail_size(event->type, &detail_size) != 0 ||
	    rtp_wire_alarm_body_size(event, &body_size) != 0 ||
	    capacity < body_size)
		return -1;

	message_size = strnlen(event->message, ALARM_EVENT_MESSAGE_MAX);
	video_path_size = strnlen(event->video_path,
				  RTP_WIRE_MAX_ALARM_VIDEO_PATH_SIZE);
	prefix.alarm_type = (uint32_t)event->type;
	prefix.info_size = (uint32_t)(detail_size +
				     RTP_WIRE_MESSAGE_LENGTH_SIZE +
				     message_size +
				     RTP_WIRE_MESSAGE_LENGTH_SIZE +
				     video_path_size);
	prefix.timestamp_us = event->timestamp_us;
	bin_write_le32(out, prefix.alarm_type);
	bin_write_le32(out + 4, prefix.info_size);
	bin_write_le64(out + 8, prefix.timestamp_us);

	if (event->type == NET_ALARM_INTRUSION) {
		bin_write_le32(out + offset, (uint32_t)event->motion_level);
		offset += 4;
	} else if (event->type == NET_ALARM_HIGH_TEMP) {
		bin_write_f32_le(out + offset, event->temperature);
		bin_write_f32_le(out + offset + 4, event->threshold);
		offset += 8;
	} else {
		bin_write_f32_le(out + offset, event->temperature);
		bin_write_f32_le(out + offset + 4, event->baseline);
		bin_write_f32_le(out + offset + 8, event->slope);
		offset += 12;
	}
	bin_write_le32(out + offset, (uint32_t)message_size);
	offset += 4;
	if (message_size)
		memcpy(out + offset, event->message, message_size);
	offset += message_size;
	bin_write_le32(out + offset, (uint32_t)video_path_size);
	offset += 4;
	if (video_path_size)
		memcpy(out + offset, event->video_path, video_path_size);
	*written = body_size;
	return 0;
}

int rtp_wire_encode_alarm_frame(const alarm_event_t *event, uint16_t sequence,
				uint8_t *out, size_t capacity,
				size_t *written)
{
	rtp_wire_tcp_header_t tcp_header;
	rtp_wire_rtp_header_t rtp_header;
	rtp_wire_frame_header_t frame_header;
	size_t body_size;
	size_t encoded_body_size;
	size_t offset = 0;

	if (!event || !out || !written ||
	    rtp_wire_alarm_body_size(event, &body_size) != 0 ||
	    body_size > UINT32_MAX ||
	    capacity < RTP_WIRE_TCP_HEADER_SIZE + RTP_WIRE_RTP_HEADER_SIZE +
			       RTP_WIRE_FRAME_HEADER_SIZE + body_size)
		return -1;

	tcp_header.payload_size = (uint32_t)(RTP_WIRE_RTP_HEADER_SIZE +
					    RTP_WIRE_FRAME_HEADER_SIZE +
					    body_size);
	rtp_header.sequence = sequence;
	rtp_header.timestamp_ms = (uint32_t)(event->timestamp_us / 1000ULL);
	rtp_header.marker = 1;
	rtp_header.payload_type = RTP_WIRE_RTP_PAYLOAD_TYPE;
	frame_header.message_type = RTP_WIRE_MESSAGE_ALARM;
	frame_header.flags = 0;
	frame_header.body_size = (uint32_t)body_size;
	frame_header.timestamp_us = event->timestamp_us;

	if (rtp_wire_encode_tcp_header(&tcp_header, out + offset) != 0)
		return -1;
	offset += RTP_WIRE_TCP_HEADER_SIZE;
	if (rtp_wire_encode_rtp_header(&rtp_header, out + offset) != 0)
		return -1;
	offset += RTP_WIRE_RTP_HEADER_SIZE;
	if (rtp_wire_encode_frame_header(&frame_header, out + offset) != 0)
		return -1;
	offset += RTP_WIRE_FRAME_HEADER_SIZE;
	if (rtp_wire_encode_alarm_body(event, out + offset,
				       capacity - offset,
				       &encoded_body_size) != 0)
		return -1;
	offset += encoded_body_size;

	*written = offset;
	return 0;
}
