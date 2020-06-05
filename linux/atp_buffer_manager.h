/* SPDX-License-Identifier: BSD-3-Clause-Clear */

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __ATP_BUFFER_MANAGER_H__
#define __ATP_BUFFER_MANAGER_H__

#include <linux/types.h>
#include <linux/dma-direction.h>

struct file;
struct dma_buf;
struct dma_buf_attachment;
struct vm_area_struct;

struct atp_buffer_data {
	/* Physical memory pages allocated for the buffer */
	size_t num_pages;
	struct page **pages;
	/* Whether this buffer is physically contiguous in memory */
	bool contig;
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
atp_buffer_manager_ioctl(struct file *file, unsigned int cmd,
			 unsigned long arg);

/*
 * Allocates and exports a shareable DMA buffer
 *
 * @size:	 [in] size of the buffer in bytes
 * @contig:	 [in] true if the buffer should be physically contiguous
 *
 * Returns pointer to the allocated buffer on success, ERR_PTR otherwise
 */
static struct dma_buf *
atp_buffer_manager_export(const size_t size, const bool contig);

/*
 * Callback triggered when a user attaches a device to a shareable DMA buffer
 *
 * @buf: [in] buffer being attached to
 * @att: [in] buffer attachment data
 *
 * Returns 0 on success, negative error code otherwise
 */
static int
atp_buffer_manager_attach(struct dma_buf *buf, struct dma_buf_attachment *att);

/*
 * Callback triggered when a user detaches a device from a shareable DMA buffer
 *
 * @buf: [in] buffer being detached from
 * @att: [in] buffer attachment data
 */
static void
atp_buffer_manager_detach(struct dma_buf *buf, struct dma_buf_attachment *att);

/*
 * Callback triggered when a user maps a shareable DMA buffer to a device
 * address space
 *
 * @att: [in] buffer attachment data
 * @dir: [in] mapping direction
 *
 * Returns pointer to the mapped SG table on success, ERR_PTR otherwise
 */
static struct sg_table *
atp_buffer_manager_map(struct dma_buf_attachment *att,
		       enum dma_data_direction dir);

/*
 * Callback triggered when a user unmaps a shareable DMA buffer from a device
 * address space
 *
 * @att: [in] buffer attachment data
 * @sgt: [in] mapped SG table to unmap
 * @dir: [in] mapping direction
 */
static void
atp_buffer_manager_unmap(struct dma_buf_attachment *att, struct sg_table *sgt,
			 enum dma_data_direction dir);

/*
 * Callback triggered when a user maps a shareable DMA buffer to its virtual
 * address space
 *
 * @buf: [in] buffer to be mapped
 * @vma: [in] virtual memory area where to map the buffer
 *
 * Returns 0 on success, negative error code otherwise
 */
static int
atp_buffer_manager_mmap(struct dma_buf *buf, struct vm_area_struct *vma);

/*
 * Callback triggered when the last reference to shareable DMA buffer is
 * released. Deallocates the buffer and associated data.
 *
 * @buf: [in] buffer to be deallocated
 */
static void
atp_buffer_manager_release(struct dma_buf *buf);

#endif
