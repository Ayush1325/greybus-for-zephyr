/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

#define OP_COUNT     2
#define TRANSFER_BUF 128

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

struct gb_msg_with_cport gb_transport_get_message(void);

static int i2c_emul_transfer(const struct emul *target, struct i2c_msg *msgs, int num_msgs,
			     int addr)
{
	size_t i;

	for (i = 0; i < msgs->len; i++) {
		if (msgs->flags & I2C_MSG_READ) {
			zassert_equal(addr, 0x02, "Wrong address");
			msgs->buf[i] = i;
		} else {
			zassert_equal(addr, 0x01, "Wrong address");
			zassert_equal(msgs->buf[i], i, "Unexpected data");
		}
	}

	return 0;
}

static const struct i2c_emul_api api = {
	.transfer = i2c_emul_transfer,
};

static const struct device i2c_dev = {
	.name = "test-dev",
};

static const struct emul i2c_emul_dev = {
	.dev = &i2c_dev,
};

static struct i2c_emul i2c_dev_1 = {
	.addr = 0x01,
	.api = &api,
	.target = &i2c_emul_dev,
};

static struct i2c_emul i2c_dev_2 = {
	.addr = 0x02,
	.api = &api,
	.target = &i2c_emul_dev,
};

ZTEST_SUITE(greybus_i2c_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_i2c_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

ZTEST(greybus_i2c_tests, test_transfer)
{
	int i, ret;
	uint8_t *write_data;
	struct gb_msg_with_cport resp;
	struct gb_i2c_transfer_request *req_data;
	struct gb_message *req = gb_message_request_alloc(
		sizeof(*req_data) + sizeof(struct gb_i2c_transfer_op) * OP_COUNT + TRANSFER_BUF,
		GB_I2C_TYPE_TRANSFER, false);

	ret = i2c_emul_register(dev, &i2c_dev_1);
	zassert_equal(ret, 0, "Failed to register i2c_dev_1");
	ret = i2c_emul_register(dev, &i2c_dev_2);
	zassert_equal(ret, 0, "Failed to register i2c_dev_2");

	req_data = (struct gb_i2c_transfer_request *)req->payload;
	req_data->op_count = OP_COUNT;
	write_data = (uint8_t *)&req_data->ops[OP_COUNT];

	/* Op 1 */
	req_data->ops[0].addr = 0x01;
	req_data->ops[0].size = TRANSFER_BUF;
	for (i = 0; i < TRANSFER_BUF; i++) {
		write_data[i] = i;
	}

	/* Op 2 */
	req_data->ops[1].addr = 0x02;
	req_data->ops[1].size = TRANSFER_BUF;
	req_data->ops[1].flags = GB_I2C_M_RD;

	greybus_rx_handler(1, req);
	resp = gb_transport_get_message();

	zassert_equal(resp.cport, 1, "Invalid cport");
	zassert(gb_message_is_success(resp.msg), "Request failed");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_I2C_TYPE_TRANSFER),
		      "Invalid response type");
	zassert_equal(gb_message_payload_len(resp.msg), TRANSFER_BUF, "Invalid response size");

	for (i = 0; i < TRANSFER_BUF; i++) {
		zassert_equal(resp.msg->payload[i], i, "Unexpected data");
	}

	gb_message_dealloc(resp.msg);
}
