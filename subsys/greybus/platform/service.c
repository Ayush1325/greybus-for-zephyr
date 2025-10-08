/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <greybus/greybus.h>
#include <greybus/platform.h>
#include <greybus-utils/manifest.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(greybus_service, CONFIG_GREYBUS_LOG_LEVEL);

#include "transport.h"
#include "manifest.h"
#include "certificate.h"

/* Currently only one greybus instance is supported */
#define GREYBUS_BUS_NAME "greybus0"

static struct gb_transport_backend *xport;
static size_t num_cports;

unsigned int unipro_cport_count(void)
{
	return num_cports;
}

const struct gb_transport_backend *gb_transport_get_backend(void)
{
	return xport;
}

static int greybus_service_init(void)
{
	int r;
	uint8_t *mnfb;

	if (xport != NULL) {
		LOG_ERR("service already initialized");
		return -EALREADY;
	}

	r = greybus_tls_init();
	if (r < 0) {
		LOG_ERR("gb_tls_init() failed: %d", r);
		return r;
	}
	LOG_DBG("Greybus initializing..");

	mnfb = manifest_get();
	if (!mnfb) {
		LOG_ERR("failed to get mnfb");
		return -ENOENT;
	}

	r = manifest_parse(mnfb, manifest_size());
	if (r != true) {
		LOG_ERR("failed to parse mnfb");
		return -EINVAL;
	}

	num_cports = manifest_get_num_cports();
	if (num_cports == 0) {
		LOG_ERR("no cports are defined");
		return -EINVAL;
	}

	xport = gb_transport_backend_init(num_cports);
	if (xport == NULL) {
		LOG_ERR("failed to get transport");
		return -EIO;
	}

	r = gb_init((struct gb_transport_backend *)xport);
	if (r < 0) {
		LOG_ERR("gb_init() failed: %d", r);
		goto clear_mnfb;
	}

	enable_cports();
	LOG_INF("Greybus is active");
	return 0;

clear_mnfb:
	return r;
}

SYS_INIT(greybus_service_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
