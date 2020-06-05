// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#include <regex>

#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestRunner.h>
#include <cppunit/CompilerOutputter.h>

#include "test_atp_buf.hh"
#include "test_atp_dev.hh"

int main(int argc, char *argv[]) {
    const bool run_all{ argc < 2 };

    CPPUNIT_TEST_SUITE_REGISTRATION(TestAtpBufGet);
    // Exclude IPC tests when running all tests
    if (!run_all) { CPPUNIT_TEST_SUITE_REGISTRATION(TestAtpBufShare); }
    CPPUNIT_TEST_SUITE_REGISTRATION(TestAtpBufCpuAccess);
    CPPUNIT_TEST_SUITE_REGISTRATION(TestAtpDevAttach);
    CPPUNIT_TEST_SUITE_REGISTRATION(TestAtpDevUniqueStr);
    CPPUNIT_TEST_SUITE_REGISTRATION(TestAtpDevPlayStr);

    CPPUNIT_NS::TestResult controller{ };
    CPPUNIT_NS::TestResultCollector result{ };
    CPPUNIT_NS::BriefTestProgressListener progress{ };
    CPPUNIT_NS::TestRunner runner{ };

    controller.addListener(&result);
    controller.addListener(&progress);

    auto *suites{ static_cast<CPPUNIT_NS::TestSuite *>(
            CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest()) };
    if (run_all) {
        runner.addTest(suites);
    } else {
        // Run subset of tests based on the passed regex
        const std::regex re{ argv[1] };
        for (auto &suite : suites->getTests()) {
            for (auto i{ 0 }; i < suite->getChildTestCount(); ++i) {
                auto *test{ suite->getChildTestAt(i) };
                if (std::regex_search(test->getName(), re))
                    runner.addTest(test);
            }
        }
    }

    runner.run(controller);

    CPPUNIT_NS::CompilerOutputter outputter{ &result, CPPUNIT_NS::stdCOut()} ;
    outputter.write();

    return result.wasSuccessful() ? 0 : 1;
}
