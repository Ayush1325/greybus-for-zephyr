/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_GPIO_H_
#define _GREYBUS_GPIO_H_

#include <zephyr/drivers/gpio.h>

struct gb_gpio_driver_data {
	struct gpio_callback cb;
	const struct device *const dev;
	uint16_t cport;
};

#endif // _GREYBUS_GPIO_H_
