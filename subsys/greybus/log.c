#include <greybus/greybus.h>
#include <greybus/greybus_protocols.h>
#include "greybus_transport.h"
#include <zephyr/logging/log_backend.h>
#include <greybus-utils/manifest.h>
#include <zephyr/logging/log_backend_std.h>

#define GREYBUS_LOG_MAX_SIZE 240

struct gb_log_data {
	uint32_t log_format_current;
	uint8_t log_output_buf[GREYBUS_LOG_MAX_SIZE];
	volatile bool is_ready;
};

static struct gb_log_data priv_data = {
	.is_ready = false,
};

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

static void gb_log_send_log(uint16_t len, const char *log)
{
	struct gb_message *msg =
		gb_message_request_alloc(sizeof(len) + len, GB_LOG_TYPE_SEND_LOG, false);
	struct gb_log_send_log_request *req_data = (struct gb_log_send_log_request *)msg->payload;

	req_data->len = sys_cpu_to_le16(len);
	memcpy(req_data->msg, log, len);

	gb_transport_message_send(msg, GREYBUS_LOG_CPORT);
	gb_message_dealloc(msg);
}

static int send_logs_with_greybus(uint8_t *data, size_t length, void *ctx)
{
	uint16_t supported_data_len = MIN(64, length);
	ARG_UNUSED(ctx);

	if (priv_data.is_ready) {
		gb_log_send_log(supported_data_len, data);
	}

	return length;
}

LOG_OUTPUT_DEFINE(log_greybus_output, send_logs_with_greybus, priv_data.log_output_buf,
		  GREYBUS_LOG_MAX_SIZE);

static void gb_log_backend_process(const struct log_backend *const backend,
				   union log_msg_generic *msg)
{
	ARG_UNUSED(backend);

	uint32_t flags = log_backend_std_get_flags() & ~LOG_OUTPUT_FLAG_COLORS;

	log_format_func_t log_output_func = log_format_func_t_get(priv_data.log_format_current);

	log_output_func(&log_greybus_output, &msg->log, flags);
}

static void gb_log_backend_panic(struct log_backend const *const backend)
{
	ARG_UNUSED(backend);

	log_backend_std_panic(&log_greybus_output);
}

static void gb_log_backend_dropped(const struct log_backend *const backend, uint32_t cnt)
{
	ARG_UNUSED(backend);

	log_backend_std_dropped(&log_greybus_output, cnt);
}

static int gb_log_backend_format_set(const struct log_backend *const backend, uint32_t log_type)
{
	ARG_UNUSED(backend);

	priv_data.log_format_current = log_type;

	return 0;
}

static void gb_log_backend_notify(const struct log_backend *const backend,
				  enum log_backend_evt event, union log_backend_evt_arg *arg)
{
	ARG_UNUSED(backend);
	ARG_UNUSED(event);
	ARG_UNUSED(arg);
}

static void gb_log_backend_init(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);
}

static int gb_log_backend_is_ready(const struct log_backend *const backend)
{
	ARG_UNUSED(backend);

	return priv_data.is_ready;
}

static const struct log_backend_api log_backend_greybus_api = {
	.process = gb_log_backend_process,
	.panic = gb_log_backend_panic,
	.dropped = gb_log_backend_dropped,
	.format_set = gb_log_backend_format_set,
	.notify = gb_log_backend_notify,
	.init = gb_log_backend_init,
	.is_ready = gb_log_backend_is_ready,
};

LOG_BACKEND_DEFINE(log_backend_greybus, log_backend_greybus_api, false);

static int gb_log_init(const void *priv, uint16_t cport)
{
	log_backend_init(&log_backend_greybus);
	log_backend_activate(&log_backend_greybus, NULL);

	return 0;
}

static void gb_log_exit(const void *priv)
{
	ARG_UNUSED(priv);

	log_backend_deactivate(&log_backend_greybus);
}

static void gb_log_connected(const void *priv)
{
	ARG_UNUSED(priv);

	priv_data.is_ready = true;
}

static void gb_log_disconnected(const void *priv)
{
	priv_data.is_ready = false;
}

struct gb_driver gb_log_driver = {
	.init = gb_log_init,
	.exit = gb_log_exit,
	.op_handler = op_handler,
	.connected = gb_log_connected,
	.disconnected = gb_log_disconnected,
};
