/*
 * Copyright (c) 2026 Pavithra CP BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GREYBUS_AUDIO_H__
#define __GREYBUS_AUDIO_H__

#include <zephyr/device.h>
#include <greybus/greybus.h>

#define STATE_REMOVED      0
#define STATE_INSERTED     1
#define STATE_UNCONFIGURED 2
#define STATE_CONFIGURED   3
#define STATE_STREAMING    4

struct gb_audio_driver_data {
	const struct device *dev;
	struct {
		uint8_t state;
	} info;
};

struct gb_audio_version_response {
	uint8_t major;
	uint8_t minor;
} __attribute__((packed));

extern const struct gb_driver gb_audio_driver;

#endif /*__GREYBUS_AUDIO_H__*/
