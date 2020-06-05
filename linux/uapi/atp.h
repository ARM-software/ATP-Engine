/* SPDX-License-Identifier: BSD-3-Clause-Clear */

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __ATP_H__
#define __ATP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ATP buffer manager declarations */

/*
 * Allocates and retrieves the FD of a shareable DMA buffer
 *
 * @fd:		 [in] buffer manager file descriptor
 * @size:	 [in] size of the buffer in bytes
 *
 * Returns buffer file descriptor on sucess, negative error code otherwise
 */
int
atp_buffer_get(const int fd, const size_t size);
int
atp_buffer_get_contig(const int fd, const size_t size);

/*
 * Deallocates a shareable DMA buffer
 *
 * @fd:	    [in] buffer manager file descriptor
 * @buf_fd: [in] file descriptor of the buffer to deallocate
 *
 * Returns 0 on sucess, negative error code otherwise
 */
int
atp_buffer_put(const int fd, const int buf_fd);

/*
 * Sends a buffer FD to another process
 * Uses ancillary data over UNIX domain sockets
 *
 * @fpath:  [in] UNIX domain socket location
 * @buf_fd: [in] buffer file descriptor to send
 *
 * Returns 0 on sucess, negative error code otherwise
 */
int
atp_buffer_send(const char *fpath, const int buf_fd);

/*
 * Receives a buffer FD from another process
 * Uses ancillary data over UNIX domain sockets
 *
 * @fpath: [in] UNIX domain socket location
 *
 * Returns buffer file descriptor on sucess, negative error code otherwise
 */
int
atp_buffer_receive(const char *fpath);

/*
 * Maps a shareable DMA buffer to the caller process address space
 * Supports mapping the entire buffer or any sub-range
 *
 * @buf_fd: [in] file descriptor of the buffer to map
 * @size:   [in] range of the buffer to map
 *
 * Returns pointer to mapped buffer, NULL otherwise
 */
void *
atp_buffer_cpu_get(const int buf_fd, const size_t size);

/*
 * Unmaps a shareable DMA buffer from the caller process address space
 *
 * @buffer: [in] buffer to unmap
 * @size:   [in] range of the buffer to unmap
 */
void
atp_buffer_cpu_put(void *buffer, const size_t size);

/*
 * Synchronises the system for the beginning of a CPU access to a buffer
 *
 * @buf_fd: [in] file descriptor of the accessed buffer
 *
 * Returns 0 on success, negative error code otherwise
 */
int
atp_buffer_cpu_begin(const int buf_fd);

/*
 * Synchronises the system for the ending of a CPU access to a buffer
 *
 * @buf_fd: [in] file descriptor of the accessed buffer
 *
 * Returns 0 on success, negative error code otherwise
 */
int
atp_buffer_cpu_end(const int buf_fd);

/*
 * Associates a device to a shareable DMA buffer
 * Enables mapping and access from the device to the buffer
 *
 * @fd:	    [in] device file descriptor
 * @buf_fd: [in] buffer file descriptor
 *
 * Returns 0 on success, negative error code otherwise
 */
int
atp_device_attach(const int fd, const int buf_fd);

/*
 * Dissociates a device from a shareable DMA buffer
 *
 * @fd:	    [in] device file descriptor
 * @buf_fd: [in] buffer file descriptor
 *
 * Returns 0 on success, negative error code otherwise
 */
int
atp_device_detach(const int fd, const int buf_fd);

/*
 * Instructs ATP Engine to activate a stream of traffic profiles
 * Maps read and/or write buffers to device address space
 * This is a blocking call, puts caller to sleep until completion
 * Completion is signaled by an interrupt from the device
 *
 * @fd:	       [in] device file descriptor
 * @stream_id: [in] unique stream ID
 * @flow_id:   [in] unique system flow ID
 * @read_fd:   [in] read DMA buffer file descriptor
 * @write_fd:  [in] write DMA buffer file descriptor
 *
 * Returns 0 on success, negative error code otherwise
 */
int
atp_device_play_stream(const int fd, const uint64_t stream_id,
		       const uint32_t flow_id, const int read_fd,
		       const int write_fd);

/*
 * Instructs ATP Engine to generate a unique, independet stream instance
 * The stream instance is associated to the device
 *
 * @fd:		 [in] device file descriptor
 * @stream_name: [in] stream name
 * @stream_id:	 [out] resulting unique stream ID
 *
 * Returns 0 on success, negative error code otherwise
 */
int
atp_device_unique_stream(const int fd, const char *stream_name,
			 uint64_t *stream_id);

#ifdef __cplusplus
}
#endif

#endif
