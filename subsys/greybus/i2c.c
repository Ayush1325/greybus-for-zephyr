/*
 * Copyright (c) 2014-2015 Google Inc.
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <greybus/greybus.h>
#include <greybus/platform.h>
#include <greybus/greybus_protocols.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "greybus_heap.h"
#include "greybus_messages.h"
#include "greybus_transport.h"

LOG_MODULE_REGISTER(greybus_i2c, CONFIG_GREYBUS_LOG_LEVEL);

static void gb_i2c_protocol_functionality(uint16_t cport, struct gb_message *req)
{
	const struct gb_i2c_functionality_response resp_data = {
		.functionality = sys_cpu_to_le32(
			GB_I2C_FUNC_I2C | GB_I2C_FUNC_SMBUS_READ_BYTE |
			GB_I2C_FUNC_SMBUS_WRITE_BYTE | GB_I2C_FUNC_SMBUS_READ_BYTE_DATA |
			GB_I2C_FUNC_SMBUS_WRITE_BYTE_DATA | GB_I2C_FUNC_SMBUS_READ_WORD_DATA |
			GB_I2C_FUNC_SMBUS_WRITE_WORD_DATA | GB_I2C_FUNC_SMBUS_READ_I2C_BLOCK |
			GB_I2C_FUNC_SMBUS_WRITE_I2C_BLOCK),
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_i2c_protocol_transfer(uint16_t cport, struct gb_message *req,
				     const struct device *dev)
{
	int i, op_count;
	uint32_t size = 0;
	int ret;
	uint8_t *write_data;
	bool read_op;
	int read_count = 0;
	struct i2c_msg *requests;

	const struct gb_i2c_transfer_op *desc;
	const struct gb_i2c_transfer_request *request =
		(const struct gb_i2c_transfer_request *)req->payload;
	struct gb_message *resp;
	struct gb_i2c_transfer_response *resp_data;
	uint16_t addr = -1;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	op_count = sys_le16_to_cpu(request->op_count);
	write_data = (uint8_t *)&request->ops[op_count];

	if (gb_message_payload_len(req) < sizeof(*request) + op_count * sizeof(request->ops[0])) {
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	for (i = 0; i < op_count; i++) {
		desc = &request->ops[i];
		read_op = (sys_le16_to_cpu(desc->flags) & GB_I2C_M_RD) ? true : false;

		if (read_op) {
			size += sys_le16_to_cpu(desc->size);
		}
	}

	resp = gb_message_alloc(size, GB_RESPONSE(req->header.type), req->header.operation_id,
				GB_OP_SUCCESS);
	if (!resp) {
		return gb_transport_message_empty_response_send(req, GB_OP_NO_MEMORY, cport);
	}
	resp_data = (struct gb_i2c_transfer_response *)resp->payload;

	requests = gb_alloc(sizeof(*requests) * op_count);
	if (!requests) {
		ret = GB_OP_NO_MEMORY;
		goto free_msg;
	}

	if (op_count > 0) {
		addr = sys_le16_to_cpu(request->ops[0].addr);
	}

	for (i = 0; i < op_count; i++) {
		desc = &request->ops[i];
		read_op = (sys_le16_to_cpu(desc->flags) & GB_I2C_M_RD) ? true : false;

		if (sys_le16_to_cpu(desc->addr) != addr) {
			/* Zephyr only allows a single address to be used */
			ret = -EINVAL;
			goto free_requests;
		}

		requests[i].flags = 0;
		requests[i].len = sys_le16_to_cpu(desc->size);

		if (read_op) {
			requests[i].flags |= I2C_MSG_READ;
			requests[i].buf = &resp_data->data[read_count];
			read_count += sys_le16_to_cpu(desc->size);
		} else {
			requests[i].buf = write_data;
			write_data += sys_le16_to_cpu(desc->size);
		}
	}

	ret = i2c_transfer(dev, requests, op_count, addr);

	gb_transport_message_send(resp, cport);
	return gb_message_dealloc(resp);

free_msg:
	gb_message_dealloc(resp);
free_requests:
	gb_free(requests);

	return gb_transport_message_empty_response_send(req, ret, cport);
}

static void gb_i2c_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct device *dev = priv;

	switch (gb_message_type(msg)) {
	case GB_I2C_TYPE_FUNCTIONALITY:
		return gb_i2c_protocol_functionality(cport, msg);
	case GB_I2C_TYPE_TRANSFER:
		return gb_i2c_protocol_transfer(cport, msg, dev);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

struct gb_driver gb_i2c_driver = {
	.op_handler = gb_i2c_handler,
};
