// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#include "test_atp_dev.hh"

#include <limits>
#include <unistd.h>

#include <atp.h>

void TestAtpDevBase::setUp() {
    TestAtpBufBase::setUp();
    dev_fd = fd("/dev/atpSTREAM");
}

void TestAtpDevBase::tearDown() {
    close(dev_fd);
    TestAtpBufBase::tearDown();
}

uint64_t TestAtpDevBase::unique() {
    const char *str_name{ "ROOT" };
    auto id{ std::numeric_limits<uint64_t>::max() };
    CPPUNIT_ASSERT_EQUAL(0, atp_device_unique_stream(dev_fd, str_name, &id));
    CPPUNIT_ASSERT(id != std::numeric_limits<uint64_t>::max());
    return id;
}

void TestAtpDevAttach::testAttach() {
    const auto buf_fd{ get(4 * getpagesize()) };
    CPPUNIT_ASSERT_EQUAL(0, atp_device_attach(dev_fd, buf_fd));
    CPPUNIT_ASSERT_EQUAL(0, atp_device_detach(dev_fd, buf_fd));
    put(buf_fd);
}

void TestAtpDevUniqueStr::testUniqueStr() {
    unique();
}

void TestAtpDevPlayStr::setUp() {
    TestAtpDevBase::setUp();
    // Size expected by ROOT stream
    buf_fd = get(3200);
    CPPUNIT_ASSERT_EQUAL(0, atp_device_attach(dev_fd, buf_fd));
    str_id = unique();
}

void TestAtpDevPlayStr::tearDown() {
    CPPUNIT_ASSERT_EQUAL(0, atp_device_detach(dev_fd, buf_fd));
    put(buf_fd);
    TestAtpDevBase::tearDown();
}

void TestAtpDevPlayStr::testPlayStr() {
    CPPUNIT_ASSERT_EQUAL(0,
            atp_device_play_stream(dev_fd, str_id, 1, buf_fd, -1));
}

void TestAtpDevPlayStr::testMultiPlayStr() {
    CPPUNIT_ASSERT_EQUAL(0,
            atp_device_play_stream(dev_fd, str_id, 1, buf_fd, -1));
    CPPUNIT_ASSERT_EQUAL(0,
            atp_device_play_stream(dev_fd, str_id, 1, buf_fd, -1));
}
