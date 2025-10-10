// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * This file contains some common functions for all greybus transports. Probably will remove this in
 * future since I don't see much point in making transports a vtable.
 */

#include "greybus_transport.h"
#include "greybus/greybus.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(greybus_transport_common, CONFIG_GREYBUS_LOG_LEVEL);

int gb_transport_message_send(const struct gb_message *msg, uint16_t cport)
{
	int retval;
	const struct gb_transport_backend *transport_backend = gb_transport_get_backend();

	retval = transport_backend->send(cport, msg);
	if (retval) {
		LOG_ERR("Greybus backend failed to send: error %d", retval);
	}

	return retval;
}
