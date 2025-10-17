/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_messages.h"
#include "greybus_transport.h"
#include <greybus/greybus.h>
#include <greybus/greybus_protocols.h>
#include <zephyr/dfu/mcuboot.h>

static void fw_mgmt_interface_fw_version(uint16_t cport, struct gb_message *req)
{
	int ret;
	struct mcuboot_img_header hdr;
	uint8_t active_slot = boot_fetch_active_slot();
	struct gb_fw_mgmt_interface_fw_version_response resp_data;

	ret = boot_read_bank_header(active_slot, &hdr, sizeof(hdr));
	if (ret < 0) {
		return gb_transport_message_empty_response_send(req, GB_OP_INTERNAL, cport);
	}

	resp_data.major = hdr.h.v1.sem_ver.major;
	resp_data.minor = hdr.h.v1.sem_ver.minor;
	strncpy(resp_data.firmware_tag, "gb_bcf", sizeof(resp_data.firmware_tag));

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void op_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	ARG_UNUSED(priv);

	switch (gb_message_type(msg)) {
	case GB_FW_MGMT_TYPE_INTERFACE_FW_VERSION:
		return fw_mgmt_interface_fw_version(cport, msg);
	default:
		return gb_transport_message_empty_response_send(msg, GB_OP_PROTOCOL_BAD, cport);
	}
}

struct gb_driver gb_fw_mgmt_driver = {
	.op_handler = op_handler,
};
