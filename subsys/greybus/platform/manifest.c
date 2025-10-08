/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <greybus-utils/manifest.h>
#include "../greybus-manifest.h"
#include "../greybus_cport.h"

struct greybus_manifest_cport {
	uint8_t bundle;
	uint8_t protocol;
};

#define GREYBUS_VENDOR_STRING_ID  1
#define GREYBUS_PRODUCT_STRING_ID 2

#define _set_greybus_string(_desc, _id, _str)                                                      \
	set_greybus_string(_desc, _id, GREYBUS_STRLEN(_str), GREYBUS_MANIFEST_STRING_SIZE(_str),   \
			   _str)

/* sizeof will include NULL-terminator. This is not required in greybus. */
#define GREYBUS_STRLEN(s)           (sizeof(s) - 1)
#define _GREYBUS_DESCRIPTOR_SIZE(n) (sizeof(struct greybus_descriptor_header) + n)

#define _GREYBUS_MANIFEST_STRING_SIZE_UNALIGNED(s)                                                 \
	_GREYBUS_DESCRIPTOR_SIZE(sizeof(struct greybus_descriptor_string) + GREYBUS_STRLEN(s))

#define GREYBUS_MANIFEST_INTERFACE_SIZE                                                            \
	_GREYBUS_DESCRIPTOR_SIZE(sizeof(struct greybus_descriptor_interface))
#define GREYBUS_MANIFEST_STRING_SIZE(s) ROUND_UP(_GREYBUS_MANIFEST_STRING_SIZE_UNALIGNED(s), 4)
#define GREYBUS_MANIFEST_BUNDLE_SIZE                                                               \
	_GREYBUS_DESCRIPTOR_SIZE(sizeof(struct greybus_descriptor_bundle))
#define GREYBUS_MANIFEST_CPORT_SIZE                                                                \
	_GREYBUS_DESCRIPTOR_SIZE(sizeof(struct greybus_descriptor_cport))

#define _GREYBUS_MANIFEST_BUNDLES_SIZE(n) (GREYBUS_MANIFEST_BUNDLE_SIZE * n)
#define _GREYBUS_MANIFEST_CPORTS_SIZE(n)  (GREYBUS_MANIFEST_CPORT_SIZE * n)

#define GREYBUS_BUNDLE_HANDLER(node_id)                                                            \
	IF_ENABLED(DT_NODE_HAS_COMPAT(node_id, zephyr_greybus_bundle),                             \
		   (DT_PROP(node_id, bundle_class), ))

static uint8_t bundles[] = {
	DT_FOREACH_CHILD_STATUS_OKAY(_GREYBUS_BASE_NODE, GREYBUS_BUNDLE_HANDLER)};

#define GREYBUS_MANIFEST_SIZE                                                                      \
	(sizeof(struct greybus_manifest_header) + GREYBUS_MANIFEST_INTERFACE_SIZE +                \
	 GREYBUS_MANIFEST_STRING_SIZE(CONFIG_GREYBUS_VENDOR_STRING) +                              \
	 GREYBUS_MANIFEST_STRING_SIZE(CONFIG_GREYBUS_PRODUCT_STRING) +                             \
	 _GREYBUS_MANIFEST_CPORTS_SIZE(GREYBUS_CPORT_COUNT) +                                      \
	 _GREYBUS_MANIFEST_BUNDLES_SIZE(ARRAY_SIZE(bundles)))

size_t manifest_size(void)
{
	return GREYBUS_MANIFEST_SIZE;
}

static void set_greybus_descriptor_header(struct greybus_descriptor_header *hdr, uint16_t size,
					  uint8_t type)
{
	hdr->type = type;
	hdr->size = sys_cpu_to_le16(size);
}

static void set_greybus_manifest_header(struct greybus_manifest *mnfb)
{
	mnfb->header.size = sys_cpu_to_le16(manifest_size());
	mnfb->header.version_major = CONFIG_GREYBUS_VERSION_MAJOR;
	mnfb->header.version_minor = CONFIG_GREYBUS_VERSION_MINOR;
}

static int set_greybus_interface(struct greybus_descriptor *desc)
{
	set_greybus_descriptor_header(&desc->header, GREYBUS_MANIFEST_INTERFACE_SIZE,
				      GREYBUS_TYPE_INTERFACE);

	desc->interface.product_stringid = GREYBUS_PRODUCT_STRING_ID;
	desc->interface.vendor_stringid = GREYBUS_VENDOR_STRING_ID;

	return GREYBUS_MANIFEST_INTERFACE_SIZE;
}

static int set_greybus_string(struct greybus_descriptor *desc, uint8_t id, uint16_t strlen,
			      uint16_t size, const char *str)
{
	set_greybus_descriptor_header(&desc->header, size, GREYBUS_TYPE_STRING);

	desc->string.length = sys_cpu_to_le16(strlen);
	desc->string.id = id;
	memcpy(desc->string.string, str, strlen);

	return size;
}

static int set_greybus_bundle(struct greybus_descriptor *desc, uint8_t id, uint8_t class)
{
	set_greybus_descriptor_header(&desc->header, GREYBUS_MANIFEST_BUNDLE_SIZE,
				      GREYBUS_TYPE_BUNDLE);

	desc->bundle.id = id;
	desc->bundle.class = class;

	return GREYBUS_MANIFEST_BUNDLE_SIZE;
}

static int set_greybus_cport(struct greybus_descriptor *desc, uint8_t id, uint8_t bundle,
			     uint8_t protocol_id)
{
	set_greybus_descriptor_header(&desc->header, GREYBUS_MANIFEST_CPORT_SIZE,
				      GREYBUS_TYPE_CPORT);

	desc->cport.id = id;
	desc->cport.bundle = bundle;
	desc->cport.protocol_id = protocol_id;

	return GREYBUS_MANIFEST_CPORT_SIZE;
}

int manifest_create(uint8_t buf[], size_t len)
{
	int ret, i;
	struct greybus_manifest *mnfb;
	struct greybus_descriptor *desc;
	struct gb_cport_new *cport;

	if (len < manifest_size()) {
		return -E2BIG;
	}

	memset(buf, 0, len);
	mnfb = (struct greybus_manifest *)buf;

	set_greybus_manifest_header(mnfb);

	desc = &mnfb->descriptors[0];
	ret = set_greybus_interface(desc);

	desc = (struct greybus_descriptor *)((uint8_t *)desc + ret);
	ret = _set_greybus_string(desc, GREYBUS_VENDOR_STRING_ID, CONFIG_GREYBUS_VENDOR_STRING);

	desc = (struct greybus_descriptor *)((uint8_t *)desc + ret);
	ret = _set_greybus_string(desc, GREYBUS_PRODUCT_STRING_ID, CONFIG_GREYBUS_PRODUCT_STRING);

	for (i = 0; i < ARRAY_SIZE(bundles); i++) {
		desc = (struct greybus_descriptor *)((uint8_t *)desc + ret);
		ret = set_greybus_bundle(desc, i, bundles[i]);
	}

	for (i = 0; i < GREYBUS_CPORT_COUNT; ++i) {
		cport = gb_cport_get_new(i);
		desc = (struct greybus_descriptor *)((uint8_t *)desc + ret);

		ret = set_greybus_cport(desc, i, cport->bundle, cport->protocol);
	}

	return manifest_size();
}
