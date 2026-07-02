/*
 * Copyright (c) 2026 Pavithra C.P., BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_fake_audio_codec

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/audio/codec.h>
#include <zephyr/logging/log.h>
#include "fake_audio.h"

LOG_MODULE_REGISTER(fake_audio, LOG_LEVEL_INF);
/*
 * Only the APIs required for the Audio Control Plane MVP are
 * implemented. Unsupported operations are intentionally omitted.
 * Zephyr's codec wrappers return -ENOSYS when optional callbacks
 * are not provided.
 */
/*
 * Store the requested codec configuration.
 * The fake driver does not program real hardware; it simply retains
 * the configuration so Greybus control-plane behavior can be verified in native_sim tests.
 */
static int codec_configure(const struct device *dev, struct audio_codec_cfg *cfg)
{
	struct audio_data *data = dev->data;

	data->cfg = *cfg;
	LOG_INF("Configured: DAI Type %d, Rate %d", cfg->dai_type, cfg->dai_cfg.pcm.samplerate);

	return 0;
}

/*
 * Track stream state transitions for control-plane validation.
 * No actual audio streaming occurs in this mock driver.
 */
static void codec_start_output(const struct device *dev)
{
	LOG_INF("Output Started");
}

static void codec_stop_output(const struct device *dev)
{
	LOG_INF("Output Stopped");
}

static int codec_set_property(const struct device *dev, audio_property_t property,
			      audio_channel_t channel, audio_property_value_t val)
{
	struct audio_data *data = dev->data;

	switch (property) {
	case AUDIO_PROPERTY_OUTPUT_VOLUME:
		data->vol_out = val.vol;
		LOG_INF("Set Output Volume to %d on channel %d", val.vol, channel);
		break;
	case AUDIO_PROPERTY_OUTPUT_MUTE:
		data->mute_out = val.mute;
		LOG_INF("Set Output Mute to %d on channel %d", val.mute, channel);
		break;
	case AUDIO_PROPERTY_INPUT_VOLUME:
		data->vol_in = val.vol;
		LOG_INF("Set Input Volume to %d on channel %d", val.vol, channel);
		break;
	case AUDIO_PROPERTY_INPUT_MUTE:
		data->mute_in = val.mute;
		LOG_INF("Set Input Mute to %d on channel %d", val.mute, channel);
		break;
	default:
		LOG_ERR("Unknown property %d", property);
		return -EINVAL;
	}
	return 0;
}

static int codec_apply_properties(const struct device *dev)
{
	LOG_INF("Properties Applied");
	return 0;
}

static int codec_start(const struct device *dev, audio_dai_dir_t dir)
{
	struct audio_data *data = dev->data;

	data->is_started = true;
	LOG_INF("Codec Started for direction: %d", dir);

	return 0;
}

static int codec_stop(const struct device *dev, audio_dai_dir_t dir)
{
	struct audio_data *data = dev->data;

	data->is_started = false;
	LOG_INF("Codec Stopped for direction: %d", dir);

	return 0;
}

 static DEVICE_API(audio_codec, audio_api)= {
	.configure = codec_configure,
	.start_output = codec_start_output,
	.stop_output = codec_stop_output,
	.set_property = codec_set_property,
	.apply_properties = codec_apply_properties,
	.start = codec_start,
	.stop = codec_stop,
};

static int audio_init(const struct device *dev)
{
	LOG_INF("init: fake audio codec initialised.");
	return 0;
}

static struct audio_data audio_data_0;

DEVICE_DT_INST_DEFINE(0, audio_init, NULL, &audio_data_0, NULL, POST_KERNEL,
		      CONFIG_AUDIO_CODEC_INIT_PRIORITY, &audio_api);
