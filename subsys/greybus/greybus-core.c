/*
 * Copyright (c) 2014-2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Fabien Parent <fparent@baylibre.com>
 */
#include <zephyr/kernel.h>

#include "greybus_cport.h"
#include <unipro/unipro.h>
#include <greybus/greybus.h>
#include "greybus_messages.h"
#include "greybus_transport.h"
#include <zephyr/logging/log.h>

#include <greybus-utils/manifest.h>

#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#if defined(CONFIG_BOARD_NATIVE_POSIX_64BIT) || defined(CONFIG_BOARD_NATIVE_POSIX_32BIT) ||        \
	defined(CONFIG_BOARD_NRF52_BSIM)
#include <semaphore.h>

#define DEFAULT_STACK_SIZE PTHREAD_STACK_MIN

#else

#include <zephyr/posix/semaphore.h>

#endif

#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(greybus, CONFIG_GREYBUS_LOG_LEVEL);

#define TIMEOUT_IN_MS 1000
#define GB_PING_TYPE  0x00

#define ONE_SEC_IN_MSEC  1000
#define ONE_MSEC_IN_NSEC 1000000

#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 100
#endif

#define TIMEOUT_WD_DELAY (TIMEOUT_IN_MS * CLOCKS_PER_SEC) / ONE_SEC_IN_MSEC

#define DEBUGASSERT(x)
#define atomic_init(ptr, val) *(ptr) = val

static unsigned int cport_count;
static struct gb_cport_driver *g_cport;
static struct gb_transport_backend *transport_backend;

K_MSGQ_DEFINE(gb_rx_msgq, sizeof(struct gb_msg_with_cport), 10, 1);

K_THREAD_STACK_DEFINE(gb_rx_thread_stack, 1536);
static struct k_thread gb_rx_thread;
static k_tid_t gb_rx_threadid;

uint8_t gb_errno_to_op_result(int err)
{
	switch (err) {
	case 0:
		return GB_OP_SUCCESS;

	case ENOMEM:
	case -ENOMEM:
		return GB_OP_NO_MEMORY;

	case EINTR:
	case -EINTR:
		return GB_OP_INTERRUPTED;

	case ETIMEDOUT:
	case -ETIMEDOUT:
		return GB_OP_TIMEOUT;

	case EPROTO:
	case -EPROTO:
	case ENOSYS:
	case -ENOSYS:
		return GB_OP_PROTOCOL_BAD;

	case EINVAL:
	case -EINVAL:
		return GB_OP_INVALID;

#ifndef EOVERFLOW
#define EOVERFLOW 75
#endif

	case EOVERFLOW:
	case -EOVERFLOW:
		return GB_OP_OVERFLOW;

	case ENODEV:
	case -ENODEV:
	case ENXIO:
	case -ENXIO:
		return GB_OP_NONEXISTENT;

	case EBUSY:
	case -EBUSY:
		return GB_OP_RETRY;

	default:
		return GB_OP_UNKNOWN_ERROR;
	}
}

static void gb_process_msg(struct gb_message *msg, uint16_t cport)
{
	struct gb_driver *drv = g_cport[cport].driver;

	if (gb_message_type(msg) == GB_PING_TYPE) {
		return gb_transport_message_empty_response_send(msg, GB_OP_SUCCESS, cport);
	}

	drv->op_handler(drv, msg, cport);
}

static void gb_pending_message_worker(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;
	struct gb_msg_with_cport msg;

	while (1) {
		ret = k_msgq_get(&gb_rx_msgq, &msg, K_FOREVER);
		if (ret < 0) {
			continue;
		}

		if (g_cport[msg.cport].exit_worker) {
			gb_message_dealloc(msg.msg);
			continue;
		}

		LOG_DBG("CPort: %d, Type: %d, Result: %d, Id: %u", msg.cport,
			gb_message_type(msg.msg), msg.msg->header.result, msg.msg->header.id);
		gb_process_msg(msg.msg, msg.cport);
	}
}

int greybus_rx_handler(uint16_t cport, struct gb_message *msg)
{
	const struct gb_msg_with_cport item = {
		.cport = cport,
		.msg = msg,
	};

	if (cport >= cport_count) {
		LOG_ERR("Invalid cport number: %u", cport);
		return -EINVAL;
	}

	if (!g_cport[cport].driver || !g_cport[cport].driver->op_handler) {
		LOG_ERR("Cport %u does not have a valid driver registered", cport);
		return 0;
	}
	// LOG_HEXDUMP_DBG(data, size, "RX: ");

	k_msgq_put(&gb_rx_msgq, &item, K_FOREVER);

	return 0;
}

int gb_unregister_driver(unsigned int cport)
{
	if (cport >= cport_count || !g_cport[cport].driver || !transport_backend) {
		return -EINVAL;
	}

	if (transport_backend->stop_listening) {
		transport_backend->stop_listening(cport);
	}

	g_cport[cport].exit_worker = true;

	if (g_cport[cport].driver->exit) {
		g_cport[cport].driver->exit(cport);
	}
	g_cport[cport].driver = NULL;

	return 0;
}

int _gb_register_driver(unsigned int cport, int bundle_id, struct gb_driver *driver)
{
	char thread_name[CONFIG_THREAD_MAX_NAME_LEN];
	int retval;

	LOG_DBG("Registering Greybus driver on CP%u", cport);

	if (cport >= cport_count) {
		LOG_ERR("Invalid cport number %u", cport);
		return -EINVAL;
	}

	if (!driver) {
		LOG_ERR("No driver to register");
		return -EINVAL;
	}

	if (g_cport[cport].driver) {
		LOG_ERR("%s is already registered for CP%u", gb_driver_name(g_cport[cport].driver),
			cport);
		return -EEXIST;
	}

	if (!driver->op_handler) {
		LOG_ERR("Invalid driver");
		return -EINVAL;
	}

	if (driver->init) {
		retval = driver->init(cport);
		if (retval) {
			LOG_ERR("Can not init %s", gb_driver_name(driver));
			return retval;
		}
	}

	g_cport[cport].exit_worker = false;

	snprintf(thread_name, sizeof(thread_name), "greybus[%u]", cport);

	g_cport[cport].driver = driver;

	return 0;
}

int gb_listen(unsigned int cport)
{
	DEBUGASSERT(transport_backend);
	DEBUGASSERT(transport_backend->listen);

	if (cport >= cport_count) {
		LOG_ERR("Invalid cport number %u", cport);
		return -EINVAL;
	}

	if (!g_cport[cport].driver) {
		LOG_ERR("No driver registered! Can not connect CP%u.", cport);
		return -EINVAL;
	}

	return transport_backend->listen(cport);
}

int gb_stop_listening(unsigned int cport)
{
	DEBUGASSERT(transport_backend);
	DEBUGASSERT(transport_backend->stop_listening);

	if (cport >= cport_count) {
		LOG_ERR("Invalid cport number %u", cport);
		return -EINVAL;
	}

	if (!g_cport[cport].driver) {
		LOG_ERR("No driver registered! Can not disconnect CP%u.", cport);
		return -EINVAL;
	}

	return transport_backend->stop_listening(cport);
}

int gb_init(struct gb_transport_backend *transport)
{
	if (!transport) {
		return -EINVAL;
	}

	cport_count = unipro_cport_count();
	g_cport = calloc(1, sizeof(struct gb_cport_driver) * cport_count);
	if (!g_cport) {
		return -ENOMEM;
	}

	gb_rx_threadid = k_thread_create(
		&gb_rx_thread, gb_rx_thread_stack, K_THREAD_STACK_SIZEOF(gb_rx_thread_stack),
		gb_pending_message_worker, NULL, NULL, NULL, 5, 0, K_NO_WAIT);

	transport_backend = transport;
	transport_backend->init();

	return 0;
}

void gb_deinit(void)
{
	int i;

	if (!transport_backend) {
		return; /* gb not initialized */
	}

	for (i = 0; i < cport_count; i++) {
		gb_unregister_driver(i);
	}

	k_thread_abort(gb_rx_threadid);

	free(g_cport);

	if (transport_backend->exit) {
		transport_backend->exit();
	}
	transport_backend = NULL;
}

int gb_notify(unsigned cport, enum gb_event event)
{
	if (cport >= cport_count) {
		return -EINVAL;
	}

	if (!g_cport[cport].driver) {
		return -ENOTCONN;
	}

	switch (event) {
	case GB_EVT_CONNECTED:
		if (g_cport[cport].driver->connected) {
			g_cport[cport].driver->connected(cport);
		}
		break;

	case GB_EVT_DISCONNECTED:
		if (g_cport[cport].driver->disconnected) {
			g_cport[cport].driver->disconnected(cport);
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

struct gb_cport_driver *gb_cport_get(uint16_t cport)
{
	if (cport > manifest_get_num_cports()) {
		return NULL;
	}

	return &g_cport[cport];
}

struct gb_transport_backend *gb_transport_get(void)
{
	return transport_backend;
}
