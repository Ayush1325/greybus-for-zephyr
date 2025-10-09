/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <zephyr/kernel.h>

#define DT_DRV_COMPAT zephyr_greybus
#include <zephyr/device.h>

static int greybus_init(const struct device *bus)
{
	return 0;
}

#define DEFINE_GREYBUS(_num)                                                                       \
	DEVICE_DT_INST_DEFINE(_num, greybus_init, NULL, NULL, NULL, POST_KERNEL,                   \
			      CONFIG_GREYBUS_SERVICE_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_GREYBUS);
