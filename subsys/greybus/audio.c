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
#include <stdio.h>

#define GB_AUDIO_VERSION_MAJOR 1
#define GB_AUDIO_VERSION_MINOR 0

#define GB_AUDIO_CONTROL_ID_VOLUME 1
#define GB_AUDIO_CONTROL_ID_MUTE   2

#define NUM_TOPOLOGY_CONTROLS  (sizeof(current_topology) / sizeof(current_topology[0]))

#define NUM_DAIS     1
#define NUM_CONTROLS 2
#define NUM_WIDGETS  2
#define NUM_ROUTES   0

#define GB_AUDIO_WIDGET_ID_SPEAKER 3
#define GB_AUDIO_WIDGET_ID_MIC     4
#define AUDIO_TOPOLOGY_MAX_SIZE 512

LOG_MODULE_REGISTER(greybus_audio, CONFIG_GREYBUS_LOG_LEVEL);

/*
 * Topology Mapping Table
 * In the future, this will be populated dynamically via GET_TOPOLOGY
 * For now, we statically map ID-1->Volume, ID-2->Mute.
 */
struct gb_audio_control_map {
	uint8_t gb_control_id;
	audio_property_t zephyr_prop;
};

static const struct gb_audio_control_map current_topology[] = {
    {.gb_control_id = GB_AUDIO_CONTROL_ID_VOLUME, .zephyr_prop = AUDIO_PROPERTY_OUTPUT_VOLUME},
    {.gb_control_id = GB_AUDIO_CONTROL_ID_MUTE,   .zephyr_prop = AUDIO_PROPERTY_INPUT_MUTE},
};

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
	struct audio_codec_cfg zephyr_cfg;
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

// separate function so we can reuse it for set and get control
static bool gb_audio_lookup_property(uint8_t control_id, audio_property_t *prop)
{
	for (size_t i = 0; i < ARRAY_SIZE(current_topology); i++) {
		if (current_topology[i].gb_control_id == control_id) {
			*prop = current_topology[i].zephyr_prop;
			return true;
		}
	}
	return false;
}

static void gb_audio_set_control(uint16_t cport, struct gb_message *msg,
				 const struct gb_audio_driver_data *data)
{
	struct gb_audio_set_control_request *req;
	audio_property_t zephyr_prop;
	audio_property_value_t zephyr_val;
	int ret;

	if (data == NULL || data->info.state < STATE_UNCONFIGURED) {
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
		return;
	}

	if (gb_message_payload_len(msg) < sizeof(*req)) {
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
		return;
	}
	req = (struct gb_audio_set_control_request *)msg->payload;

	if (!gb_audio_lookup_property(req->control_id, &zephyr_prop)) {
		LOG_ERR("Unknown control ID %u", req->control_id);
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
		return;
	}
	switch (zephyr_prop) {
	case AUDIO_PROPERTY_OUTPUT_VOLUME:
		zephyr_val.vol = sys_le32_to_cpu(req->value.value.integer_value[0]);
		break;
	case AUDIO_PROPERTY_INPUT_MUTE:
		zephyr_val.mute = (sys_le32_to_cpu(req->value.value.integer_value[0]) != 0);
		break;
	default:
		LOG_ERR("Unsupported audio property %d", zephyr_prop);
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
		return;
	}
	ret = audio_codec_set_property(data->dev, zephyr_prop, AUDIO_CHANNEL_ALL, zephyr_val);
	if (ret) {
		gb_transport_message_empty_response_send(msg, GB_OP_UNKNOWN_ERROR, cport);
		return;
	}
	gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
}


static size_t gb_audio_get_topology_size_bytes(void)
{
    return sizeof(struct gb_audio_topology) + (NUM_DAIS * sizeof(struct gb_audio_dai)) + (NUM_CONTROLS * sizeof(struct gb_audio_control)) + (NUM_WIDGETS * sizeof(struct gb_audio_widget)) + (NUM_ROUTES * sizeof(struct gb_audio_route));
}

static void gb_audio_get_topology_size(uint16_t cport, struct gb_message *msg)
{
    struct gb_audio_get_topology_size_response resp_payload;
    resp_payload.size = sys_cpu_to_le16((uint16_t)gb_audio_get_topology_size_bytes());

    gb_transport_message_response_success_send(msg, &resp_payload, sizeof(resp_payload), cport);
}

static void gb_audio_get_topology(uint16_t cport, struct gb_message *msg)
{
    size_t topo_size = gb_audio_get_topology_size_bytes();
    /*
     * Use a static aligned buffer to avoid large stack allocations
    */
    static uint8_t buffer[AUDIO_TOPOLOGY_MAX_SIZE] __aligned(4);

    if (topo_size > sizeof(buffer)) {
        LOG_ERR("Topology size %zu exceeds max buffer %zu", topo_size, sizeof(buffer));
        gb_transport_message_empty_response_send(msg, GB_OP_UNKNOWN_ERROR, cport);
        return;
    }

    struct gb_audio_topology *topo = (struct gb_audio_topology *)buffer;
    memset(topo, 0, topo_size);

    topo->num_dais = NUM_DAIS;
    topo->num_controls = NUM_CONTROLS;
    topo->num_widgets = NUM_WIDGETS;
    topo->num_routes = NUM_ROUTES;

    topo->size_dais = sys_cpu_to_le32(NUM_DAIS * sizeof(struct gb_audio_dai));
    topo->size_controls = sys_cpu_to_le32(NUM_CONTROLS * sizeof(struct gb_audio_control));
    topo->size_widgets = sys_cpu_to_le32(NUM_WIDGETS * sizeof(struct gb_audio_widget));
    topo->size_routes = sys_cpu_to_le32(NUM_ROUTES * sizeof(struct gb_audio_route));
    topo->jack_type = sys_cpu_to_le32(GB_AUDIO_JACK_HEADSET);

    uint8_t *data_ptr = topo->data;

    struct gb_audio_dai *dais = (struct gb_audio_dai *)data_ptr;
    snprintf(dais[0].name, AUDIO_DAI_NAME_MAX, "Zephyr-I2S");
    dais[0].data_cport = sys_cpu_to_le16(cport);
    data_ptr += (NUM_DAIS * sizeof(struct gb_audio_dai));

    struct gb_audio_control *controls = (struct gb_audio_control *)data_ptr;
    
    snprintf(controls[0].name, AUDIO_CONTROL_NAME_MAX, "Master Playback Volume");
    controls[0].id = GB_AUDIO_CONTROL_ID_VOLUME;
    controls[0].iface = GB_AUDIO_CTL_ELEM_IFACE_MIXER;
    controls[0].access = sys_cpu_to_le32(GB_AUDIO_ACCESS_READ | GB_AUDIO_ACCESS_WRITE);
    
    snprintf(controls[1].name, AUDIO_CONTROL_NAME_MAX, "Master Capture Switch");
    controls[1].id = GB_AUDIO_CONTROL_ID_MUTE;
    controls[1].iface = GB_AUDIO_CTL_ELEM_IFACE_MIXER;
    controls[1].access = sys_cpu_to_le32(GB_AUDIO_ACCESS_READ | GB_AUDIO_ACCESS_WRITE);
    
    data_ptr += (NUM_CONTROLS * sizeof(struct gb_audio_control));

    struct gb_audio_widget *widgets = (struct gb_audio_widget *)data_ptr;
    
    snprintf(widgets[0].name, AUDIO_WIDGET_NAME_MAX, "Speaker");
    widgets[0].id = GB_AUDIO_WIDGET_ID_SPEAKER;
    widgets[0].type = GB_AUDIO_WIDGET_TYPE_SPK;
    widgets[0].state = GB_AUDIO_WIDGET_STATE_ENABLED; 
    widgets[0].ncontrols = 0;
    
    snprintf(widgets[1].name, AUDIO_WIDGET_NAME_MAX, "Mic");
    widgets[1].id = GB_AUDIO_WIDGET_ID_MIC;
    widgets[1].type = GB_AUDIO_WIDGET_TYPE_MIC;
    widgets[1].state = GB_AUDIO_WIDGET_STATE_ENABLED; 
    widgets[1].ncontrols = 0;
    
    gb_transport_message_response_success_send(msg, topo, topo_size, cport);
}

static void gb_audio_handler(const void *priv, struct gb_message *msg, uint16_t cport)
{
	const struct gb_audio_driver_data *data = priv;

	switch (gb_message_type(msg)) {
	case GB_AUDIO_TYPE_PROTOCOL_VERSION:
		gb_audio_protocol_version(cport, msg);
		break;
  case GB_AUDIO_TYPE_GET_TOPOLOGY_SIZE:
    gb_audio_get_topology_size(cport, msg);
    break;
  case GB_AUDIO_TYPE_GET_TOPOLOGY:
    gb_audio_get_topology(cport, msg);
    break;
	case GB_AUDIO_TYPE_SET_PCM:
		gb_audio_set_pcm(cport, msg, data);
		break;
	case GB_AUDIO_TYPE_SET_CONTROL:
		gb_audio_set_control(cport, msg, data);
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
