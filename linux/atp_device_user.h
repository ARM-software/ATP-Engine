/* SPDX-License-Identifier: BSD-3-Clause-Clear */

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __ATP_DEVICE_USER_H__
#define __ATP_DEVICE_USER_H__

struct atp_data_play_stream {
	/* Unique ID of the stream to play */
	uint64_t atp_stream_id;
	/* Unique flow ID. Identifies the stream activity across the system */
	uint32_t flow_id;
	/*
	 * Read and write buffer file descriptors. At least one must be
	 * provided
	 */
	int read_fd;
	int write_fd;
};

struct atp_data_unique_stream {
	/* ATP stream name, same as in .atp files */
	const char *atp_stream_name;
	/* (Out) Resulting unique stream ID */
	uint64_t atp_stream_id;
};

#define ATP_DEVICE_MAX_LEN_STREAM_NAME 256

#define ATP_DEVICE_IOCTL_BASE 0xE1

/* Attaches the device to an allocated shareable DMA buffer */
#define ATP_ATTACH_BUFFER \
	_IOW(ATP_DEVICE_IOCTL_BASE, 1, int)
/* Detaches the device from an allocated shareable DMA buffer */
#define ATP_DETACH_BUFFER \
	_IOW(ATP_DEVICE_IOCTL_BASE, 2, int)
/* Plays a unique stream in the ATP Engine */
#define ATP_PLAY_STREAM \
	_IOW(ATP_DEVICE_IOCTL_BASE, 3, struct atp_data_play_stream)
/* Instantiates a unique stream in the ATP Engine and retrieves its ID */
#define ATP_UNIQUE_STREAM \
	_IOW(ATP_DEVICE_IOCTL_BASE, 4, struct atp_data_unique_stream)

#endif
