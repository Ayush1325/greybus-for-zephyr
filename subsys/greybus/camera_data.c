/*
 * Copyright (c) 2026 Pavithra CP, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "camera_data.h"
#include "greybus_transport.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/video.h>

#ifndef GB_CAMERA_TYPE_DATA
#define GB_CAMERA_TYPE_DATA 0x00
#endif

LOG_MODULE_DECLARE(greybus_camera_test, CONFIG_GREYBUS_LOG_LEVEL);

static void gb_camera_frame_worker(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct gb_camera_data_ctx *ctx = CONTAINER_OF(dwork, struct gb_camera_data_ctx, frame_work);
	struct video_buffer *vbuf;
	int ret;

	if (atomic_get(&ctx->stream_state) != GB_DATA_STATE_STREAMING) {
		return;
	}

	k_mutex_lock(&ctx->data_lock, K_FOREVER);

	ret = video_dequeue(ctx->video_dev, &vbuf, K_NO_WAIT);
	if (ret || vbuf == NULL) {
		if (atomic_get(&ctx->stream_state) == GB_DATA_STATE_STREAMING) {
			k_work_schedule(&ctx->frame_work, K_MSEC(10));
		}
		k_mutex_unlock(&ctx->data_lock);
		return;
	}

	uint32_t frame_size = vbuf->bytesused;
	uint32_t offset = 0;

	ctx->frag_state.frame_id++;

	while (offset < frame_size) {

		uint32_t chunk_size = frame_size - offset;
		if (chunk_size > GB_CAMERA_DATA_MTU) {
			chunk_size = GB_CAMERA_DATA_MTU;
		}

		struct gb_message *msg =
			gb_message_request_alloc(chunk_size, GB_CAMERA_TYPE_DATA, false);

		if (!msg) {
			LOG_ERR("Failed to allocate memory for frame fragment!");
			break; /* Break the loop, but DO NOT skip recycling */
		}

		memcpy(msg->payload, vbuf->buffer + offset, chunk_size);

		ret = gb_transport_message_send(msg, ctx->data_cport);
		if (ret < 0) {
			LOG_ERR("Failed to transmit fragment at offset %u", offset);
			gb_message_dealloc(msg);
			break; /* Break the loop, but DO NOT skip recycling */
		}

		offset += chunk_size;
	}

	ret = video_enqueue(ctx->video_dev, vbuf);
	if (ret) {
		LOG_ERR("Failed to recycle video buffer!");
		video_buffer_release(vbuf);
	}

	if (atomic_get(&ctx->stream_state) == GB_DATA_STATE_STREAMING) {
		k_work_schedule(&ctx->frame_work, K_MSEC(10));
	}

	k_mutex_unlock(&ctx->data_lock);
}

int gb_camera_data_init(struct gb_camera_data_ctx *ctx, const struct device *video_device,
			uint16_t cport)
{
	if (!ctx || !video_device) {
		return -EINVAL;
	}

	ctx->video_dev = video_device;
	ctx->data_cport = cport;
	atomic_set(&ctx->stream_state, GB_DATA_STATE_IDLE);

	k_mutex_init(&ctx->data_lock);
	k_work_init_delayable(&ctx->frame_work, gb_camera_frame_worker);

	ctx->frag_state.frame_id = 0;
	ctx->frag_state.sequence_num = 0;

	return 0;
}

int gb_camera_data_start_stream(struct gb_camera_data_ctx *ctx)
{
	if (atomic_get(&ctx->stream_state) == GB_DATA_STATE_STREAMING) {
		return -EALREADY;
	}

	for (int i = 0; i < GB_CAMERA_NUM_BUFFERS; i++) {

		struct video_buffer *vbuf = video_buffer_alloc(GB_CAMERA_BUFFER_SIZE, K_NO_WAIT);

		if (!vbuf) {
			LOG_ERR("Failed to allocate buffer %d", i);
			return -ENOMEM;
		}

		vbuf->type = VIDEO_BUF_TYPE_OUTPUT;

		int ret = video_enqueue(ctx->video_dev, vbuf);

		if (ret) {
			LOG_ERR("Failed to enqueue buffer %d, err=%d", i, ret);
			video_buffer_release(vbuf);
			return ret;
		}
	}
	atomic_set(&ctx->stream_state, GB_DATA_STATE_STREAMING);

	k_work_schedule(&ctx->frame_work, K_NO_WAIT);

	return 0;
}

int gb_camera_data_stop_stream(struct gb_camera_data_ctx *ctx)
{
	if (atomic_get(&ctx->stream_state) == GB_DATA_STATE_IDLE) {
		return -EALREADY;
	}

	atomic_set(&ctx->stream_state, GB_DATA_STATE_STOPPING);

	struct k_work_sync sync;
	k_work_cancel_delayable_sync(&ctx->frame_work, &sync);
	atomic_set(&ctx->stream_state, GB_DATA_STATE_IDLE);
	return 0;
}
