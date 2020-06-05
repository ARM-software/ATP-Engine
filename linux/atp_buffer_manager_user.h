/* SPDX-License-Identifier: BSD-3-Clause-Clear */

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __ATP_BUFFER_MANAGER_USER_H__
#define __ATP_BUFFER_MANAGER_USER_H__

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stddef.h>
#include <sys/ioctl.h>
#endif

struct atp_data_get_buf {
	/* Buffer size in bytes */
	size_t size;
	/* (Out) Resulting file descriptor idenitifying the buffer */
	int fd;
	/*
	 * Specifies the buffer as physically contiguous in memory. Intended to
	 * allow access to the buffer from devices not behind an IOMMU. Can be
	 * safely used for buffers up to 1 MiB in size; allocations from 1 MiB
	 * to 4 MiB have a small chance of succeeding; allocations bigger than
	 * 4 MiB will return always fail.
	 */
	bool contig;
};

#define ATP_BUFFER_MANAGER_IOCTL_BASE 0xE2

/* Allocate and retrieve the FD of a shareable DMA buffer */
#define ATP_GET_BUF \
	_IOWR(ATP_BUFFER_MANAGER_IOCTL_BASE, 1, struct atp_data_get_buf)
/* Deallocate a shareable DMA buffer */
#define ATP_PUT_BUF \
	_IOW(ATP_BUFFER_MANAGER_IOCTL_BASE, 2, int)

#endif
