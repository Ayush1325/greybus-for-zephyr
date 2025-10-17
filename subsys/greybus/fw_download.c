/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_messages.h"
#include "greybus_transport.h"
#include <greybus/greybus.h>
#include <greybus/greybus_protocols.h>

static void op_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	default:
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

struct gb_driver gb_fw_download_driver = {
	.op_handler = op_handler,
};
