/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_cport.h"
#include "greybus-utils/manifest.h"

#define GREYBUS_MANFIEST_CPORT(_bundle, _protocol)                                                 \
	{                                                                                          \
		.bundle = _bundle,                                                                 \
		.protocol = _protocol,                                                             \
	},

#define GREYBUS_CREATE_CPORT_FROM_PROP(_node_id, _bundle)                                          \
	GREYBUS_MANFIEST_CPORT(_bundle, DT_PROP(_node_id, cport_protocol))

#define GREYBUS_CPORT_IN_BUNDLE_HANDLER(_node_id)                                                  \
	DT_FOREACH_CHILD_STATUS_OKAY_VARGS(_node_id, GREYBUS_CREATE_CPORT_FROM_PROP,               \
					   DT_PROP(_node_id, id))

#define GREYBUS_CPORT_HANDLER(_node_id)                                                            \
	IF_ENABLED(DT_NODE_HAS_COMPAT(_node_id, zephyr_greybus_bundle),                            \
		   (GREYBUS_CPORT_IN_BUNDLE_HANDLER(_node_id)))

static struct gb_cport_new cports[GREYBUS_CPORT_COUNT] = {
	DT_FOREACH_CHILD_STATUS_OKAY(_GREYBUS_BASE_NODE, GREYBUS_CPORT_HANDLER)};

struct gb_cport_new *gb_cport_get_new(uint16_t cport)
{
	return (cport >= GREYBUS_CPORT_COUNT) ? NULL : &cports[cport];
}
