/* SPDX-License-Identifier: BSD-3-Clause-Clear */

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __TEST_ATP_DEV_HH__
#define __TEST_ATP_DEV_HH__

#include <cppunit/extensions/HelperMacros.h>

#include "test_atp_buf.hh"

// Base class for test suites using the ATP device module
class TestAtpDevBase : public TestAtpBufBase {
  public:
    virtual void setUp() override;
    virtual void tearDown() override;
  protected:
    int dev_fd;
    uint64_t unique(void);
};

//! Test suite targeting "ATP_ATTACH_BUFFER" and "ATP_DETACH_BUFFER" APIs
class TestAtpDevAttach : public TestAtpDevBase {
    CPPUNIT_TEST_SUITE(TestAtpDevAttach);
    CPPUNIT_TEST(testAttach);
    CPPUNIT_TEST_SUITE_END();
  protected:
    // Tests attaching an ATP device to a previously allocated buffer
    void testAttach();
};

//! Test suite targeting "ATP_UNIQUE_STREAM" API
class TestAtpDevUniqueStr : public TestAtpDevBase {
    CPPUNIT_TEST_SUITE(TestAtpDevUniqueStr);
    CPPUNIT_TEST(testUniqueStr);
    CPPUNIT_TEST_SUITE_END();
  protected:
    // Tests creating a unique stream identifier through ATP Engine
    void testUniqueStr();
};

//! Test suite targeting "ATP_PLAY_STREAM" API
class TestAtpDevPlayStr : public TestAtpDevBase {
    CPPUNIT_TEST_SUITE(TestAtpDevPlayStr);
    CPPUNIT_TEST(testPlayStr);
    CPPUNIT_TEST(testMultiPlayStr);
    CPPUNIT_TEST_SUITE_END();
  public:
    virtual void setUp() override;
    virtual void tearDown() override;
  protected:
    int buf_fd;
    uint64_t str_id;
    // Tests activating a stream through ATP Engine
    void testPlayStr();
    /*
     * Tests multiple activations of a stream through ATP Engine
     * Verifies the request handling process in the kernel module
     */
    void testMultiPlayStr();
};

#endif
