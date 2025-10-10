/*
 * Copyright (c) 2020 Friedt Professional Engineering Services, Inc
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEPHYR_INCLUDE_GREYBUS_PLATFORM_H_
#define ZEPHYR_INCLUDE_GREYBUS_PLATFORM_H_

#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gb_spi_master_config_response;
struct gb_spi_device_config_response;
struct spi_cs_control;
struct gb_platform_spi_api {
	int (*controller_config_response)(const struct device *dev,
					  struct gb_spi_master_config_response *rsp);
	int (*num_peripherals)(const struct device *dev);
	int (*peripheral_config_response)(const struct device *dev, uint8_t chip_select,
					  struct gb_spi_device_config_response *rsp);
	int (*get_cs_control)(const struct device *dev, uint8_t chip_select,
			      struct spi_cs_control *ctrl);
};

/*
 * Get the greybus spi device associated with a physical spi device
 *
 * usage:
 *
 * const struct device *gb_spidev = gb_spidev_from_zephyr_spidev(device_get_binding("SPI_0"));
 * const struct gb_platform_spi_api *api = (struct gb_platform_spi_api *)gb_spidev->driver_api;
 */
const struct device *gb_spidev_from_zephyr_spidev(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_GREYBUS_PLATFORM_H_ */
