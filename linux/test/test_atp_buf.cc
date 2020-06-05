// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#include "test_atp_buf.hh"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <atp.h>

void TestAtpBufBase::setUp() {
    bm_fd = fd("/dev/atpbuffer");
}

void TestAtpBufBase::tearDown() {
    close(bm_fd);
}

int TestAtpBufBase::fd(const std::string &name) {
    const int fd{ open(name.c_str(), O_RDWR) };
    CPPUNIT_ASSERT_GREATEREQUAL(0, fd);
    return fd;
}

int TestAtpBufBase::get(const size_t size) {
    const auto buf_fd{ atp_buffer_get(bm_fd, size) };
    CPPUNIT_ASSERT_GREATEREQUAL(0, buf_fd);
    return buf_fd;
}

void TestAtpBufBase::put(const int buf_fd) {
    CPPUNIT_ASSERT_EQUAL(0, atp_buffer_put(bm_fd, buf_fd));
}

void TestAtpBufGet::testSinglePage() {
    put(get(getpagesize()));
}

void TestAtpBufGet::testMultiPage() {
    put(get(4 * getpagesize()));
}

void TestAtpBufGet::testB4M() {
    put(get(8388608));
}

void TestAtpBufGet::testContig() {
    const auto buf_fd{ atp_buffer_get_contig(bm_fd, 4 * getpagesize()) };
    CPPUNIT_ASSERT_GREATEREQUAL(0, buf_fd);
    put(buf_fd);
}

void TestAtpBufGet::testContigB4M() {
    CPPUNIT_ASSERT_LESS(0, atp_buffer_get_contig(bm_fd, 8388608));
}

void TestAtpBufGet::testMultiBuf() {
    std::vector<int> buf_fds{ };
    buf_fds.reserve(4);
    for (size_t i{ 0 }; i < buf_fds.capacity(); ++i)
        buf_fds.emplace_back(get(4 * getpagesize()));
    for (auto &buf_fd : buf_fds)
        put(buf_fd);
}

void TestAtpBufShare::testSend() {
    const auto buf_fd{ get(4 * getpagesize()) };
    CPPUNIT_ASSERT_EQUAL(0, atp_buffer_send("/tmp/fd_comm", buf_fd));
}

void TestAtpBufShare::testRecv() {
    const auto buf_fd{ atp_buffer_receive("/tmp/fd_comm") };
    CPPUNIT_ASSERT_GREATEREQUAL(0, buf_fd);
    put(buf_fd);
}

void *TestAtpBufCpuAccess::getCpu(const int buf_fd, const size_t size) {
    auto *buf{ atp_buffer_cpu_get(buf_fd, size) };
    CPPUNIT_ASSERT(buf != nullptr);
    return buf;
}

void TestAtpBufCpuAccess::testSinglePage() {
    const auto buf_sz{ getpagesize() };
    const auto buf_fd{ get(buf_sz) };
    atp_buffer_cpu_put(getCpu(buf_fd, buf_sz), buf_sz);
    put(buf_fd);
}

void TestAtpBufCpuAccess::testMultiPage() {
    const auto buf_sz{ 4 * getpagesize() };
    const auto buf_fd{ get(buf_sz) };
    atp_buffer_cpu_put(getCpu(buf_fd, buf_sz), buf_sz);
    put(buf_fd);
}

void TestAtpBufCpuAccess::testAccess() {
    const auto buf_sz{ 2 * getpagesize() };
    const auto buf_fd{ get(buf_sz) };
    char *buf{ static_cast<char *>(getCpu(buf_fd, buf_sz)) };
    CPPUNIT_ASSERT_EQUAL(0, atp_buffer_cpu_begin(buf_fd));
    std::memset(buf, 'A', buf_sz);
    for (auto i{ 0 }; i < buf_sz; ++i) {
        CPPUNIT_ASSERT(*(buf + i) == 'A');
    }
    CPPUNIT_ASSERT_EQUAL(0, atp_buffer_cpu_end(buf_fd));
    atp_buffer_cpu_put(buf, buf_sz);
    put(buf_fd);
}
