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

#define TEST_CPORT             4
#define TEST_CONTROL_ID_VOLUME 1

/*
 * Note- I have drv_data= {0} here to get rid
 * of the errors (declared but uninitialised). If that is not preferred,
 * I will initialise drv_data with memset(&drv_data, 0, sizeof(drv_data));
 */
ZTEST_SUITE(audio_protocol_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(audio_protocol_tests, test_audio_connection_lifecycle)
{
	struct gb_audio_driver_data drv_data = {0};

	gb_audio_driver.connected(&drv_data, TEST_CPORT);

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

	struct gb_message *msg = gb_message_request_alloc(0, GB_AUDIO_TYPE_PROTOCOL_VERSION, false);
	zassert_not_null(msg, "Failed to allocate message");

	msg->header.operation_id = 1;

	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
		      "Protocol Version request should not alter driver state");
}

ZTEST(audio_protocol_tests, test_audio_set_pcm_valid)
{
	struct gb_audio_driver_data drv_data = {0};

	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	struct gb_message *msg = gb_message_request_alloc(sizeof(struct gb_audio_set_pcm_request),
							  GB_AUDIO_TYPE_SET_PCM, false);
	zassert_not_null(msg, "Failed to allocate SET_PCM message");

	struct gb_audio_set_pcm_request *req = (struct gb_audio_set_pcm_request *)msg->payload;
	req->rate = sys_cpu_to_le32(48000);
	req->channels = 2; // standard stereo audio- left and right
	req->sig_bits = 16;

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_CONFIGURED,
		      "Driver should transition to STATE_CONFIGURED after valid PCM setup");
}

ZTEST(audio_protocol_tests, test_audio_set_control_volume)
{
	struct gb_audio_driver_data drv_data = {0};

	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	struct gb_message *msg = gb_message_request_alloc(
		sizeof(struct gb_audio_set_control_request), GB_AUDIO_TYPE_SET_CONTROL, false);
	zassert_not_null(msg, "Failed to allocate SET_CONTROL message");

	struct gb_audio_set_control_request *req =
		(struct gb_audio_set_control_request *)msg->payload;
	req->control_id = TEST_CONTROL_ID_VOLUME;

	req->value.value.integer_value[0] = sys_cpu_to_le32(75); // arbitary value

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
		      "Driver state should remain stable after a control update");
}

ZTEST(audio_protocol_tests, test_audio_get_topology_size)
{
    struct gb_audio_driver_data drv_data = {0};

    gb_audio_driver.connected(&drv_data, TEST_CPORT);

    struct gb_message *msg = gb_message_request_alloc(0, GB_AUDIO_TYPE_GET_TOPOLOGY_SIZE, false);
    zassert_not_null(msg, "Failed to allocate GET_TOPOLOGY_SIZE message");

    gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

    zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
              "Driver state should remain STATE_UNCONFIGURED after topology size query");
}

ZTEST(audio_protocol_tests, test_audio_get_topology)
{
    struct gb_audio_driver_data drv_data = {0};

    gb_audio_driver.connected(&drv_data, TEST_CPORT);

    struct gb_message *msg = gb_message_request_alloc(0, GB_AUDIO_TYPE_GET_TOPOLOGY, false);
    zassert_not_null(msg, "Failed to allocate GET_TOPOLOGY message");

    gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

    zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
              "Driver state should remain STATE_UNCONFIGURED after topology query");
}
