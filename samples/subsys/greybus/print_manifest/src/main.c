/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/sys/printk.h>
#include "greybus-utils/manifest.h"

static char manifest[1024];

int main(void)
{
	int ret = manifest_create(manifest, ARRAY_SIZE(manifest));

	printk("Cports: %d\n", GREYBUS_CPORT_COUNT);
	printk("Manifest Size: %d\n", ret);
	manifest_print(manifest);

	return 0;
}
