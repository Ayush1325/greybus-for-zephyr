/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_PWM_H_
#define _GREYBUS_PWM_H_

#include <stdint.h>
#include <stdbool.h>

extern struct gb_driver gb_pwm_driver;

struct gb_pwm_channel_data {
	uint32_t duty;
	uint32_t period;
	/* false for normal. true for inverted */
	bool polarity;
};

struct gb_pwm_driver_data {
	struct gb_pwm_channel_data *channel_data;
	const struct device *dev;
	uint8_t channel_num;
};

#endif // _GREYBUS_PWM_H_
