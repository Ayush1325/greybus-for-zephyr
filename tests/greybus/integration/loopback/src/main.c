/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>

#define REQ_SIZE 256

K_SEM_DEFINE(rx_sem, 0, 1);

static struct gb_message *last_msg;

static int trans_init()
{
	return 0;
}

static int trans_listen(uint16_t cport)
{
	return 0;
}

static int trans_send(uint16_t cport, const struct gb_message *msg)
{
	last_msg = gb_message_copy(msg);
	k_sem_give(&rx_sem);

	return 0;
}

const struct gb_transport_backend gb_trans_backend = {
	.init = trans_init,
	.listen = trans_listen,
	.send = trans_send,
};

ZTEST_SUITE(greybus_loopback_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_loopback_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

ZTEST(greybus_loopback_tests, test_ping)
{
	struct gb_message *req = gb_message_request_alloc(0, GB_LOOPBACK_TYPE_PING, false);

	greybus_rx_handler(1, req);
	k_sem_take(&rx_sem, K_FOREVER);
	zassert_true(gb_message_is_success(last_msg), "Greybus loopback ping failed");
	zassert_equal(gb_message_type(last_msg), GB_RESPONSE(GB_LOOPBACK_TYPE_PING),
		      "Invalid request response");
	zassert_equal(gb_message_payload_len(last_msg), 0,
		      "Greybus ping request should have empty response");
}

ZTEST(greybus_loopback_tests, test_sink)
{
	size_t i;
	struct gb_message *req =
		gb_message_request_alloc(sizeof(struct gb_loopback_transfer_request) + REQ_SIZE,
					 GB_LOOPBACK_TYPE_SINK, false);
	struct gb_loopback_transfer_request *req_data =
		(struct gb_loopback_transfer_request *)req->payload;

	req_data->len = sys_cpu_to_le32(REQ_SIZE);
	for (i = 0; i < REQ_SIZE; i++) {
		req_data->data[i] = i;
	}

	greybus_rx_handler(1, req);
	k_sem_take(&rx_sem, K_FOREVER);
	zassert_true(gb_message_is_success(last_msg), "Greybus loopback sink failed");
	zassert_equal(gb_message_type(last_msg), GB_RESPONSE(GB_LOOPBACK_TYPE_SINK),
		      "Invalid request response");
	zassert_equal(gb_message_payload_len(last_msg), 0,
		      "Greybus sink request should have empty response");
}

ZTEST(greybus_loopback_tests, test_transfer)
{
	size_t i;
	struct gb_message *req_copy;
	struct gb_message *req =
		gb_message_request_alloc(sizeof(struct gb_loopback_transfer_request) + REQ_SIZE,
					 GB_LOOPBACK_TYPE_TRANSFER, false);
	struct gb_loopback_transfer_request *req_data =
		(struct gb_loopback_transfer_request *)req->payload;

	req_data->len = sys_cpu_to_le32(REQ_SIZE);
	for (i = 0; i < REQ_SIZE; i++) {
		req_data->data[i] = i;
	}

	req_copy = gb_message_copy(req);
	greybus_rx_handler(1, req_copy);
	k_sem_take(&rx_sem, K_FOREVER);
	zassert_true(gb_message_is_success(last_msg), "Greybus loopback sink failed");
	zassert_equal(gb_message_type(last_msg), GB_RESPONSE(GB_LOOPBACK_TYPE_TRANSFER),
		      "Invalid request response");
	zassert_equal(gb_message_payload_len(last_msg), gb_message_payload_len(req),
		      "Greybus transfer request should have same size response");
	zassert_equal(memcmp(req->payload, last_msg->payload, gb_message_payload_len(last_msg)), 0,
		      "Response data should be same as request");

	gb_message_dealloc(req);
}
