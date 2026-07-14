/*
 * Copyright (c) 2026 Pavithra C.P., BeagleBoard.org
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <greybus/greybus.h>
#include <greybus/greybus_protocols.h>
#include "greybus_transport.h"
#include "greybus_internal.h"
#include "greybus_audio.h"
#include <zephyr/sys/byteorder.h>
#include <zephyr/audio/codec.h>

#define GB_AUDIO_VERSION_MAJOR 1
#define GB_AUDIO_VERSION_MINOR 0

LOG_MODULE_REGISTER(greybus_audio, CONFIG_GREYBUS_LOG_LEVEL);

static void gb_audio_protocol_version(uint16_t cport, struct gb_message *msg)
{
	const struct gb_audio_version_response response = {
		.major = GB_AUDIO_VERSION_MAJOR,
		.minor = GB_AUDIO_VERSION_MINOR,
	};

	gb_transport_message_response_success_send(msg, &response, sizeof(response), cport);
}

static void gb_audio_connected(const void *priv, uint16_t cport)
{
	struct gb_audio_driver_data *data = (struct gb_audio_driver_data *)priv;

	if (!data) {
		return;
	}

	data->dev = DEVICE_DT_GET(DT_NODELABEL(fake_audio));
	if (!device_is_ready(data->dev)) {
		LOG_ERR("Audio device not ready");
		return;
	}
	data->info.state = STATE_UNCONFIGURED;
	LOG_INF("Greybus Audio connected on cport %u", cport);
}

static void gb_audio_disconnected(const void *priv)
{
	struct gb_audio_driver_data *data = (struct gb_audio_driver_data *)priv;
	if (data) {
		data->info.state = STATE_REMOVED;
	}
	LOG_INF("Greybus Audio disconnected");
}

/*
* This extracts the sample rate and channels from the
* Greybus packet, formats them into a Zephyr audio_codec_cfg struct, 
* and passes them to the fake codec.
*/ 
static void gb_audio_set_pcm(uint16_t cport, struct gb_message *msg,
                             const struct gb_audio_driver_data *data)
{
    struct gb_audio_driver_data *drv_data = (struct gb_audio_driver_data *)data;
    struct gb_audio_set_pcm_request *req;
    struct audio_codec_cfg zephyr_cfg = {0};
    int ret;

    if (drv_data == NULL || drv_data->info.state < STATE_UNCONFIGURED) {
        gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
        return;
    }

    if (gb_message_payload_len(msg) < sizeof(*req)) {
        gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
        return;
    }

    req = (struct gb_audio_set_pcm_request *)msg->payload;

    zephyr_cfg.dai_type = AUDIO_DAI_TYPE_I2S;
    zephyr_cfg.dai_cfg.pcm.samplerate = sys_le32_to_cpu(req->rate);
    zephyr_cfg.dai_cfg.pcm.channels = req->channels;
    zephyr_cfg.dai_cfg.pcm.pcm_width = req->sig_bits;

    ret = audio_codec_configure(drv_data->dev, &zephyr_cfg);
    if (ret) {
        gb_transport_message_empty_response_send(msg, GB_OP_UNKNOWN_ERROR, cport);
        return;
    }

    drv_data->info.state = STATE_CONFIGURED;
    gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
}

static void gb_audio_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
  const struct gb_audio_driver_data *data = priv;

	switch (gb_message_type(msg)) {
	case GB_AUDIO_TYPE_PROTOCOL_VERSION:
		gb_audio_protocol_version(cport, msg);
		break;
  case GB_AUDIO_TYPE_SET_PCM:
        gb_audio_set_pcm(cport, msg, data);
        break;
	default:
		LOG_ERR("Invalid type: %d", gb_message_type(msg));
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
		break;
	}
}

const struct gb_driver gb_audio_driver = {
	.connected = gb_audio_connected,
	.disconnected = gb_audio_disconnected,
	.op_handler = gb_audio_handler,
};
