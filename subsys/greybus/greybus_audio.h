/*
 * Copyright (c) 2026 Pavithra CP BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GREYBUS_AUDIO_H__
#define __GREYBUS_AUDIO_H__

#include <zephyr/device.h>

struct gb_audio_driver_data {
    const struct device *dev;
    struct {
        uint8_t state;
    } info;
};

#endif /*__GREYBUS_AUDIO_H__*/
