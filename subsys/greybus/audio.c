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

#define GB_AUDIO_VERSION_MAJOR 1
#define GB_AUDIO_VERSION_MINOR 0

LOG_MODULE_REGISTER(greybus_audio, CONFIG_GREYBUS_LOG_LEVEL);

static void gb_audio_protocol_version(uint16_t cport, struct gb_message *msg)
{
   const struct gb_audio_version_response response= {
        .major = GB_AUDIO_VERSION_MAJOR,
        .minor = GB_AUDIO_VERSION_MINOR,
    };

    gb_transport_message_response_success_send(msg, &response, sizeof(response), cport);
}

static void gb_audio_connected(const void *priv, uint16_t cport)
{
    struct gb_audio_driver_data *data = (struct gb_audio_driver_data *)priv;

    if (!data) return;

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

static void gb_audio_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
    switch (gb_message_type(msg)) {
    case GB_AUDIO_TYPE_PROTOCOL_VERSION:
        gb_audio_protocol_version(cport, msg);
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
