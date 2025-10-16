/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_UART_H_
#define _GREYBUS_UART_H_

#include <zephyr/drivers/uart.h>

extern struct gb_driver gb_uart_driver;

struct gb_uart_driver_data {
	const struct device *dev;
	uint16_t cport;
};

#endif // _GREYBUS_UART_H_
