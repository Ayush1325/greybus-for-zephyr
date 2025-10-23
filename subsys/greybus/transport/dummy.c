/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../greybus_transport.h"
#include <greybus/greybus.h>

static int init()
{
	return 0;
}

static int listen(uint16_t cport)
{
	return 0;
}

const struct gb_transport_backend gb_trans_backend = {
	.init = init,
	.listen = listen,
};
