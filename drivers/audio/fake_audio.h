/*
 * Copyright (c) 2026 Pavithra C.P., BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_AUDIO_FAKE_AUDIO_H_
#define ZEPHYR_DRIVERS_AUDIO_FAKE_AUDIO_H_

#include <zephyr/audio/codec.h>

struct audio_data {
    struct audio_codec_cfg cfg;
    int vol_out;
    int vol_in;
    bool mute_out;
    bool mute_in;
    bool is_started;
};

#endif /* ZEPHYR_DRIVERS_AUDIO_FAKE_AUDIO_H_ */
