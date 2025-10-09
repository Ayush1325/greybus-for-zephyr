/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Provides common heap for greybus subsystem.
 */

#ifndef _GREYBUS_HEAP_H_
#define _GREYBUS_HEAP_H_

#include <stddef.h>

void *gb_alloc(size_t len);

void gb_free(void *ptr);

#endif // _GREYBUS_HEAP_H_
