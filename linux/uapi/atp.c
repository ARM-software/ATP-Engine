// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#define _GNU_SOURCE

#include "atp.h"

#include <linux/atp_buffer_manager_user.h>
#include <linux/atp_device_user.h>
#include <linux/dma-buf.h>

#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* ATP buffer manager definitions */

int
atp_buffer_allocate(const int fd, const size_t size, const bool contig)
{
	struct atp_data_get_buf data;
	int error;

	data.size = size;
	data.contig = contig;

	error = ioctl(fd, ATP_GET_BUF, &data);
	if (error)
		return error;

	return data.fd;
}

int
atp_buffer_get(const int fd, const size_t size)
{
	return atp_buffer_allocate(fd, size, false);
}

int
atp_buffer_get_contig(const int fd, const size_t size)
{
	return atp_buffer_allocate(fd, size, true);
}

int
atp_buffer_put(const int fd, const int buf_fd)
{
	int error;

	error = ioctl(fd, ATP_PUT_BUF, &buf_fd);
	if (error)
		return error;

	return 0;
}

int
atp_buffer_send(const char *fpath, const int buf_fd)
{
	int error, csocket;
	struct sockaddr_un socket_addr;
	union {
		struct cmsghdr cmh;
		char control[CMSG_SPACE(sizeof(int))];
	} ctrl_un;
	char unused;
	struct iovec iov;
	struct msghdr hdr;
	struct cmsghdr *hp;
	ssize_t bytes_sent;

	// Create the client socket
	error = csocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (error == -1)
		goto atp_buffer_send_end;

	// Setup the socket file path
	memset(&socket_addr, 0, sizeof(socket_addr));
	socket_addr.sun_family = AF_UNIX;
	strncpy(socket_addr.sun_path, fpath, sizeof(socket_addr.sun_path) - 1);

	// Connect to the server socket, wait for the buffer
	do {
		error = connect(csocket, (struct sockaddr *) &socket_addr,
				sizeof(socket_addr));
	} while (error);

	// Transmit buffer FD as ancillary data
	memset(&ctrl_un, 0, sizeof(ctrl_un));
	unused = 'u';
	iov.iov_base = &unused;
	iov.iov_len = sizeof(unused);
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;
	hdr.msg_control = ctrl_un.control;
	hdr.msg_controllen = sizeof(ctrl_un.control);
	hp = CMSG_FIRSTHDR(&hdr);
	hp->cmsg_len = CMSG_LEN(sizeof(int));
	hp->cmsg_level = SOL_SOCKET;
	// SCM_RIGHTS is required for passing FDs to other processes
	hp->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(hp)) = buf_fd;

	bytes_sent = sendmsg(csocket, &hdr, 0);
	if (bytes_sent < 0) {
		error = -2;
		goto atp_buffer_send_close;
	}

	error = 0;
atp_buffer_send_close:
	close(csocket);
atp_buffer_send_end:
	return error;
}

int
atp_buffer_receive(const char *fpath)
{
	int buf_fd, ssocket, conn_socket;
	struct sockaddr_un socket_addr;
	union {
		struct cmsghdr cmh;
		char control[CMSG_SPACE(sizeof(int))];
	} ctrl_un;
	struct iovec iov;
	char iov_buf[10];
	struct msghdr hdr;
	struct cmsghdr *hp;
	ssize_t bytes_received;

	buf_fd = -1;

	// Create the server socke
	ssocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ssocket == -1)
		goto atp_buffer_receive_end;

	// Setup the socket file path
	memset(&socket_addr, 0, sizeof(socket_addr));
	socket_addr.sun_family = AF_UNIX;
	strncpy(socket_addr.sun_path, fpath, sizeof(socket_addr.sun_path) - 1);
	unlink(fpath);

	// Bind the address to the server socket file descriptor
	if (bind(ssocket, (struct sockaddr *) &socket_addr,
		 sizeof(socket_addr)))
		goto atp_buffer_receive_close;

	// Listen for connections
	if (listen(ssocket, 8))
		goto atp_buffer_receive_close;

	// Accept the incoming connection from sender
	conn_socket = accept(ssocket, NULL, NULL);
	if (conn_socket == -1)
		goto atp_buffer_receive_close;

	// Describe ancillary data. Length is the size of an int, since that is
	// the data type of the buffer file descriptor
	ctrl_un.cmh.cmsg_len = CMSG_LEN(sizeof(int));
	ctrl_un.cmh.cmsg_level = SOL_SOCKET;
	ctrl_un.cmh.cmsg_type = SCM_CREDENTIALS;
	iov.iov_base = iov_buf;
	iov.iov_len = sizeof(iov_buf);
	hdr.msg_control = ctrl_un.control;
	hdr.msg_controllen = sizeof(ctrl_un.control);
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;

	// Wait for reception of the buffer FD
	bytes_received = recvmsg(conn_socket, &hdr, 0);
	if (bytes_received <= 0)
		goto atp_buffer_receive_close;

	// Extract the buffer FD
	hp = CMSG_FIRSTHDR(&hdr);
	if (hp && (hp->cmsg_len == CMSG_LEN(sizeof(int))) &&
	    (hp->cmsg_level == SOL_SOCKET) && (hp->cmsg_type == SCM_RIGHTS))
		buf_fd = *(int *) CMSG_DATA(hp);

atp_buffer_receive_close:
	close(ssocket);
atp_buffer_receive_end:
	return buf_fd;
}

void *
atp_buffer_cpu_get(const int buf_fd, const size_t size)
{
	void *buffer;

	buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
		      buf_fd, 0);
	if (buffer == MAP_FAILED)
		return NULL;

	return buffer;
}

void
atp_buffer_cpu_put(void *buffer, const size_t size)
{
	munmap(buffer, size);
}

int
atp_buffer_cpu_access(const int buf_fd, const uint64_t access_flag)
{
	struct dma_buf_sync sync;
	int error;

	sync.flags = access_flag | DMA_BUF_SYNC_RW;
	error = ioctl(buf_fd, DMA_BUF_IOCTL_SYNC, &sync);
	if (error)
		return error;

	return 0;
}

int
atp_buffer_cpu_begin(const int buf_fd)
{
	return atp_buffer_cpu_access(buf_fd, DMA_BUF_SYNC_START);
}

int
atp_buffer_cpu_end(const int buf_fd)
{
	return atp_buffer_cpu_access(buf_fd, DMA_BUF_SYNC_END);
}

/* ATP device definitions */

int
atp_device_attachment(const int fd, const int buf_fd, const unsigned int call)
{
	int error;

	error = ioctl(fd, call, &buf_fd);
	if (error)
		return error;

	return 0;
}

int
atp_device_attach(const int fd, const int buf_fd)
{
	return atp_device_attachment(fd, buf_fd, ATP_ATTACH_BUFFER);
}

int
atp_device_detach(const int fd, const int buf_fd)
{
	return atp_device_attachment(fd, buf_fd, ATP_DETACH_BUFFER);
}

int
atp_device_play_stream(const int fd, const uint64_t stream_id,
		       const uint32_t flow_id, const int read_fd,
		       const int write_fd)
{
	struct atp_data_play_stream data;
	int error;

	data.atp_stream_id = stream_id;
	data.flow_id = flow_id;
	data.read_fd = read_fd;
	data.write_fd = write_fd;
	error = ioctl(fd, ATP_PLAY_STREAM, &data);
	if (error)
		return error;

	return 0;
}

int
atp_device_unique_stream(const int fd, const char *stream_name,
			 uint64_t *stream_id)
{
	struct atp_data_unique_stream data;
	int error;

	data.atp_stream_name = stream_name;
	error = ioctl(fd, ATP_UNIQUE_STREAM, &data);
	if (error)
		return error;

	*stream_id = data.atp_stream_id;
	return 0;
}
