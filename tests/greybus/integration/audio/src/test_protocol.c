/*
 * Copyright (c) 2026 Pavithra CP, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <zephyr/fff.h>
#include <greybus/greybus_protocols.h>
#include "greybus_audio.h"
#include "greybus_internal.h"

ZTEST_SUITE(audio_protocol_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(audio_protocol_tests, test_audio_connection_lifecycle)
{
	struct gb_audio_driver_data drv_data = {0};
	uint16_t test_cport = 4;

	gb_audio_driver.connected(&drv_data, test_cport);

	zassert_not_null(drv_data.dev, "Device pointer should be populated on connect");
	zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
		      "State should be STATE_UNCONFIGURED after connection");

	gb_audio_driver.disconnected(&drv_data);

	zassert_equal(drv_data.info.state, STATE_REMOVED,
		      "State should transition to STATE_REMOVED on disconnect");
}

ZTEST(audio_protocol_tests, test_audio_protocol_version)
{
	struct gb_audio_driver_data drv_data = {0};
	uint16_t test_cport = 4;

	struct gb_message *msg = gb_message_request_alloc(0, GB_AUDIO_TYPE_PROTOCOL_VERSION, false);
	zassert_not_null(msg, "Failed to allocate message");

	msg->header.operation_id = 1;

	gb_audio_driver.connected(&drv_data, test_cport);

	gb_audio_driver.op_handler(&drv_data, msg, test_cport);

	zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
		      "Protocol Version request should not alter driver state");
}
