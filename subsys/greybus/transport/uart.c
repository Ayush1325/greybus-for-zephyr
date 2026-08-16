#include "hdlc/hdlc.h"
#include "zephyr/sys/byteorder.h"
#include <stdint.h>
#include <greybus/greybus.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#define ADDRESS_GREYBUS 0x01

LOG_MODULE_REGISTER(greybus_transport_uart, CONFIG_GREYBUS_LOG_LEVEL);

static const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(greybus_transport_uart));

/**
 * struct hdlc_greybus_frame - Structure to represent greybus HDLC frame
 *
 * @cport: cport id
 * @hdr: greybus operation header
 * @payload: greybus message payload
 */
struct hdlc_greybus_frame {
	uint16_t cport;
	struct gb_operation_msg_hdr hdr;
	uint8_t payload[];
} __packed;

static int gb_message_hdlc_send(const struct gb_message *msg, uint16_t cport)
{
	char buffer[HDLC_MAX_BLOCK_SIZE];

	memcpy(buffer, &sys_cpu_to_le16(cport), sizeof(cport));
	memcpy(&buffer[sizeof(cport)], &msg->header, sizeof(struct gb_operation_msg_hdr));
	memcpy(&buffer[sizeof(struct gb_operation_msg_hdr) + sizeof(cport)], msg->payload,
	       gb_message_payload_len(msg));

	return hdlc_block_send_sync(buffer, msg->header.size + sizeof(cport), ADDRESS_GREYBUS);
}

static int hdlc_process_frame_cb(const void *buffer, size_t buffer_len, uint8_t address)
{
	struct gb_msg_with_cport msg;
	int ret;
	const struct hdlc_greybus_frame *gb_frame = (const struct hdlc_greybus_frame *)buffer;
	size_t msg_len = buffer_len - sizeof(uint16_t);

	if (gb_hdr_message_len(&gb_frame->hdr) > msg_len) {
		LOG_ERR("Greybus Message size is greater than received buffer.");
		return -1;
	}

	msg.cport = sys_le16_to_cpu(gb_frame->cport);
	msg.msg = gb_message_alloc(gb_hdr_payload_len(&gb_frame->hdr), gb_frame->hdr.type,
				    gb_frame->hdr.operation_id, gb_frame->hdr.result);
	if (!msg.msg) {
		LOG_ERR("Failed to allocate greybus message");
		return -1;
	}

	memcpy(msg.msg->payload, gb_frame->payload, gb_message_payload_len(msg.msg));
        ret = greybus_rx_handler(msg.cport, msg.msg);
	if (ret < 0) {
		LOG_ERR("Failed to process greybus message");
		return ret;
	}

	return 0;
}

static int hdlc_send_frame_cb(const uint8_t *buffer, size_t buffer_len)
{
	size_t i;

	for (i = 0; i < buffer_len; ++i) {
		uart_poll_out(uart_dev, buffer[i]);
	}

	return i;
}

static void serial_callback(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	uint8_t *buf;
	int ret;

	uart_irq_update(dev);

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	ret = hdlc_rx_start(&buf);
	if (ret == 0) {
		/* No space */
		LOG_ERR("No more space for HDLC receive");
		return;
	}

	ret = uart_fifo_read(dev, buf, ret);
	if (ret < 0) {
		/* Something went wrong */
		LOG_ERR("Failed to read UART");
		return;
	}

	ret = hdlc_rx_finish(ret);
	if (ret < 0) {
		/* Some error */
		LOG_ERR("Filed to write data to hdlc buffer");
		return;
	}
}

static int init()
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not found!");
		return -ENODEV;
	}

	ret = hdlc_init(hdlc_process_frame_cb, hdlc_send_frame_cb);
	if (ret < 0) {
		return ret;
	}

	ret = uart_irq_callback_user_data_set(uart_dev, serial_callback, NULL);
	if (ret < 0) {
		if (ret == -ENOTSUP) {
			LOG_ERR("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			LOG_ERR("UART device does not support interrupt-driven API\n");
		} else {
			LOG_ERR("Error setting UART callback: %d\n", ret);
		}
		return ret;
	}

	uart_irq_rx_enable(uart_dev);

	return 0;
}

static int listen(uint16_t cport)
{
	return 0;
}

static int trans_send(uint16_t cport, const struct gb_message *msg)
{
	return gb_message_hdlc_send(msg, cport);
}

const struct gb_transport_backend gb_trans_backend = {
	.init = init,
	.listen = listen,
	.send = trans_send,
};
