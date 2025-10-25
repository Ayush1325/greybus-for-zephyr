/*
 * Copyright (c) 2025 Ayush Singh, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "greybus/greybus_messages.h"
#include <zephyr/ztest.h>
#include <greybus/greybus.h>
#include <greybus-utils/manifest.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/spi_emul.h>

#define OP_COUNT     3
#define TRANSFER_BUF 128

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(spi0));

struct gb_msg_with_cport gb_transport_get_message(void);

static int spi_emul_io(const struct emul *target, const struct spi_config *config,
		       const struct spi_buf_set *tx_bufs, const struct spi_buf_set *rx_bufs)
{
	uint8_t *buf;
	size_t i, j;

	if (tx_bufs) {
		for (i = 0; i < tx_bufs->count; i++) {
			for (j = 0; j < tx_bufs->buffers[i].len; j++) {
				buf = tx_bufs->buffers[i].buf;
				zassert_equal(buf[j], j, "Invalid data");
			}
		}
	}

	if (rx_bufs) {
		for (i = 0; i < rx_bufs->count; i++) {
			for (j = 0; j < rx_bufs->buffers[i].len; j++) {
				buf = rx_bufs->buffers[i].buf;
				buf[j] = j;
			}
		}
	}

	return 0;
}

static const struct spi_emul_api spi_emul_api = {
	.io = spi_emul_io,
};

static const struct device spi_dev = {
	.name = "test-dev",
};

static const struct emul spi_emul_dev = {
	.dev = &spi_dev,
};

static struct spi_emul spi_emul = {
	.target = &spi_emul_dev,
	.api = &spi_emul_api,
	.chipsel = 0,
};

ZTEST_SUITE(greybus_spi_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(greybus_spi_tests, test_cport_count)
{
	zassert_equal(GREYBUS_CPORT_COUNT, 2, "Invalid number of cports");
}

ZTEST(greybus_spi_tests, test_transfer)
{
	int i, ret;
	uint8_t *write_data;
	struct gb_msg_with_cport resp;
	struct gb_spi_transfer_request *req_data;
	struct gb_message *req = gb_message_request_alloc(
		sizeof(*req_data) + sizeof(struct gb_spi_transfer) * OP_COUNT +
			TRANSFER_BUF * (OP_COUNT - 1),
		GB_SPI_TYPE_TRANSFER, false);

	ret = spi_emul_register(dev, &spi_emul);
	zassert_equal(ret, 0, "Failed to register spi device");

	memset(req->payload, 0, gb_message_payload_len(req));

	req_data = (struct gb_spi_transfer_request *)req->payload;
	req_data->count = sys_cpu_to_le16(OP_COUNT);
	req_data->mode = 0;
	req_data->chip_select = 0;

	write_data = (uint8_t *)&req_data->transfers[OP_COUNT];

	/* Op 1 */
	req_data->transfers[0].speed_hz = 10000;
	req_data->transfers[0].len = sys_cpu_to_le32(TRANSFER_BUF);
	req_data->transfers[0].xfer_flags = GB_SPI_XFER_WRITE;
	for (i = 0; i < TRANSFER_BUF; i++) {
		write_data[i] = i;
	}

	/* Op 2 */
	req_data->transfers[1].speed_hz = 10000;
	req_data->transfers[1].len = sys_cpu_to_le32(TRANSFER_BUF);
	req_data->transfers[1].xfer_flags = GB_SPI_XFER_READ;

	/* Op 3 Read + Write */
	req_data->transfers[2].speed_hz = 10000;
	req_data->transfers[2].len = sys_cpu_to_le32(TRANSFER_BUF);
	req_data->transfers[2].xfer_flags = (GB_SPI_XFER_READ | GB_SPI_XFER_WRITE);
	for (i = 0; i < TRANSFER_BUF; i++) {
		write_data[TRANSFER_BUF + i] = i;
	}

	greybus_rx_handler(1, req);
	resp = gb_transport_get_message();

	zassert_equal(resp.cport, 1, "Invalid cport");
	zassert_equal(gb_message_type(resp.msg), GB_RESPONSE(GB_SPI_TYPE_TRANSFER),
		      "Invalid response type");
	zassert(gb_message_is_success(resp.msg), "Request failed");
	zassert_equal(gb_message_payload_len(resp.msg), TRANSFER_BUF * (OP_COUNT - 1),
		      "Invalid response size");

	for (i = 0; i < TRANSFER_BUF; i++) {
		zassert_equal(resp.msg->payload[i], i, "Unexpected data");
		zassert_equal(resp.msg->payload[TRANSFER_BUF + i], i, "Unexpected data");
	}

	gb_message_dealloc(resp.msg);
}
