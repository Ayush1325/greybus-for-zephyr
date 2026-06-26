/*
 * Copyright (c) 2026 Pavithra C.P., BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/device.h>
#include <zephyr/audio/codec.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(audio_test, LOG_LEVEL_INF);

/* * Redefine the internal data structure here so our test suite 
 * can look inside the driver and verify the state changes.
 */
struct audio_data {
    struct audio_codec_cfg cfg;
    int vol_out;
    int vol_in;
    bool mute_out;
    bool mute_in;
    bool is_started;
};

static const struct device *get_audio_dev(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(fake_audio));
    zassert_true(device_is_ready(dev), "The audio codec device is not ready!");
    return dev;
}

static void *audio_setup(void)
{
    const struct device *dev = get_audio_dev();
    LOG_INF("Audio Codec found and ready for testing.");
    return (void *)dev;
}

ZTEST_SUITE(audio_tests, NULL, audio_setup, NULL, NULL, NULL);

ZTEST(audio_tests, test_codec_configure)
{
    const struct device *dev = get_audio_dev();
    struct audio_data *internal_state = dev->data;
    struct audio_codec_cfg cfg = {0};
    int ret;

    cfg.dai_type = AUDIO_DAI_TYPE_I2S;
    cfg.dai_cfg.pcm.samplerate = AUDIO_PCM_RATE_48K;

    ret = audio_codec_configure(dev, &cfg);
    zassert_equal(ret, 0, "audio_codec_configure failed");

    zassert_equal(internal_state->cfg.dai_type, AUDIO_DAI_TYPE_I2S, "DAI type not saved");
    zassert_equal(internal_state->cfg.dai_cfg.pcm.samplerate, AUDIO_PCM_RATE_48K, 
                  "Sample rate not saved");
}

ZTEST(audio_tests, test_output_volume)
{
    const struct device *dev = get_audio_dev();
    struct audio_data *internal_state = dev->data;
    audio_property_value_t val;
    int ret;

    val.vol = 75; /* Set volume to 75 -sample*/
    ret = audio_codec_set_property(dev, AUDIO_PROPERTY_OUTPUT_VOLUME, AUDIO_CHANNEL_ALL, val);
    
    zassert_equal(ret, 0, "Setting output volume failed");
    zassert_equal(internal_state->vol_out, 75, "Output volume state was not updated inside driver");
}

ZTEST(audio_tests, test_input_mute)
{
    const struct device *dev = get_audio_dev();
    struct audio_data *internal_state = dev->data;
    audio_property_value_t val;
    int ret;

    /*mute and unmute*/ 
    val.mute = true; 
    ret = audio_codec_set_property(dev, AUDIO_PROPERTY_INPUT_MUTE, AUDIO_CHANNEL_ALL, val);
    
    zassert_equal(ret, 0, "Setting input mute failed");
    zassert_true(internal_state->mute_in, "Input mute state was not updated to true");

    val.mute = false; 
    ret = audio_codec_set_property(dev, AUDIO_PROPERTY_INPUT_MUTE, AUDIO_CHANNEL_ALL, val);
    zassert_false(internal_state->mute_in, "Input mute state was not updated to false");
}

ZTEST(audio_tests, test_unsupported_property)
{
    const struct device *dev = get_audio_dev();
    audio_property_value_t val = { .vol = 0 };
    int ret;

    ret = audio_codec_set_property(dev, 9999, AUDIO_CHANNEL_ALL, val);
    zassert_equal(ret, -EINVAL, "Driver should return -EINVAL for unknown properties");
}

ZTEST(audio_tests, test_start_stop)
{
    const struct device *dev = get_audio_dev();
    struct audio_data *internal_state = dev->data;
    int ret;

    ret = audio_codec_start(dev, AUDIO_DAI_DIR_TXRX);
    zassert_equal(ret, 0, "Codec start failed");
    zassert_true(internal_state->is_started, "is_started flag not set to true");

    ret = audio_codec_stop(dev, AUDIO_DAI_DIR_TXRX);
    zassert_equal(ret, 0, "Codec stop failed");
    zassert_false(internal_state->is_started, "is_started flag not set to false");
}

ZTEST(audio_tests, test_null_api_safeguard)
{
    const struct device *dev = get_audio_dev();
    int ret;

    /* * We purposefully did not implement route_input in our fake driver. 
     * This test proves that the kernel safely catches it and returns -ENOSYS 
     * instead of causing a segmentation fault.
     */
    ret = audio_codec_route_input(dev, AUDIO_CHANNEL_ALL, 0);
    zassert_equal(ret, -ENOSYS, "Unimplemented API should return -ENOSYS");
}
