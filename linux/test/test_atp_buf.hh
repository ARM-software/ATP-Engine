/* SPDX-License-Identifier: BSD-3-Clause-Clear */

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __TEST_ATP_BUF_HH__
#define __TEST_ATP_BUF_HH__

#include <cppunit/extensions/HelperMacros.h>

//! Base class for test suites using the ATP buffer manager module
class TestAtpBufBase : public CPPUNIT_NS::TestFixture {
  public:
    virtual void setUp() override;
    virtual void tearDown() override;
  protected:
    //! Buffer manager file descriptor
    int bm_fd;
    //! Helper. Returns a module file descriptor. Checks correctness.
    int fd(const std::string &name);
    //! Helper. Returns a buffer file descriptor. Checks correctness.
    int get(const size_t size);
    //! Helper. Releases a buffer. Checks correctness.
    void put(const int buf_fd);
};

//! Test suite targeting "ATP_GET_BUF" and "ATP_PUT_BUF" APIs
class TestAtpBufGet : public TestAtpBufBase {
    CPPUNIT_TEST_SUITE(TestAtpBufGet);
    CPPUNIT_TEST(testSinglePage);
    CPPUNIT_TEST(testMultiPage);
    CPPUNIT_TEST(testB4M);
    CPPUNIT_TEST(testContig);
    CPPUNIT_TEST(testContigB4M);
    CPPUNIT_TEST(testMultiBuf);
    CPPUNIT_TEST_SUITE_END();
  protected:
    //! Tests single-page buffer allocation
    void testSinglePage();
    //! Tests multi-page buffer allocation
    void testMultiPage();
    //! Tests bigger than 4MiB (contiguous limit) buffer allocation
    void testB4M();
    //! Tests contiguous buffer allocation
    void testContig();
    //! Tests contiguous bigger than 4MiB buffer allocation
    void testContigB4M();
    //! Tests multiple outstanding buffer allocations
    void testMultiBuf();
};

/*!
 * Test suite targeting "atp_buffer_send" and "atp_buffer_receive" APIs
 * Disabled by default when running all test suites
 * Sender and receiver should be executed in different Linux processes
 */
class TestAtpBufShare : public TestAtpBufBase {
    CPPUNIT_TEST_SUITE(TestAtpBufShare);
    CPPUNIT_TEST(testSend);
    CPPUNIT_TEST(testRecv);
    CPPUNIT_TEST_SUITE_END();
  protected:
    //! Tests sending a buffer file descriptor over UNIX sockets
    void testSend();
    //! Tests receiving a buffer file descriptor over UNIX sockets
    void testRecv();
};

//! Test suite targeting "atp_buffer_cpu_*" APIs
class TestAtpBufCpuAccess : public TestAtpBufBase {
    CPPUNIT_TEST_SUITE(TestAtpBufCpuAccess);
    CPPUNIT_TEST(testSinglePage);
    CPPUNIT_TEST(testMultiPage);
    CPPUNIT_TEST(testAccess);
    CPPUNIT_TEST_SUITE_END();
  protected:
    //! Tests single-page buffer CPU address space mapping
    void testSinglePage();
    //! Tests multi-page buffer CPU address space mapping
    void testMultiPage();
    //! Tests access to a buffer from CPU address space
    void testAccess();
    //! Helper. Returns a CPU mapped buffer. Checks correcteness.
    void *getCpu(const int buf_fd, const size_t size);
};

#endif
