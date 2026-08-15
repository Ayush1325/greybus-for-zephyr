// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) 2016-2019 Intel Corporation
 * Copyright (c) 2020 Statropy Software LLC
 *
 * Modifications Copyright (c) 2023 Ayush Singh <ayushdevel1325@gmail.com>
 */

#include "hdlc.h"
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/ring_buffer.h>
#include <greybus/greybus_protocols.h>
#include <assert.h>

#define HDLC_FRAME     0x7E
#define HDLC_ESC       0x7D
#define HDLC_ESC_FRAME 0x5E
#define HDLC_ESC_ESC   0x5D
#define HDLC_UFRAME    0x03

#define HDLC_RX_WORKQUEUE_STACK_SIZE 2048
#define HDLC_RX_WORKQUEUE_PRIORITY   5

static void hdlc_rx_handler(struct k_work *);

LOG_MODULE_REGISTER(greybus_hdlc, CONFIG_GREYBUS_LOG_LEVEL);

K_WORK_DEFINE(hdlc_rx_work, hdlc_rx_handler);
RING_BUF_DECLARE(hdlc_rx_ringbuf, HDLC_MAX_BLOCK_SIZE);

struct hdlc_driver {
	hdlc_process_frame_callback process_callback_frame_cb;
	hdlc_send_frame_callback send_frame_cb;

	uint16_t crc;
	uint16_t rx_buffer_len;
	uint8_t rx_buffer[HDLC_MAX_BLOCK_SIZE];
	bool next_escaped;
};

static struct hdlc_driver hdlc_driver;

static void uart_poll_out_crc(uint8_t byte, uint16_t *crc)
{
	uint8_t temp;

	*crc = crc16_ccitt(*crc, &byte, 1);
	if (byte == HDLC_FRAME || byte == HDLC_ESC) {
		temp = HDLC_ESC;
		hdlc_driver.send_frame_cb(&temp, 1);
		byte ^= 0x20;
	}
	hdlc_driver.send_frame_cb(&byte, 1);
}

static void hdlc_process_complete_frame(struct hdlc_driver *drv)
{
	int ret;
	uint8_t address = drv->rx_buffer[0];
	size_t len = drv->rx_buffer_len - 4;
	void *buffer = &drv->rx_buffer[2];

	ret = drv->process_callback_frame_cb(buffer, len, address);
	if (ret < 0) {
		LOG_ERR("Dropped HDLC addr:%x ctrl:%x", address, drv->rx_buffer[1]);
		LOG_HEXDUMP_DBG(drv->rx_buffer, drv->rx_buffer_len, "rx_buffer");
	}
}

static void hdlc_process_frame(struct hdlc_driver *drv)
{
	if (drv->rx_buffer_len > 3 && drv->crc == 0xf0b8) {
		uint8_t ctrl = drv->rx_buffer[1];
                assert(ctrl == HDLC_UFRAME);
		hdlc_process_complete_frame(drv);
	} else {
		LOG_ERR("Dropped HDLC crc:%04x len:%d", drv->crc, drv->rx_buffer_len);
	}

	drv->crc = 0xffff;
	drv->rx_buffer_len = 0;
}

static int hdlc_save_byte(struct hdlc_driver *drv, uint8_t byte)
{
	if (drv->rx_buffer_len >= HDLC_MAX_BLOCK_SIZE) {
		LOG_ERR("HDLC RX Buffer Overflow");
		drv->crc = 0xffff;
		drv->rx_buffer_len = 0;
	}

	drv->rx_buffer[drv->rx_buffer_len++] = byte;

	return 0;
}

static void hdlc_rx_input_byte(struct hdlc_driver *drv, uint8_t byte)
{
	switch (byte) {
	case HDLC_FRAME:
		if (drv->rx_buffer_len) {
			hdlc_process_frame(drv);
		}
		break;
	case HDLC_ESC:
		drv->next_escaped = true;
		break;
	default:
		if (drv->next_escaped) {
			byte ^= 0x20;
			drv->next_escaped = false;
		}
		drv->crc = crc16_ccitt(drv->crc, &byte, 1);
		hdlc_save_byte(drv, byte);
	}
}

static int hdlc_process_buffer(uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		hdlc_rx_input_byte(&hdlc_driver, buf[i]);
	}
	return len;
}

static void hdlc_rx_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	uint8_t *data;
	int ret;

	ret = ring_buf_get_claim(&hdlc_rx_ringbuf, &data, HDLC_MAX_BLOCK_SIZE);
	ret = hdlc_process_buffer(data, ret);
	if (ret < 0) {
		LOG_ERR("Error processing HDLC buffer");
	}

	ret = ring_buf_get_finish(&hdlc_rx_ringbuf, ret);
	if (ret < 0) {
		LOG_ERR("Cannot flush ring buffer (%d)", ret);
	}
}

int hdlc_block_send_sync(const uint8_t *buffer, size_t buffer_len, uint8_t address)
{
	uint8_t temp = HDLC_FRAME;
	uint16_t crc = 0xffff;

	hdlc_driver.send_frame_cb(&temp, 1);
	uart_poll_out_crc(address, &crc);
	uart_poll_out_crc(HDLC_UFRAME, &crc);

	for (int i = 0; i < buffer_len; i++) {
		uart_poll_out_crc(buffer[i], &crc);
	}

	uint16_t crc_calc = crc ^ 0xffff;

	uart_poll_out_crc(crc_calc, &crc);
	uart_poll_out_crc(crc_calc >> 8, &crc);
	hdlc_driver.send_frame_cb(&temp, 1);

	return 0;
}

int hdlc_init(hdlc_process_frame_callback process_cb, hdlc_send_frame_callback send_cb)
{
	hdlc_driver.crc = 0xffff;
	hdlc_driver.next_escaped = false;
	hdlc_driver.rx_buffer_len = 0;

	hdlc_driver.process_callback_frame_cb = process_cb;
	hdlc_driver.send_frame_cb = send_cb;

	return 0;
}

uint32_t hdlc_rx_start(uint8_t **buf)
{
	return ring_buf_put_claim(&hdlc_rx_ringbuf, buf, HDLC_MAX_BLOCK_SIZE);
}

int hdlc_rx_finish(uint32_t written)
{
	int ret;

	ret = ring_buf_put_finish(&hdlc_rx_ringbuf, written);
	k_work_submit(&hdlc_rx_work);

	return ret;
}
