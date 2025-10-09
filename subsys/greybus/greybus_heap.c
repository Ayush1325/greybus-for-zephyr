/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Provides common heap for greybus subsystem.
 */

#include "greybus_heap.h"
#include <zephyr/kernel.h>

K_HEAP_DEFINE(greybus_heap, CONFIG_GREYBUS_HEAP_MEM_POOL_SIZE);

void *gb_alloc(size_t len)
{
	return k_heap_alloc(&greybus_heap, len, K_FOREVER);
}

void gb_free(void *ptr)
{
	k_heap_free(&greybus_heap, ptr);
}
