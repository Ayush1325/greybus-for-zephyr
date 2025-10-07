/*
 * Copyright (c) 2015 Google, Inc.
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

#include "greybus_messages.h"
#include "greybus_transport.h"
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <greybus/greybus.h>
#include <greybus/platform.h>
#include <stdlib.h>
#include <string.h>
#include <greybus-utils/utils.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_BOARD_NATIVE_POSIX_64BIT) || defined(CONFIG_BOARD_NATIVE_POSIX_32BIT) ||        \
	defined(CONFIG_BOARD_NRF52_BSIM)
#include <unistd.h>
extern int usleep(useconds_t usec);
#else
#include <zephyr/posix/unistd.h>
#endif

#include "spi-gb.h"

LOG_MODULE_REGISTER(greybus_spi, CONFIG_GREYBUS_LOG_LEVEL);

#define GB_SPI_VERSION_MAJOR 0
#define GB_SPI_VERSION_MINOR 1

/**
 * @brief Returns the major and minor Greybus SPI protocol version number
 *        supported by the SPI master
 *
 * @param operation pointer to structure of Greybus operation message
 * @return GB_OP_SUCCESS on success, error code on failure
 */
static void gb_spi_protocol_version(uint16_t cport, struct gb_message *req)
{
	const struct gb_spi_proto_version_response resp_data = {
		.major = GB_SPI_VERSION_MAJOR,
		.minor = GB_SPI_VERSION_MINOR,
	};

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static int device_spi_get_master_config(const struct device *dev,
					struct gb_spi_master_config_response *response)
{
	int ret;

	const struct device *gb_spidev = gb_spidev_from_zephyr_spidev(dev);
	if (gb_spidev == NULL) {
		return -ENODEV;
	}

	const struct gb_platform_spi_api *api = gb_spidev->api;
	__ASSERT_NO_MSG(api != NULL);

	ret = api->controller_config_response(gb_spidev, response);
	__ASSERT_NO_MSG(ret == 0);

	return 0;
}

/**
 * @brief Returns a set of configuration parameters related to SPI master.
 */
static void gb_spi_protocol_master_config(uint16_t cport, struct gb_message *req,
					  const struct device *dev)
{
	struct gb_spi_master_config_response resp_data;
	int ret = 0;

	ret = device_spi_get_master_config(dev, &resp_data);
	if (ret) {
		ret = gb_errno_to_op_result(ret);
		gb_transport_message_empty_response_send(req, ret, cport);
	}

	/* TODO: use compile-time byte swap operators in platform/spi.c
	 * so byteswapping is eliminated here */
	resp_data.bpw_mask = sys_cpu_to_le32(resp_data.bpw_mask);
	resp_data.min_speed_hz = sys_cpu_to_le32(resp_data.min_speed_hz);
	resp_data.max_speed_hz = sys_cpu_to_le32(resp_data.max_speed_hz);
	resp_data.mode = sys_cpu_to_le16(resp_data.mode);
	resp_data.flags = sys_cpu_to_le16(resp_data.flags);
	resp_data.num_chipselect = sys_cpu_to_le16(resp_data.num_chipselect);

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static int device_spi_get_device_config(const struct device *dev, uint8_t cs,
					struct gb_spi_device_config_response *response)
{
	int ret;

	const struct device *gb_spidev = gb_spidev_from_zephyr_spidev(dev);
	const struct gb_platform_spi_api *api = (struct gb_platform_spi_api *)gb_spidev->api;
	__ASSERT_NO_MSG(api != NULL);

	ret = api->peripheral_config_response(gb_spidev, cs, response);
	__ASSERT_NO_MSG(ret == 0);

	return 0;
}

/**
 * @brief Get configuration parameters from chip
 *
 * Returns a set of configuration parameters taht related to SPI device is
 * selected.
 */
static void gb_spi_protocol_device_config(uint16_t cport, struct gb_message *req,
					  const struct device *dev)
{
	const struct gb_spi_device_config_request *request =
		(const struct gb_spi_device_config_request *)req->payload;
	struct gb_spi_device_config_response resp_data;
	uint8_t cs;
	int ret = 0;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	cs = request->chip_select;

	/* get selected chip of configuration */
	ret = device_spi_get_device_config(dev, cs, &resp_data);
	if (ret) {
		ret = gb_errno_to_op_result(ret);
		gb_transport_message_empty_response_send(req, ret, cport);
	}

	/* TODO: use compile-time byte swap operators in platform/spi.c
	 * so byteswapping is eliminated here */
	resp_data.mode = sys_cpu_to_le16(resp_data.mode);
	resp_data.max_speed_hz = sys_cpu_to_le32(resp_data.max_speed_hz);

	gb_transport_message_response_success_send(req, &resp_data, sizeof(resp_data), cport);
}

static int request_to_spi_config(const struct gb_spi_transfer_request *const request,
				 const size_t freq, const uint8_t bits_per_word,
				 const struct device *spi_dev, struct spi_config *const spi_config,
				 struct spi_cs_control *ctrl)
{
	const struct device *gb_spidev = gb_spidev_from_zephyr_spidev(spi_dev);

	if (gb_spidev == NULL) {
		return -ENODEV;
	}

	const struct gb_platform_spi_api *api = gb_spidev->api;
	__ASSERT_NO_MSG(api != NULL);

	spi_config->frequency = freq;
	spi_config->slave = request->chip_select;
	spi_config->operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(bits_per_word);

	if (request->mode & GB_SPI_MODE_CPHA) {
		spi_config->operation |= SPI_MODE_CPHA;
	}

	if (request->mode & GB_SPI_MODE_CPOL) {
		spi_config->operation |= SPI_MODE_CPOL;
	}

	if (request->mode & GB_SPI_MODE_CS_HIGH) {
		spi_config->operation |= SPI_CS_ACTIVE_HIGH;
	}

	if (request->mode & GB_SPI_MODE_CS_HIGH) {
		spi_config->operation |= SPI_CS_ACTIVE_HIGH;
	}

	if (request->mode & GB_SPI_MODE_LSB_FIRST) {
		spi_config->operation |= SPI_TRANSFER_LSB;
	}

	if (request->mode & GB_SPI_MODE_3WIRE) {
		/* Do nothing. I think this is the presumptive case */
	}

	if (request->mode & GB_SPI_MODE_LOOP) {
		spi_config->operation |= SPI_MODE_LOOP;
	}

	if (request->mode & GB_SPI_MODE_NO_CS) {
		// if (spi_config->cs != NULL) {
		/* LOG_DBG("GB_SPI_MODE_NO_CS flag given but spi_config->cs is "
			"non-NULL (%p)", spi_config->cs); */
		// }
	}

	if (request->mode & GB_SPI_MODE_READY) {
		/* LOG_DBG("GB_SPI_MODE_READY not handled"); */
	}

	if (false || ((request->mode & GB_SPI_FLAG_HALF_DUPLEX) != 0) ||
	    ((request->mode & GB_SPI_FLAG_NO_RX) != 0) ||
	    ((request->mode & GB_SPI_FLAG_NO_TX) != 0)) {
		/* LOG_DBG("GB_SPI_FLAG_{HALF_DUPLEX,NO_{RX,TX}} not handled"); */
	}

	if (!api->get_cs_control(gb_spidev, request->chip_select, ctrl)) {
		// spi_config->cs = ctrl;
		memcpy(&spi_config->cs, ctrl, sizeof(struct spi_cs_control));
	} else {
		// spi_config->cs = NULL;
	}

	return 0;
}

/**
 * @brief Performs a SPI transaction as one or more SPI transfers, defined
 *        in the supplied array.
 */
static void gb_spi_protocol_transfer(uint16_t cport, struct gb_message *req,
				     const struct device *dev)
{
	struct gb_message *resp;
	const struct gb_spi_transfer_desc *desc;
	struct gb_spi_transfer_response *resp_data;
	struct spi_config spi_config;
	struct spi_cs_control spi_cs_control;

	struct spi_buf_set _tx_buf_set;
	struct spi_buf_set *tx_buf_set = &_tx_buf_set;
	struct spi_buf *tx_buf = NULL;

	struct spi_buf_set _rx_buf_set;
	struct spi_buf_set *rx_buf_set = &_rx_buf_set;
	struct spi_buf *rx_buf = NULL;

	uint32_t read_data_size = 0;
	uint8_t *write_data, *read_data;
	int i, op_count;
	uint8_t bits_per_word;
	int ret = 0, errcode = GB_OP_SUCCESS;
	size_t expected_size;
	const struct gb_spi_transfer_request *request =
		(const struct gb_spi_transfer_request *)req->payload;

	if (gb_message_payload_len(req) < sizeof(*request)) {
		LOG_ERR("dropping short message");
		return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
	}

	op_count = sys_le16_to_cpu(request->count);
	if (op_count == 0) {
		return gb_transport_message_empty_response_send(req, GB_OP_SUCCESS, cport);
	}

	write_data = (uint8_t *)&request->transfers[op_count];
	bits_per_word = request->transfers[0].bits_per_word;

	for (i = 0, read_data_size = 0; i < op_count; ++i) {
		desc = &request->transfers[i];
		if (desc->rdwr & GB_SPI_XFER_READ) {
			if (sys_le32_to_cpu(desc->len) == 0) {
				LOG_ERR("read operation of length 0 is invalid");
				return gb_transport_message_empty_response_send(req, GB_OP_INVALID,
										cport);
			}
			read_data_size += sys_le32_to_cpu(desc->len);
		}

		/* ensure 1 bpw setting */
		if (desc->bits_per_word != bits_per_word) {
			LOG_ERR("only 1 bpw setting supported");
			return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
		}

		/* ensure no cs_change */
		if (desc->cs_change) {
			LOG_ERR("cs_change not supported");
			return gb_transport_message_empty_response_send(req, GB_OP_INVALID, cport);
		}
	}

	tx_buf = malloc(op_count * 2 * sizeof(*tx_buf));
	rx_buf = &tx_buf[op_count];
	if (tx_buf == NULL) {
		free(tx_buf);
		LOG_ERR("Failed to allocate buffer descriptors");
		errcode = GB_OP_NO_MEMORY;
		goto out;
	}

	expected_size = sizeof(*request) + op_count * sizeof(request->transfers[0]);
	if (gb_message_payload_len(req) < expected_size) {
		LOG_ERR("dropping short message");
		errcode = GB_OP_INVALID;
		goto freebufs;
	}

	resp = gb_message_alloc(read_data_size, GB_RESPONSE(req->header.type), req->header.id,
				GB_OP_SUCCESS);
	if (!resp) {
		gb_transport_message_empty_response_send(req, GB_OP_NO_MEMORY, cport);
		goto freebufs;
	}
	resp_data = (struct gb_spi_transfer_response *)resp->payload;

	/* parse all transfer request from AP host side */
	tx_buf_set->buffers = tx_buf;
	tx_buf_set->count = 0;

	if (read_data_size > 0) {
		rx_buf_set->buffers = rx_buf;
		rx_buf_set->count = 0;
		read_data = resp_data->data;
	} else {
		read_data = NULL;
		rx_buf_set = NULL;
	}

	for (i = 0; i < op_count; ++i) {
		desc = &request->transfers[i];

		/* setup SPI transfer */
		switch (desc->rdwr) {
		case GB_SPI_XFER_WRITE:
			tx_buf[i].buf = write_data;
			tx_buf[i].len = sys_le32_to_cpu(desc->len);
			write_data += tx_buf[i].len;
			++tx_buf_set->count;
			rx_buf[i].buf = NULL;
			rx_buf[i].len = 0;
			break;
		case GB_SPI_XFER_READ:
			tx_buf[i].buf = NULL;
			tx_buf[i].len = 0;
			rx_buf[i].buf = read_data;
			rx_buf[i].len = sys_le32_to_cpu(desc->len);
			read_data += rx_buf[i].len;
			++rx_buf_set->count;
			break;
		case GB_SPI_XFER_WRITE | GB_SPI_XFER_READ:
			tx_buf[i].buf = write_data;
			tx_buf[i].len = sys_le32_to_cpu(desc->len);
			write_data += tx_buf[i].len;
			++tx_buf_set->count;
			rx_buf[i].buf = read_data;
			rx_buf[i].len = sys_le32_to_cpu(desc->len);
			read_data += rx_buf[i].len;
			++rx_buf_set->count;
			break;
		default:
			LOG_ERR("invalid flags in rdwr %x", desc->rdwr);
			errcode = GB_OP_INVALID;
			goto freemsg;
		}
	}

	/* set SPI configuration */
	ret = request_to_spi_config(request, sys_le32_to_cpu(request->transfers[0].speed_hz),
				    bits_per_word, dev, &spi_config, &spi_cs_control);
	if (ret) {
		errcode = gb_errno_to_op_result(-ret);
		goto freemsg;
	}

	/* start SPI transfer */
	ret = spi_transceive(dev, &spi_config, tx_buf_set, rx_buf_set);
	if (ret) {
		errcode = gb_errno_to_op_result(-ret);
		goto freemsg;
	}

	errcode = GB_OP_SUCCESS;

	gb_transport_message_send(resp, cport);
	gb_message_dealloc(resp);
	return;

freemsg:
	gb_message_dealloc(resp);

freebufs:
	free(tx_buf);

out:
	gb_transport_message_empty_response_send(req, errcode, cport);
}

/**
 * @brief Greybus SPI protocol initialize function
 *
 * @param cport CPort number
 * @param bundle Greybus bundle handle
 * @return 0 on success, negative errno on error
 */
static int gb_spi_init(unsigned int cport, struct gb_bundle *bundle)
{
	unsigned int cport_idx = cport - bundle->cport_start;

	bundle->dev[cport_idx] = (struct device *)gb_cport_to_device(cport);
	if (!bundle->dev[cport_idx]) {
		return -EIO;
	}
	return 0;
}

/**
 * @brief Greybus SPI protocol deinitialize function
 *
 * @param cport CPort number
 * @param bundle Greybus bundle handle
 */
static void gb_spi_exit(unsigned int cport, struct gb_bundle *bundle)
{
}

static void gb_spi_handler(struct gb_driver *drv, struct gb_message *msg, uint16_t cport)
{
	const struct device *dev = gb_cport_to_device(cport);

	switch (gb_message_type(msg)) {
	case GB_SPI_PROTOCOL_VERSION:
		return gb_spi_protocol_version(cport, msg);
	case GB_SPI_TYPE_MASTER_CONFIG:
		return gb_spi_protocol_master_config(cport, msg, dev);
	case GB_SPI_TYPE_DEVICE_CONFIG:
		return gb_spi_protocol_device_config(cport, msg, dev);
	case GB_SPI_PROTOCOL_TRANSFER:
		return gb_spi_protocol_transfer(cport, msg, dev);
	default:
		LOG_ERR("Invalid type");
		gb_transport_message_empty_response_send(msg, GB_OP_INVALID, cport);
	}
}

static struct gb_driver gb_spi_driver = {
	.init = gb_spi_init,
	.exit = gb_spi_exit,
	.op_handler = gb_spi_handler,
};

/**
 * @brief Register Greybus SPI protocol
 *
 * @param cport CPort number
 * @param bundle Bundle number.
 */
void gb_spi_register(int cport, int bundle)
{
	gb_register_driver(cport, bundle, &gb_spi_driver);
}
