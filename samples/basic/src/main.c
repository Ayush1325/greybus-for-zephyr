/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>

#ifdef CONFIG_PRINT_MANIFEST
#include "greybus-utils/manifest.h"
#endif // CONFIG_PRINT_MANIFEST

int main(void)
{
#ifdef CONFIG_PRINT_MANIFEST
	static char manifest[1024];

	int ret = manifest_create(manifest, ARRAY_SIZE(manifest));

	printk("Manifest Size: %d\n", ret);
	manifest_print(manifest);
#endif // CONFIG_PRINT_MANIFEST

	return 0;
}
