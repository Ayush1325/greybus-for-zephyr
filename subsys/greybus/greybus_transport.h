/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_TRANSPORT_H_
#define _GREYBUS_TRANSPORT_H_

#include <greybus/greybus_messages.h>

extern const struct gb_transport_backend gb_trans_backend;

/**
 * Send message to AP.
 *
 * This function does not take ownership over the message. Hence it is the caller's responsibility
 * to cleanup.
 *
 * @param cport
 * @param msg
 */
int gb_transport_message_send(const struct gb_message *msg, uint16_t cport);

/**
 * Helper to allocate and send success response
 *
 * NOTE: This will dealloc request message.
 *
 * @param req Request message
 * @param payload Response payload. Can be NULL.
 * @param payload_len
 */
static inline void gb_transport_message_response_success_send(struct gb_message *req,
							      const void *payload,
							      size_t payload_len, uint16_t cport)
{
	/* Very low chance of allocation failure. However, if it happens, the only thing we can do
	 * is either busy wait or drop the message. Choosing to drop for now. */
	struct gb_message *resp =
		gb_message_response_alloc_from_req(payload, payload_len, req, GB_OP_SUCCESS);
	if (resp) {
		gb_transport_message_send(resp, cport);
	}

	gb_message_dealloc(req);
	gb_message_dealloc(resp);
}

/**
 * Helper to allocate and send a message with no payload.
 *
 * NOTE: This will dealloc request message.
 *
 * @param req Request message
 * @param status Response status
 */
static inline void gb_transport_message_empty_response_send(struct gb_message *req, uint8_t status,
							    uint16_t cport)
{
	/* No point in allocating empty messages on heap. */
	const struct gb_message resp = {
		.header =
			{
				.size = sizeof(struct gb_message),
				.operation_id = req->header.operation_id,
				.pad = {0, 0},
				.type = GB_RESPONSE(req->header.type),
				.result = status,
			},
	};

	gb_transport_message_send(&resp, cport);
	gb_message_dealloc(req);
}

/**
 * Get the current greybus transport backend
 */
static inline const struct gb_transport_backend *gb_transport_get_backend(void)
{
	return &gb_trans_backend;
}

#endif // _GREYBUS_TRANSPORT_H_
