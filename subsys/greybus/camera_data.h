/*
 * Copyright (c) 2026 Pavithra CP, BeagleBoard.org
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GREYBUS_CAMERA_DATA_H_
#define GREYBUS_CAMERA_DATA_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#define GB_CAMERA_DATA_MTU          1024  
#define GB_CAMERA_NUM_BUFFERS       3     
#define GB_CAMERA_BUFFER_SIZE       4096  

enum gb_data_stream_state {
	GB_DATA_STATE_IDLE,
	GB_DATA_STATE_STREAMING,
	GB_DATA_STATE_STOPPING,
};

struct gb_camera_fragment_state {
	uint32_t frame_id;
	uint32_t sequence_num;
};

struct gb_camera_data_ctx {
	const struct device *video_dev;
	uint16_t data_cport;
	atomic_t stream_state;
	
	struct k_work_delayable frame_work;
	struct k_mutex data_lock;
	
	struct gb_camera_fragment_state frag_state;
};

int gb_camera_data_init(struct gb_camera_data_ctx *ctx, const struct device *video_device, uint16_t cport);

int gb_camera_data_start_stream(struct gb_camera_data_ctx *ctx);

int gb_camera_data_stop_stream(struct gb_camera_data_ctx *ctx);

#endif /* GREYBUS_CAMERA_DATA_H_ */
