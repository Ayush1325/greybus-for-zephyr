/*
 * Copyright (c) 2025 Ayush Singh BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus_cport.h"
#include "greybus-utils/manifest.h"
#include "greybus-manifest.h"
#include <zephyr/logging/log.h>
#include "greybus_gpio.h"

LOG_MODULE_REGISTER(greybus_cport, CONFIG_GREYBUS_LOG_LEVEL);

/* Reset the counter to 0 */
enum {
	COUNTER_BASE = __COUNTER__
};
#define LOCAL_COUNTER (__COUNTER__ - COUNTER_BASE - 1)

#define GB_GPIO_PRIV_DATA(_node_id, _prop, _idx)                                                   \
	static struct gb_gpio_driver_data gb_gpio_priv_data_##_idx = {                             \
		.dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx)),                    \
	};

#define GB_GPIO_PRIV_DATA_HANDLER(_node_id)                                                        \
	DT_FOREACH_PROP_ELEM(_node_id, gpio_controllers, GB_GPIO_PRIV_DATA)

#define GB_PRIV_DATA_HANDLER(_node_id)                                                             \
	IF_ENABLED(DT_NODE_HAS_COMPAT(_node_id, zephyr_greybus_bundle_bridged_phy),                \
		   (GB_GPIO_PRIV_DATA_HANDLER(_node_id)))

/* Define GPIO private data */
DT_FOREACH_CHILD_STATUS_OKAY(_GREYBUS_BASE_NODE, GB_PRIV_DATA_HANDLER)

#define GB_CPORT_DEV_PRIV_DATA(_node_id, _prop, _idx)                                              \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(_node_id, _prop, _idx))

#define GB_CPORT_GPIO_PRIV_DATA(_node_id, _prop, _idx) &gb_gpio_priv_data_##_idx

#define GB_CPORT(_priv, _bundle, _protocol)                                                        \
	{                                                                                          \
		.bundle = _bundle,                                                                 \
		.protocol = _protocol,                                                             \
		.priv = _priv,                                                                     \
	}

#define _GB_CPORT(_node_id, _prop, _idx, _bundle, _protocol, PRIV_FN)                              \
	GB_CPORT(PRIV_FN(_node_id, _prop, _idx), _bundle, _protocol)

#define GREYBUS_CPORTS_IN_BRIDGED_PHY_BUNDLE(_node_id, _bundle)                                    \
	FOR_EACH_NONEMPTY_TERM(                                                                    \
		IDENTITY, (, ),                                                                    \
		IF_ENABLED(CONFIG_GREYBUS_GPIO,                                                    \
			   (DT_FOREACH_PROP_ELEM_SEP_VARGS(_node_id, gpio_controllers, _GB_CPORT,  \
							   (, ), _bundle, GREYBUS_PROTOCOL_GPIO,   \
							   GB_CPORT_GPIO_PRIV_DATA))),             \
		IF_ENABLED(CONFIG_GREYBUS_I2C,                                                     \
			   (DT_FOREACH_PROP_ELEM_SEP_VARGS(_node_id, i2c_controllers, _GB_CPORT,   \
							   (, ), _bundle, GREYBUS_PROTOCOL_I2C,    \
							   GB_CPORT_DEV_PRIV_DATA))))

#define GB_CPORTS_IN_BUNDLE(node_id)                                                               \
	COND_CODE_1(DT_NODE_HAS_COMPAT(node_id, zephyr_greybus_bundle_bridged_phy),                \
		    (GREYBUS_CPORTS_IN_BRIDGED_PHY_BUNDLE(node_id, LOCAL_COUNTER)), (EMPTY))

static struct gb_cport cports[] = {
	/* cport0 is always control cport */
	GB_CPORT(NULL, LOCAL_COUNTER, GREYBUS_PROTOCOL_CONTROL),
	DT_FOREACH_CHILD_STATUS_OKAY(_GREYBUS_BASE_NODE, GB_CPORTS_IN_BUNDLE)};

BUILD_ASSERT(GREYBUS_CPORT_COUNT == ARRAY_SIZE(cports));

struct gb_cport *gb_cport_get(uint16_t cport)
{
	return (cport >= GREYBUS_CPORT_COUNT) ? NULL : &cports[cport];
}

extern void gb_control_register(int cport);
extern void gb_gpio_register(int cport);
extern void gb_i2c_register(int cport);
extern void gb_power_supply_register(int cport);
extern void gb_loopback_register(int cport);
extern void gb_vibrator_register(int cport);
extern void gb_usb_register(int cport);
extern void gb_pwm_register(int cport);
extern void gb_spi_register(int cport);
extern void gb_uart_register(int cport);
extern void gb_hid_register(int cport);
extern void gb_lights_register(int cport);
extern void gb_sdio_register(int cport);
extern void gb_camera_register(int cport);
extern void gb_audio_mgmt_register(int cport);
extern void gb_audio_data_register(int cport);

void enable_cports(void)
{
	__attribute__((unused)) uint16_t cport_id;
	__attribute__((unused)) uint8_t protocol;

	for (cport_id = 0; cport_id < GREYBUS_CPORT_COUNT; ++cport_id) {
		protocol = cports[cport_id].protocol;

		if (protocol == GREYBUS_PROTOCOL_CONTROL) {
			LOG_INF("Registering CONTROL greybus driver.");
			gb_control_register(cport_id);
		}

#ifdef CONFIG_GREYBUS_GPIO
		if (protocol == GREYBUS_PROTOCOL_GPIO) {
			LOG_INF("Registering GPIO greybus driver.");
			gb_gpio_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_I2C
		if (protocol == GREYBUS_PROTOCOL_I2C) {
			LOG_INF("Registering I2C greybus driver.");
			gb_i2c_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_POWER_SUPPLY
		if (protocol == GREYBUS_PROTOCOL_POWER_SUPPLY) {
			LOG_INF("Registering POWER_SUPPLY greybus driver.");
			gb_power_supply_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_LOOPBACK
		if (protocol == GREYBUS_PROTOCOL_LOOPBACK) {
			LOG_INF("Registering Loopback greybus driver.");
			gb_loopback_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_VIBRATOR
		if (protocol == GREYBUS_PROTOCOL_VIBRATOR) {
			LOG_INF("Registering VIBRATOR greybus driver.");
			gb_vibrator_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_USB_HOST
		if (protocol == GREYBUS_PROTOCOL_USB) {
			LOG_INF("Registering USB greybus driver.");
			gb_usb_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_PWM
		if (protocol == GREYBUS_PROTOCOL_PWM) {
			LOG_INF("Registering PWM greybus driver.");
			gb_pwm_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_SPI
		if (protocol == GREYBUS_PROTOCOL_SPI) {
			LOG_INF("Registering SPI greybus driver.");
			// gb_spi_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_UART
		if (protocol == GREYBUS_PROTOCOL_UART) {
			LOG_INF("Registering Uart greybus driver. id= %d", cport_id);
			gb_uart_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_HID
		if (protocol == GREYBUS_PROTOCOL_HID) {
			LOG_INF("Registering HID greybus driver. id= %d", cport_id);
			gb_hid_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_LIGHTS
		if (protocol == GREYBUS_PROTOCOL_LIGHTS) {
			LOG_INF("Registering Lights greybus driver. id= %d", cport_id);
			gb_lights_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_SDIO
		if (protocol == GREYBUS_PROTOCOL_SDIO) {
			LOG_INF("Registering SDIO greybus driver.");
			gb_sdio_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_CAMERA
		if (protocol == GREYBUS_PROTOCOL_CAMERA_MGMT) {
			LOG_INF("Registering Camera greybus driver. id= %d", cport_id);
			gb_camera_register(cport_id);
		}
#endif

#ifdef CONFIG_GREYBUS_AUDIO
		if (protocol == GREYBUS_PROTOCOL_AUDIO_MGMT) {
			LOG_INF("Registering Audio MGMT greybus driver.");
			gb_audio_mgmt_register(cport_id);
		}

		if (protocol == GREYBUS_PROTOCOL_AUDIO_DATA) {
			LOG_INF("Registering Audio DATA greybus driver.");
			gb_audio_data_register(cport_id);
		}
#endif
	}
}
