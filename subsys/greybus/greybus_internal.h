/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_INTERNAL_H_
#define _GREYBUS_INTERNAL_H_

#include "greybus_messages.h"

enum gb_event {
	GB_EVT_CONNECTED,
	GB_EVT_DISCONNECTED,
};

int gb_listen(uint16_t cport);
int gb_stop_listening(uint16_t cport);
int gb_notify(uint16_t cport, enum gb_event event);

int greybus_rx_handler(uint16_t cport, struct gb_message *msg);

uint8_t gb_errno_to_op_result(int err);

#endif // _GREYBUS_INTERNAL_H_
