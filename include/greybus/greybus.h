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

#include <greybus/types.h>

struct gb_message;

struct gb_transport_backend {
	int (*init)(void);
	void (*exit)(void);
	int (*listen)(uint16_t cport);
	int (*stop_listening)(uint16_t cport);
	int (*send)(uint16_t cport, const struct gb_message *msg);
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

int gb_init(const struct gb_transport_backend *transport);
void gb_deinit(void);

#endif /* _GREYBUS_H_ */
