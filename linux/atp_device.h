/* SPDX-License-Identifier: BSD-3-Clause-Clear */

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __ATP_DEVICE_H__
#define __ATP_DEVICE_H__

#include <linux/types.h>
#include <linux/dma-direction.h>
#include <linux/irqreturn.h>
#include <linux/spinlock_types.h>
#include <linux/miscdevice.h>

struct dma_buf;
struct platform_device;

struct atp_device_data {
	/* ATP device ID, same as in .atp files */
	const char *atp_id;
	/* Play stream requests */
	struct list_head play_requests;
	uint32_t next_play_request_id;
	spinlock_t play_requests_lock;
	/* Allocated interrupt for the device */
	int irq;
	/* Memory region for accessing the device MMIO registers */
	void __iomem *mmio;
	spinlock_t mmio_lock;
	struct miscdevice mdev;
};

/* See gem5/atp_device.hh for a description of MMIO registers */
enum atp_device_offset {
	STREAM_NAME_BASE  = 0x00,
	STREAM_NAME_RANGE = 0x08,
	READ_BASE	  = 0x10,
	READ_RANGE	  = 0x18,
	WRITE_BASE	  = 0x20,
	WRITE_RANGE	  = 0x28,
	STREAM_ID	  = 0x30,
	TASK_ID		  = 0x38,
	IN_REQUEST_ID	  = 0x3c,
	OUT_REQUEST_ID	  = 0x40,
	CONTROL		  = 0x44,
	STATUS		  = 0x45
};

struct atp_request_play_stream {
	/* Unique request ID */
	uint32_t id;
	/* Buffer DMA read address and range */
	dma_addr_t raddr;
	size_t rrange;
	/* Buffer DMA write address and range */
	dma_addr_t waddr;
	size_t wrange;
	/* Unique ATP stream ID. Obtained via ATP_UNIQUE_STREAM */
	uint64_t stream_id;
	/* Unique flow ID. Identifies the stream activity across the system */
	uint32_t flow_id;
	wait_queue_head_t wait;
	bool completed;
	struct list_head node;
};

struct atp_request_unique_stream {
	/* ATP stream name, same as in .atp files */
	const char *stream_name;
	/* Stream base DMA address and range */
	dma_addr_t stream_name_addr;
	size_t stream_name_range;
};

/*
 * IOCTL system call handler
 *
 * @file: [in] data structure corresponding to the FD provided from user-space
 * @cmd:  [in] IOCTL command code (see atp_buffer_manager_user.h)
 * @arg:  [in/out] command argument / parameter
 *
 * Returns 0 on success, negative error code otherwise
 */
static long
atp_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/*
 * Retrieves data for a particular device - buffer attachment
 *
 * @buf: [in] attached buffer
 * @dev: [in] attached device
 *
 * Returns pointer to the attachment data on success, ERR_PTR otherwise
 */
static struct dma_buf_attachment *
atp_device_attachment_get(struct dma_buf *buf, struct device *dev);

/*
 * Maps a shareable DMA buffer to a device address space
 *
 * @buf_fd: [in] buffer file descriptor
 * @dev:    [in] attached device to receive the mapping
 * @dir:    [in] mapping direction
 * @addr:   [out] resulting mapping base address
 * @range:  [out] resulting mapping range
 *
 * Returns 0 on success, negative error code otherwise
 */
static int
atp_device_attachment_map(const int buf_fd, struct device *dev,
			  const enum dma_data_direction dir, dma_addr_t *addr,
			  size_t *range);

/*
 * Allocates a request for a play stream call
 *
 * @dev_data:  [in] device-specific data
 * @stream_id: [in] call-provided stream ID
 * @flow_id:   [in] call-provided flow ID
 *
 * Returns pointer to allocated request
 */
static struct atp_request_play_stream *
atp_device_request_get_play_stream(struct atp_device_data *dev_data,
				   const uint64_t stream_id,
				   const uint32_t flow_id);

/*
 * Looks up a play stream call request in the device pending list
 *
 * @dev_data: [in] device-specific data
 * @req_id:   [in] request ID used in the lookup
 *
 * Returns pointer to the pending request, ERR_PTR otherwise
 */
static struct atp_request_play_stream *
atp_device_request_get_play_stream_by_id(struct atp_device_data *dev_data,
					 const uint32_t req_id);

/*
 * Deallocates a request for a play stream call
 *
 * @dev_data: [in] device-specific data
 * @req:      [in] request to be deallocated
 */
static void
atp_device_request_put_play_stream(struct atp_device_data *dev_data,
				   struct atp_request_play_stream *req);

/*
 * Allocates a request for a unique stream call
 *
 * @stream_name: [in] call-provided stream name
 *
 * Returns pointer to allocated request
 */
static struct atp_request_unique_stream *
atp_device_request_get_unique_stream(const char __user *stream_name);

/*
 * Deallocates a request for a unique stream call
 *
 * @req: [in] request to be deallocated
 */
static void
atp_device_request_put_unique_stream(struct atp_request_unique_stream *req);

/*
 * Programs a device MMIO registers to play a stream
 *
 * @dev_data: [in] device-specific data
 * @req:      [in] request with specific programming parameters
 */
static void
atp_device_mmio_play_stream(struct atp_device_data *dev_data,
			    struct atp_request_play_stream *req);

/*
 * Programs a device MMIO registers to instantiate a unique stream
 *
 * @dev_data:  [in] device-specific data
 * @req:       [in] request with specific programming parameters
 * @stream_id: [out] resulting unique stream ID
 */
static void
atp_device_mmio_unique_stream(struct atp_device_data *dev_data,
			      struct atp_request_unique_stream *req,
			      uint64_t *stream_id);

/*
 * Common ATP device interrupt service routine
 *
 * @irq:    [in] device-specific interrupt received
 * @handle: [in] pointer to device-specific data
 *
 * Returns IRQ_HANDLED
 */
static irqreturn_t
atp_device_isr(int irq, void *handle);

/*
 * Callback triggered when a compatible ATP device is discovered
 *
 * @pdev: [in] discovered device
 *
 * Returns 0 on success, negative error code otherwise
 */
static int
atp_device_probe(struct platform_device *pdev);

/*
 * Callback triggered when a compatible ATP device is removed
 *
 * @pdev: [in] removed device
 *
 * Returns 0
 */
static int
atp_device_remove(struct platform_device *pdev);

#endif
