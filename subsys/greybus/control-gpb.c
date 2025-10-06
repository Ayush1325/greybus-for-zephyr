/*
 * Copyright (c) 2015 Google Inc.
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
 *
 * Author: Viresh Kumar <viresh.kumar@linaro.org>
 */

#include <greybus/ara_version.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <greybus/greybus.h>
#include <unipro/unipro.h>
#include <zephyr/device.h>
#include <greybus/timesync.h>
#include <greybus-utils/manifest.h>
#include "greybus_messages.h"
#include "greybus_transport.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(greybus_control, CONFIG_GREYBUS_LOG_LEVEL);

#include "control-gb.h"

static uint8_t gb_control_protocol_version(uint16_t cport, struct gb_message *req)
{
	const struct gb_control_proto_version_response resp_data = {
		.major = GB_CONTROL_VERSION_MAJOR,
		.minor = GB_CONTROL_VERSION_MINOR,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_control_get_manifest_size(uint16_t cport, struct gb_message *req)
{
	const struct gb_control_get_manifest_size_response resp_data = {
		.size = sys_cpu_to_le16(get_manifest_size()),
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_control_get_manifest(uint16_t cport, struct gb_message *req)
{
	gb_transport_message_response_success_send(req, get_manifest_blob(), get_manifest_size(),
						   cport);
}

static void gb_control_connected(uint16_t cport, struct gb_message *req)
{
	int retval;
	const struct gb_control_connected_request *req_data =
		(const struct gb_control_connected_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*req_data)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	retval = gb_listen(cport);
	if (retval) {
		LOG_ERR("Can not connect cport %d: error %d", sys_le16_to_cpu(req_data->cport_id),
			retval);
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	retval = gb_notify(cport, GB_EVT_CONNECTED);
	if (retval) {
		goto error_notify;
	}

	unipro_enable_fct_tx_flow(cport);
	return gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);

error_notify:
	gb_stop_listening(cport);
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_disconnected(uint16_t cport, struct gb_message *req)
{
	int retval;
	const struct gb_control_connected_request *req_data =
		(const struct gb_control_connected_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*req_data)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	unipro_disable_fct_tx_flow(cport);

	retval = gb_notify(cport, GB_EVT_DISCONNECTED);
	if (retval) {
		LOG_ERR("Cannot notify GB driver of disconnect event.");
		/*
		 * don't return, we still want to reset the cport and stop listening
		 * on the CPort.
		 */
	}

	unipro_reset_cport(cport, NULL, NULL);

	retval = gb_stop_listening(cport);
	if (retval) {
		LOG_ERR("Can not disconnect cport %d: error %d", cport, retval);
	}

	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_disconnecting(uint16_t cport, struct gb_message *req)
{
	gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
}

static void gb_control_pm_stub(uint16_t cport, struct gb_message *req)
{
	const struct gb_control_bundle_pm_response resp_data = {
		.status = GB_CONTROL_BUNDLE_PM_OK,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_control_interface_version(uint16_t cport, struct gb_message *req)
{
	const struct gb_control_interface_version_response resp_data = {
		.major = sys_cpu_to_le16(GB_INTERFACE_VERSION_MAJOR),
		.minor = sys_cpu_to_le16(GB_INTERFACE_VERSION_MINOR),
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static void gb_control_timesync_enable(uint16_t cport, struct gb_message *req)
{
	uint8_t count;
	uint64_t frame_time;
	uint32_t strobe_delay;
	uint32_t refclk;
	const struct gb_control_timesync_enable_request *req_data =
		(const struct gb_control_timesync_enable_request *)req->payload;
	int retval;

	if (gb_message_payload_len(req) < sizeof(*req_data)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	count = req_data->count;
	frame_time = sys_le64_to_cpu(req_data->frame_time);
	strobe_delay = sys_le32_to_cpu(req_data->strobe_delay);
	refclk = sys_le32_to_cpu(req_data->refclk);

	retval = timesync_enable(count, frame_time, strobe_delay, refclk);
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_timesync_disable(uint16_t cport, struct gb_message *req)
{
	int retval = timesync_disable();
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_timesync_authoritative(uint16_t cport, struct gb_message *req)
{
	uint64_t frame_time[GB_TIMESYNC_MAX_STROBES];
	const struct gb_control_timesync_authoritative_request *req_data =
		(const struct gb_control_timesync_authoritative_request *)req->payload;
	int i;
	int retval;

	if (gb_message_payload_len(req) < sizeof(*req_data)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	for (i = 0; i < GB_TIMESYNC_MAX_STROBES; i++) {
		frame_time[i] = sys_le64_to_cpu(req_data->frame_time[i]);
	}

	retval = timesync_authoritative(frame_time);
	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_timesync_get_last_event(uint16_t cport, struct gb_message *req)
{
	uint64_t frame_time;
	struct gb_control_timesync_get_last_event_response resp_data;
	int retval;

	retval = timesync_get_last_event(&frame_time);
	if (!retval) {
		resp_data.frame_time = sys_cpu_to_le64(frame_time);
		return gb_transport_message_response_success_send(req, &resp_data,
								  sizeof(resp_data), cport);
	}

	gb_transport_message_empty_response_send(req, gb_errno_to_op_result(retval), cport);
}

static void gb_control_handler(struct gb_driver *drv, struct gb_message *msg, uint16_t cport)
{
	switch (gb_message_type(msg)) {
	case GB_CONTROL_TYPE_PROTOCOL_VERSION:
		return gb_control_protocol_version(cport, msg);
	case GB_CONTROL_TYPE_GET_MANIFEST_SIZE:
		return gb_control_get_manifest_size(cport, msg);
	case GB_CONTROL_TYPE_GET_MANIFEST:
		return gb_control_get_manifest(cport, msg);
	case GB_CONTROL_TYPE_CONNECTED:
		return gb_control_connected(cport, msg);
	case GB_CONTROL_TYPE_DISCONNECTED:
		return gb_control_disconnected(cport, msg);
	case GB_CONTROL_TYPE_INTERFACE_VERSION:
		return gb_control_interface_version(cport, msg);
	case GB_CONTROL_TYPE_DISCONNECTING:
		return gb_control_disconnecting(cport, msg);
	case GB_CONTROL_TYPE_BUNDLE_ACTIVATE:
	case GB_CONTROL_TYPE_BUNDLE_SUSPEND:
	case GB_CONTROL_TYPE_BUNDLE_RESUME:
	case GB_CONTROL_TYPE_BUNDLE_DEACTIVATE:
	case GB_CONTROL_TYPE_INTF_SUSPEND_PREPARE:
	case GB_CONTROL_TYPE_INTF_DEACTIVATE_PREPARE:
		return gb_control_pm_stub(cport, msg);
	/* XXX SW-4136: see control-gb.h */
	/*GB_HANDLER(GB_CONTROL_TYPE_INTF_POWER_STATE_SET, gb_control_intf_pwr_set),
	GB_HANDLER(GB_CONTROL_TYPE_BUNDLE_POWER_STATE_SET, gb_control_bundle_pwr_set),*/
	case GB_CONTROL_TYPE_TIMESYNC_ENABLE:
		return gb_control_timesync_enable(cport, msg);
	case GB_CONTROL_TYPE_TIMESYNC_DISABLE:
		return gb_control_timesync_disable(cport, msg);
	case GB_CONTROL_TYPE_TIMESYNC_AUTHORITATIVE:
		return gb_control_timesync_authoritative(cport, msg);
	case GB_CONTROL_TYPE_TIMESYNC_GET_LAST_EVENT:
		return gb_control_timesync_get_last_event(cport, msg);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

struct gb_driver control_driver = {
	.op_handler = gb_control_handler,
};

void gb_control_register(int cport, int bundle)
{
	gb_register_driver(cport, bundle, &control_driver);
	unipro_attr_local_write(T_CPORTFLAGS,
				CPORT_FLAGS_CSV_N | CPORT_FLAGS_CSD_N | CPORT_FLAGS_E2EFC, cport);
	unipro_enable_fct_tx_flow(cport);
	gb_listen(cport);
}
