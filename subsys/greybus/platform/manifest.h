/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SUBSYS_GREYBUS_PLATFORM_MANIFEST_H_
#define SUBSYS_GREYBUS_PLATFORM_MANIFEST_H_

#include <stddef.h>
#include <stdint.h>

uint8_t *manifest_get(void);
size_t manifest_size(void);

#endif /* SUBSYS_GREYBUS_PLATFORM_MANIFEST_H_ */
