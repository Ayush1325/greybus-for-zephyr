/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GREYBUS_LOG_H_
#define _GREYBUS_LOG_H_

#include <stdint.h>

extern struct gb_driver gb_log_driver;

void gb_log_send_log(uint16_t len, const char *log);

#endif // _GREYBUS_LOG_H_
