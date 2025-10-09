/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "transport.h"

static void init()
{
}

static struct gb_transport_backend gb_trans_backend = {
	.init = init,
};

struct gb_transport_backend *gb_transport_backend_init(size_t num_cports)
{
	return &gb_trans_backend;
}
