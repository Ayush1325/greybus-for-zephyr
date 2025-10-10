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

#ifndef _GREYBUS_H_
#define _GREYBUS_H_

#include <stddef.h>
#include <stdbool.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/dlist.h>
#include <greybus/types.h>

#ifndef OK
enum {
	OK
};
#endif

#define GB_MTU                  2048
#define GB_MAX_PAYLOAD_SIZE     (GB_MTU - sizeof(struct gb_operation_hdr))
#define GB_TIMESYNC_MAX_STROBES 0x04

#define CONTROL_BUNDLE_ID 0x00
#define CONTROL_CPORT_ID  0x00

#define GB_INVALID_TYPE 0x7f

enum gb_event {
	GB_EVT_CONNECTED,
	GB_EVT_DISCONNECTED,
};

struct gb_message;

struct gb_msg_with_cport {
	uint16_t cport;
	struct gb_message *msg;
};

struct gb_transport_backend {
	void (*init)(void);
	void (*exit)(void);
	int (*listen)(uint16_t cport);
	int (*stop_listening)(uint16_t cport);
	int (*send)(uint16_t cport, const struct gb_message *msg);
};

struct gb_driver;

typedef void (*gb_operation_handler_t)(const void *priv, struct gb_message *msg, uint16_t cport);

struct gb_driver {
	/*
	 * This is the callback in which all the initialization of driver-specific
	 * data should be done. The driver should create struct device and assign
	 * it to bundle->dev. The driver should store any private data it may need
	 * in bundle->priv. The same bundle object will be passed to the driver in
	 * all subsequent greybus handler callbacks calls.
	 */
	int (*init)(const void *priv);
	/*
	 * This function is called upon driver deregistration. All private data
	 * stored in bundle should be freed and struct device should be closed.
	 */
	void (*exit)(const void *priv);
	void (*connected)(const void *priv);
	void (*disconnected)(const void *priv);

	gb_operation_handler_t op_handler;
};

struct gb_operation_hdr {
	__le16 size;
	__le16 id;
	__u8 type;
	__u8 result; /* present in response only */
	__u8 pad[2];
};

enum gb_operation_type {
	GB_TYPE_RESPONSE_FLAG = 0x80,
};

#define GB_RESPONSE(req) (req | GB_TYPE_RESPONSE_FLAG)

enum gb_operation_result {
	GB_OP_SUCCESS = 0x00,
	GB_OP_INTERRUPTED = 0x01,
	GB_OP_TIMEOUT = 0x02,
	GB_OP_NO_MEMORY = 0x03,
	GB_OP_PROTOCOL_BAD = 0x04,
	GB_OP_OVERFLOW = 0x05,
	GB_OP_INVALID = 0x06,
	GB_OP_RETRY = 0x07,
	GB_OP_NONEXISTENT = 0x08,
	GB_OP_UNKNOWN_ERROR = 0xfe,
	GB_OP_INTERNAL = 0xff,
};

int gb_init(struct gb_transport_backend *transport);
void gb_deinit(void);

int gb_listen(uint16_t cport);
int gb_stop_listening(uint16_t cport);
int gb_notify(uint16_t cport, enum gb_event event);

int greybus_rx_handler(uint16_t cport, struct gb_message *msg);

uint8_t gb_errno_to_op_result(int err);

#endif /* _GREYBUS_H_ */
