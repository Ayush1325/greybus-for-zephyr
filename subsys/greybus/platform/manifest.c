/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#ifdef CONFIG_GREYBUS_MANIFEST_BUILTIN
static const unsigned char greybus_manifest_builtin[] = {
#include "greybus_mnfb.inc"
};
#else
#define greybus_manifest_builtin NULL
#endif /* CONFIG_GREYBUS_MANIFEST_BUILTIN */

#include "../greybus-manifest.h"

uint8_t *manifest_get(void)
{
	if (IS_ENABLED(CONFIG_GREYBUS_MANIFEST_BUILTIN)) {
		return (uint8_t *)greybus_manifest_builtin;
	}

	return NULL;
}

size_t manifest_size(void)
{
	if (IS_ENABLED(CONFIG_GREYBUS_MANIFEST_BUILTIN)) {
		return sizeof(greybus_manifest_builtin);
	}

	return 0;
}
