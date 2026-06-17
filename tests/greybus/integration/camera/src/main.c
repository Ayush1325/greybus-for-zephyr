/*
 * Copyright (c) 2026 Pavithra CP, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <greybus/greybus_messages.h>
#include <greybus/greybus_protocols.h>
#include <greybus/greybus.h>
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <zephyr/fff.h>

DEFINE_FFF_GLOBALS;
LOG_MODULE_REGISTER(greybus_camera_test, CONFIG_GREYBUS_LOG_LEVEL);

struct gb_msg_with_cport gb_transport_get_message(void);

static const int camera_cport = 1;

static void *camera_setup(void)
{
	struct gb_msg_with_cport resp;
	struct gb_control_version_request *ver_req;
	struct gb_message *msg;

	msg = gb_message_request_alloc(sizeof(*ver_req), GB_CONTROL_TYPE_VERSION, false);
	ver_req = (struct gb_control_version_request *)msg->payload;
	ver_req->major = 0;
	ver_req->minor = 1;

	greybus_rx_handler(0, msg);
	resp = gb_transport_get_message();
	zassert_not_null(resp.msg, "No version response received");
	zassert_true(gb_message_is_success(resp.msg), "Version Handshake failed");

	gb_message_dealloc(resp.msg);
	return NULL;
}

ZTEST_SUITE(greybus_camera_tests, NULL, camera_setup, NULL, NULL, NULL);

ZTEST(greybus_camera_tests, test_attempt_premature_capture)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req;
	struct gb_camera_capture_request *cap_req;

	req = gb_message_request_alloc(sizeof(*cap_req), GB_CAMERA_TYPE_CAPTURE, false);
	cap_req = (struct gb_camera_capture_request *)req->payload;
	cap_req->num_frames = sys_cpu_to_le16(1);

	greybus_rx_handler(camera_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No capture response received");
	zassert_equal(resp.msg->header.result, GB_OP_INVALID,
		      "Expected GB_OP_INVALID for premature capture, got: %d",
		      resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

/*note that the tests are named as such to conincidentally follow the lexicographic order right now
 */
ZTEST(greybus_camera_tests, test_bad_configure_zero_res)
{
	struct gb_message *req;
	struct gb_msg_with_cport resp;
	struct gb_camera_configure_streams_request *cfg_req;
	size_t payload_size = sizeof(*cfg_req) + sizeof(struct gb_camera_stream_config_request);

	req = gb_message_request_alloc(payload_size, GB_CAMERA_TYPE_CONFIGURE_STREAMS, false);
	cfg_req = (struct gb_camera_configure_streams_request *)req->payload;
	cfg_req->num_streams = 1;
	cfg_req->config[0].width = sys_cpu_to_le16(0);
	cfg_req->config[0].height = sys_cpu_to_le16(480);

	greybus_rx_handler(camera_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No config response received");
	zassert_equal(resp.msg->header.result, GB_OP_INVALID,
		      "Expected GB_OP_INVALID for 0x480 resolution, got: %d",
		      resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_camera_tests, test_configure_valid)
{
	struct gb_message *req;
	struct gb_msg_with_cport resp;
	struct gb_camera_configure_streams_request *cfg_req;

	size_t payload_size = sizeof(struct gb_camera_configure_streams_request) +
			      sizeof(struct gb_camera_stream_config_request);

	req = gb_message_request_alloc(payload_size, GB_CAMERA_TYPE_CONFIGURE_STREAMS, false);
	cfg_req = (struct gb_camera_configure_streams_request *)req->payload;
	cfg_req->num_streams = 1;

	cfg_req->config[0].width = sys_cpu_to_le16(640);
	cfg_req->config[0].height = sys_cpu_to_le16(480);
	cfg_req->config[0].format = sys_cpu_to_le16(1);

	greybus_rx_handler(camera_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No config response received");
	zassert_true(gb_message_is_success(resp.msg), "Valid Config failed: %d",
		     resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_camera_tests, test_flush_prematurely)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req;

	req = gb_message_request_alloc(0, GB_CAMERA_TYPE_FLUSH, false);

	greybus_rx_handler(camera_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No flush response received");
	zassert_equal(resp.msg->header.result, GB_OP_INVALID,
		      "Expected GB_OP_INVALID for premature flush, got: %d",
		      resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_camera_tests, test_garbage_capture_payload)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req;
	struct gb_camera_capture_request *cap_req;

	size_t bad_payload_size = sizeof(*cap_req) + 1;

	req = gb_message_request_alloc(bad_payload_size, GB_CAMERA_TYPE_CAPTURE, false);
	cap_req = (struct gb_camera_capture_request *)req->payload;
	cap_req->num_frames = sys_cpu_to_le16(1);

	greybus_rx_handler(camera_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No capture response received");
	zassert_equal(resp.msg->header.result, GB_OP_INVALID,
		      "Expected GB_OP_INVALID for malformed payload, got: %d",
		      resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_camera_tests, test_valid_capture)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req;
	struct gb_camera_capture_request *cap_req;

	req = gb_message_request_alloc(sizeof(*cap_req), GB_CAMERA_TYPE_CAPTURE, false);
	cap_req = (struct gb_camera_capture_request *)req->payload;
	cap_req->num_frames = sys_cpu_to_le16(10);

	greybus_rx_handler(camera_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No capture response received");
	zassert_true(gb_message_is_success(resp.msg), "Valid Capture failed: %d",
		     resp.msg->header.result);

	gb_message_dealloc(resp.msg);
}

ZTEST(greybus_camera_tests, test_valid_flush)
{
	struct gb_msg_with_cport resp;
	struct gb_message *req;
	const struct gb_camera_flush_response *flush_resp;

	req = gb_message_request_alloc(0, GB_CAMERA_TYPE_FLUSH, false);

	greybus_rx_handler(camera_cport, req);
	resp = gb_transport_get_message();

	zassert_not_null(resp.msg, "No flush response received");
	zassert_true(gb_message_is_success(resp.msg), "Valid Flush failed: %d",
		     resp.msg->header.result);

	flush_resp = (struct gb_camera_flush_response *)resp.msg->payload;
	zassert_equal(flush_resp->request_id, sys_cpu_to_le32(0),
		      "Expected stubbed request_id of 0");

	gb_message_dealloc(resp.msg);
}
