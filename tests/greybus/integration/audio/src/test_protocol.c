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

	struct gb_message *msg =
		gb_message_request_alloc(0, GB_AUDIO_TYPE_GET_TOPOLOGY_SIZE, false);
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

/*
 * Here the state is STATE_UNCONFIGURED by default upon connection
 * and it should remain unconfigured as it is an invalid command
 */
ZTEST(audio_protocol_tests, test_audio_activate_tx_unconfigured)
{
	struct gb_audio_driver_data drv_data = {0};
	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	struct gb_message *msg = gb_message_request_alloc(0, GB_AUDIO_TYPE_ACTIVATE_TX, false);
	zassert_not_null(msg, "Failed to allocate ACTIVATE_TX message");

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
		      "State should not change on invalid TX activation");
}

ZTEST(audio_protocol_tests, test_audio_activate_tx_configured)
{
	struct gb_audio_driver_data drv_data = {0};
	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	drv_data.info.state = STATE_CONFIGURED;

	struct gb_message *msg = gb_message_request_alloc(0, GB_AUDIO_TYPE_ACTIVATE_TX, false);
	zassert_not_null(msg, "Failed to allocate ACTIVATE_TX message");

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_CONFIGURED,
		      "State should remain STATE_CONFIGURED after valid TX activation");
}

ZTEST(audio_protocol_tests, test_audio_send_data_unconfigured)
{
	struct gb_audio_driver_data drv_data = {0};
	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	struct gb_message *msg = gb_message_request_alloc(
		sizeof(struct gb_audio_send_data_request) + 64, GB_AUDIO_TYPE_SEND_DATA, false);
	zassert_not_null(msg, "Failed to allocate SEND_DATA message");

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_UNCONFIGURED,
		      "State should remain STATE_UNCONFIGURED after rejecting SEND_DATA");
}

ZTEST(audio_protocol_tests, test_audio_send_data_configured)
{
	struct gb_audio_driver_data drv_data = {0};
	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	drv_data.info.state = STATE_CONFIGURED;

	struct gb_message *msg = gb_message_request_alloc(
		sizeof(struct gb_audio_send_data_request) + 128, GB_AUDIO_TYPE_SEND_DATA, false);
	zassert_not_null(msg, "Failed to allocate SEND_DATA message");

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_CONFIGURED,
		      "State should remain STATE_CONFIGURED after successful SEND_DATA");
}

ZTEST(audio_protocol_tests, test_audio_deactivate_tx)
{
	struct gb_audio_driver_data drv_data = {0};
	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	drv_data.info.state = STATE_CONFIGURED;

	struct gb_message *msg = gb_message_request_alloc(
		sizeof(struct gb_audio_deactivate_tx_request), GB_AUDIO_TYPE_DEACTIVATE_TX, false);
	zassert_not_null(msg, "Failed to allocate DEACTIVATE_TX message");

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_CONFIGURED,
		      "State should remain STATE_CONFIGURED after DEACTIVATE_TX");
}

ZTEST(audio_protocol_tests, test_audio_deactivate_rx)
{
	struct gb_audio_driver_data drv_data = {0};
	gb_audio_driver.connected(&drv_data, TEST_CPORT);

	drv_data.info.state = STATE_CONFIGURED;

	struct gb_message *msg = gb_message_request_alloc(
		sizeof(struct gb_audio_deactivate_rx_request), GB_AUDIO_TYPE_DEACTIVATE_RX, false);
	zassert_not_null(msg, "Failed to allocate DEACTIVATE_RX message");

	gb_audio_driver.op_handler(&drv_data, msg, TEST_CPORT);

	zassert_equal(drv_data.info.state, STATE_CONFIGURED,
		      "State should remain STATE_CONFIGURED after DEACTIVATE_RX");
}

ZTEST(audio_protocol_tests, test_audio_jack_event)
{
	int ret;
	/* simulating a headphone insertion into widget ID 4 */
	ret = gb_audio_send_jack_event(TEST_CPORT, 4, GB_AUDIO_JACK_HEADPHONE,
				       GB_AUDIO_JACK_EVENT_INSERTION);

	zassert_equal(ret, -42, "Expected transport error -42 due to missing backend");
}

ZTEST(audio_protocol_tests, test_audio_button_event)
{
	int ret;
	/*Simulating pressing the ID 1 (play/pause) */
	ret = gb_audio_send_button_event(TEST_CPORT, 4, 1, GB_AUDIO_BUTTON_EVENT_PRESS);
	// since native_sim lacks a physical transport backend
	zassert_equal(ret, -42, "Expected transport error -42 due to missing backend");
}
