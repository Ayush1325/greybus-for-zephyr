/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../greybus_transport.h"

static void init()
{
}

static int listen(uint16_t cport)
{
	return 0;
}

static struct gb_transport_backend gb_trans_backend = {
	.init = init,
	.listen = listen,
};

struct gb_transport_backend *gb_transport_backend_init(uint16_t num_cports)
{
	return &gb_trans_backend;
}
