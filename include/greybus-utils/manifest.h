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
 */

#ifndef _GREYBUS_UTILS_MANIFEST_H_
#define _GREYBUS_UTILS_MANIFEST_H_

#include <zephyr/sys/dlist.h>
#include <zephyr/types.h>
#include <zephyr/devicetree.h>

#define _GREYBUS_BASE_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_greybus)

#define _GREYBUS_CPORT_COUNTER(_node_id)                                                           \
	COND_CODE_1(DT_NODE_HAS_COMPAT(_node_id, zephyr_greybus_bundle),                           \
		    (DT_CHILD_NUM_STATUS_OKAY(_node_id)), (0))

#define GREYBUS_CPORT_COUNT                                                                        \
	(DT_FOREACH_CHILD_STATUS_OKAY_SEP(_GREYBUS_BASE_NODE, _GREYBUS_CPORT_COUNTER, (+)))

typedef void (*manifest_handler)(unsigned char *manifest_file, int device_id, int manifest_number);
void enable_cports(void);

#endif
