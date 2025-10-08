/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_cport.h"
#include "greybus-utils/manifest.h"
#include "greybus-manifest.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(greybus_cport, CONFIG_GREYBUS_LOG_LEVEL);

#define GREYBUS_MANFIEST_CPORT(_bundle, _protocol)                                                 \
	{                                                                                          \
		.bundle = _bundle,                                                                 \
		.protocol = _protocol,                                                             \
	},

#define GREYBUS_CREATE_CPORT_FROM_PROP(_node_id, _bundle)                                          \
	GREYBUS_MANFIEST_CPORT(_bundle, DT_PROP(_node_id, cport_protocol))

#define GREYBUS_CPORT_IN_BUNDLE_HANDLER(_node_id)                                                  \
	DT_FOREACH_CHILD_STATUS_OKAY_VARGS(_node_id, GREYBUS_CREATE_CPORT_FROM_PROP,               \
					   DT_PROP(_node_id, id))

#define GREYBUS_CPORT_HANDLER(_node_id)                                                            \
	IF_ENABLED(DT_NODE_HAS_COMPAT(_node_id, zephyr_greybus_bundle),                            \
		   (GREYBUS_CPORT_IN_BUNDLE_HANDLER(_node_id)))

static struct gb_cport_new cports[GREYBUS_CPORT_COUNT] = {
	DT_FOREACH_CHILD_STATUS_OKAY(_GREYBUS_BASE_NODE, GREYBUS_CPORT_HANDLER)};

struct gb_cport_new *gb_cport_get_new(uint16_t cport)
{
	return (cport >= GREYBUS_CPORT_COUNT) ? NULL : &cports[cport];
}

extern void gb_control_register(int cport, int bundle);
extern void gb_gpio_register(int cport, int bundle);
extern void gb_i2c_register(int cport, int bundle);
extern void gb_power_supply_register(int cport, int bundle);
extern void gb_loopback_register(int cport, int bundle);
extern void gb_vibrator_register(int cport, int bundle);
extern void gb_usb_register(int cport, int bundle);
extern void gb_pwm_register(int cport, int bundle);
extern void gb_spi_register(int cport, int bundle);
extern void gb_uart_register(int cport, int bundle);
extern void gb_hid_register(int cport, int bundle);
extern void gb_lights_register(int cport, int bundle);
extern void gb_sdio_register(int cport, int bundle);
extern void gb_camera_register(int cport, int bundle);
extern void gb_audio_mgmt_register(int cport, int bundle);
extern void gb_audio_data_register(int cport, int bundle);

void enable_cports(void)
{
	__attribute__((unused)) uint16_t cport_id;
	__attribute__((unused)) uint8_t bundle_id;
	__attribute__((unused)) uint8_t protocol;

	for (cport_id = 0; cport_id < GREYBUS_CPORT_COUNT; ++cport_id) {
		bundle_id = cports[cport_id].bundle;
		protocol = cports[cport_id].protocol;

#ifdef CONFIG_GREYBUS_CONTROL
		if (protocol == GREYBUS_PROTOCOL_CONTROL) {
			LOG_INF("Registering CONTROL greybus driver.");
			gb_control_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_GPIO
		if (protocol == GREYBUS_PROTOCOL_GPIO) {
			LOG_INF("Registering GPIO greybus driver.");
			gb_gpio_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_I2C
		if (protocol == GREYBUS_PROTOCOL_I2C) {
			LOG_INF("Registering I2C greybus driver.");
			gb_i2c_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_POWER_SUPPLY
		if (protocol == GREYBUS_PROTOCOL_POWER_SUPPLY) {
			LOG_INF("Registering POWER_SUPPLY greybus driver.");
			gb_power_supply_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_LOOPBACK
		if (protocol == GREYBUS_PROTOCOL_LOOPBACK) {
			LOG_INF("Registering Loopback greybus driver.");
			gb_loopback_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_VIBRATOR
		if (protocol == GREYBUS_PROTOCOL_VIBRATOR) {
			LOG_INF("Registering VIBRATOR greybus driver.");
			gb_vibrator_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_USB_HOST
		if (protocol == GREYBUS_PROTOCOL_USB) {
			LOG_INF("Registering USB greybus driver.");
			gb_usb_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_PWM
		if (protocol == GREYBUS_PROTOCOL_PWM) {
			LOG_INF("Registering PWM greybus driver.");
			gb_pwm_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_SPI
		if (protocol == GREYBUS_PROTOCOL_SPI) {
			LOG_INF("Registering SPI greybus driver.");
			gb_spi_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_UART
		if (protocol == GREYBUS_PROTOCOL_UART) {
			LOG_INF("Registering Uart greybus driver. id= %d", cport_id);
			gb_uart_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_HID
		if (protocol == GREYBUS_PROTOCOL_HID) {
			LOG_INF("Registering HID greybus driver. id= %d", cport_id);
			gb_hid_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_LIGHTS
		if (protocol == GREYBUS_PROTOCOL_LIGHTS) {
			LOG_INF("Registering Lights greybus driver. id= %d", cport_id);
			gb_lights_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_SDIO
		if (protocol == GREYBUS_PROTOCOL_SDIO) {
			LOG_INF("Registering SDIO greybus driver.");
			gb_sdio_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_CAMERA
		if (protocol == GREYBUS_PROTOCOL_CAMERA_MGMT) {
			LOG_INF("Registering Camera greybus driver. id= %d", cport_id);
			gb_camera_register(cport_id, bundle_id);
		}
#endif

#ifdef CONFIG_GREYBUS_AUDIO
		if (protocol == GREYBUS_PROTOCOL_AUDIO_MGMT) {
			LOG_INF("Registering Audio MGMT greybus driver.");
			gb_audio_mgmt_register(cport_id, bundle_id);
		}

		if (protocol == GREYBUS_PROTOCOL_AUDIO_DATA) {
			LOG_INF("Registering Audio DATA greybus driver.");
			gb_audio_data_register(cport_id, bundle_id);
		}
#endif
	}
}
