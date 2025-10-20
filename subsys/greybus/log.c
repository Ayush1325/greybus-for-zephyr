#include <greybus/greybus.h>
#include <greybus/greybus_protocols.h>
#include "greybus_transport.h"

static void op_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	case GB_RESPONSE(GB_LOG_TYPE_SEND_LOG):
		return gb_message_dealloc(msg);
	default:
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

struct gb_driver gb_log_driver = {
	.op_handler = op_handler,
};

void gb_log_send_log(uint16_t len, const char *log)
{
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(len) + len, GB_LOG_TYPE_SEND_LOG, false);
	struct gb_log_send_log_request *req_data = (struct gb_log_send_log_request *)msg->payload;

	req_data->len = sys_cpu_to_le16(len);
	memcpy(req_data->msg, log, len);

	gb_transport_message_send(msg, 3);
	gb_message_dealloc(msg);
}
