/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Sep 30, 2015
 *      Author: Matteo Andreozzi
 */
// standard library includes
#include <stdlib.h>
#include <getopt.h>
#include <csignal>
// cpp unit includes
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestSuite.h>

#include "test_atp.hh"

// logger and tools
#include "logger.hh"
#include "utilities.hh"

// interactive shell
#include "shell.hh"

using namespace TrafficProfiles;
using namespace std;

/*
 * Prints the command line options and exits
 */
void usage() {

    PRINT(  "\n "
            "******** ATP ENGINE *******\n",
            "** by Matteo Andreozzi **\n",
            "*************************\n\n",
            "Usage: <options> file1.atp file2.atp\n",
            "\t -v (--verbose) : enables debug logging\n",
            "\t -b (--bandwidth) <value>: configures the memory bandwidth\n",
            "\t -l (--latency) <value>: configures the memory latency\n",
            "\t -p (--profiles-as-masters): instantiates one ATP master per ATP FIFO\n",
            "\t -t (--trace) <value>: enables tracing to the specified directory\n"
            "\t -i (--interactive): starts the Engine in interactive shell mode\n"
            "\t -? or -h (--help): Prints usage and exits\n\n",
            "No file arguments: runs self-tests\n");
}

// instantiate test suite
TestAtp test;

void signalHandler( int signum ) {
   PRINT("Interrupt signal (",signum,") received");

   // print statistics and exit

   test.dumpStats();

   exit(signum);
}

int main(int argc, char* argv[]) {

    // enable colours
    Logger::get()->setColours(true);
    // register signal handler
    signal(SIGINT, signalHandler);

    // default parameters
    const string defaultBandwidth = "32GB/s";
    const string defaultLatency = "80ns";
    const string defaultTraceDir = "out";
    // option flags and index counters
    int opt = 0, option_index = 0;
    int verbose_flag=0, trace_flag=0,
            interactive_flag=0, profiles_as_masters_flag=0;

    // long options vector
    option long_options[] =
    {
            {"interactive", no_argument, &interactive_flag, 1},
            {"verbose",     no_argument, &verbose_flag, 1},
            {"help",        no_argument, 0,       '?'},
            {"help",        no_argument, 0,       'h'},
            {"profiles-as-masters", no_argument, 0, 'p'},
            {"latency",     required_argument, 0, 'l'},
            {"bandwidth",   required_argument, 0, 'b'},
            {"trace",       optional_argument, &trace_flag, 1},
            {0, 0, 0, 0}
    };


    // set defaults
    string bandwidth(defaultBandwidth);
    string latency(defaultLatency);
    string traceDir(defaultTraceDir);

    // parse options
    while ((opt = getopt_long(argc,argv,":ivpb:l:t:?h",
            long_options, &option_index)) != EOF) {
        switch(opt)
        {
        case 0: {
            // long option set - nothing to do
            break;
        }
        case 'i': {
            interactive_flag=1;
            break;
        }

        case 'p':{
            profiles_as_masters_flag=1;
            break;
        }

        case 'v': {
            verbose_flag = 1;
            break;
        }
        case 'b': {
            // assign value
            bandwidth = optarg;
            break;
        }
        case 'l': {
            latency = optarg;
            break;
        }
        case 't': {
            trace_flag = 1;
            if (optarg){
                traceDir = optarg;
            }
            break;
        }
        case 'h':
        case '?': /* intentional fallthrough */
        default:
        {
            // print usage and exit
            usage();
            exit(0);
        }
        }
    }

    // handle flag
    if (verbose_flag) {
        // enable logging
        Logger::get()->setLevel(Logger::DEBUG_LEVEL);
        LOG("ATP Engine: Debug logging enabled from command line");
    }

    // interactive mode bypasses self-tests and profiles loading

    if (interactive_flag) {
        Shell::get()->setTest(&test);
        Shell::get()->loop();
    } else if (optind == argc) {
        // if no command line files are provided
        // run self-tests
        CppUnit::TextUi::TestRunner runner;
        LOG("ATP Engine: Creating Test Suites");
        runner.addTest(TestAtp::suite());
        LOG("ATP Engine: Running the unit tests");
        runner.run();
    } else {
        // handle trace flag
        if (trace_flag) {
            test.getTpm()->enableTracer(traceDir);
        }

        // handle profiles as masters flag
        if (profiles_as_masters_flag) {
            test.getTpm()->enableProfilesAsMasters();
        }

        for (int i = optind; i < argc; ++i)
        {
            if (test.buildManager_fromFile(argv[i])) {
                LOG("ATP Engine: loading profiles from file", argv[i]);
            } else {
                ERROR("ATP Engine: unable to load file", argv[i]);
            }
        }

        // start the test
        test.testAgainstInternalSlave(bandwidth, latency);
        // cleanup
        test.tearDown();
    }
    return 0;
}
