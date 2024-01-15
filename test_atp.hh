/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 19, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef _AMBA_TRAFFIC_PROFILE_TEST_ATP_HH_
#define _AMBA_TRAFFIC_PROFILE_TEST_ATP_HH_

// std includes

#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <map>

// Unit test suit includes

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestSuite.h>
#include <cppunit/TestCase.h>

// ATP includes

#include "logger.hh"
#include "traffic_profile_manager.hh"

using namespace std;

namespace TrafficProfiles {
/*!
 *\brief Unit Tests class for ATP
 *
 * Implements Unit Test to validate the ATP implementation,
 */
class TestAtp : public CppUnit::TestFixture  {

  private:
    //! pointer to ATP traffic profile manager
    TrafficProfileManager* tpm;
    //! Google Protocol Buffer ATP manager configuration objects
    Configuration* configuration;

    struct ProfileDescription {
        const string name;
        const Profile::Type type;
        const string *const master;
        const list<string> *const waitFor;
        // iommu_id and flow_id have no default values in constructor as they
        // are optional fields
        uint64_t iommu_id;
        uint64_t flow_id;
        ProfileDescription(const string &n, const Profile::Type &t,
                           const string *const m=nullptr,
                           const list<string> *const wf=nullptr)
            : name { n }, type { t }, master { m }, waitFor { wf } { }
    };

  public:
    //! Default Constructor
    TestAtp();
    //! Default Destructor
    virtual ~TestAtp();

    //! Creates a CppUnit test suite
    static CppUnit::TestSuite* suite();

    //! Unit Tests setup method
    void setUp();

    //! Unit Tests tear down method
    void tearDown();

    //! Dumps the TPM Stats
    void dumpStats();

    //! returns a pointer to the stored TPM - creates it if needed
    inline TrafficProfileManager* getTpm() {
        if (nullptr == tpm) { tpm = new TrafficProfileManager(); } return tpm;}

  protected:

    /*!
     * Configures a Packet Configuration
     *\param t the Packet Configuration to be configured
     *\param cmd the Packet Command to be used for generating packets
     *\param wait the Packet Command related to responses waited for by this Packet Configuration
     *\return a pointer to the passed Packet Configuration
     */
    PatternConfiguration* makePatternConfiguration(PatternConfiguration*, const Command, const Command = NONE);

    /*!
     * Builder for traffic profiles timing info
     *\param t the FifoConfiguration object to be filled
     *\param fullLevel the FIFO model maximum fill level
     *\param level indicates whether the FIFO starts full or empty
     *\param ot the maximum number of outstanding transactions for the associated Traffic Profile
     *\param total the total number of transaction the associated Traffic Profile will send
     *\param rate the FIFO model fill/depletion rate
     *\return a pointer to the FifoConfiguration
     */
    FifoConfiguration* makeFifoConfiguration(FifoConfiguration* , const uint64_t,
            const FifoConfiguration::StartupLevel, const uint64_t, const uint64_t, const uint64_t);

    /*!
     * Creates a new Traffic Profile
     *\param p the Profile to be configured
     *\param desc description of the new Profile
     */
    void
    makeProfile(Profile *p, const ProfileDescription &desc) const;

  public:

    //! Builds a TPM loading from file
    bool buildManager_fromFile(const string& );

    /*!
     * Requests the TPM to send packets from masters
     * and routes them to an internal ATP Slave
     *\param rate memory bandwidth of the slave
     *\param latency request to response latency
     */
    void testAgainstInternalSlave(const string&, const string&);


    //! UNIT TESTS

    //! tests the ATP FIFO model
    void testAtp_fifo();

    //! tests the ATP Event
    void testAtp_event();

    //! tests the ATP packet descriptor
    void testAtp_packetDesc();

    //! tests the ATP packet tagger functionality
    void testAtp_packetTagger();

    //! tests the statistics object
    void testAtp_stats();

    //! tests the traffic profile object
    void testAtp_trafficProfile();

    //! tests the ATP packet tagger creation logic
    void testAtp_packetTaggerCreation();

    //! tests the traffic profile manager
    void testAtp_tpm();

    //! tests the traffic profile delay
    void testAtp_trafficProfileDelay();

    //! tests the unit conversion functions
    void testAtp_unitConversion();

    //! tests the Kronos engine
    void testAtp_kronos();

    //! tests the traffic profile slave
    void testAtp_trafficProfileSlave();

    //! tests the Traffic Profile Manager routing functionality
    void testAtp_trafficProfileManagerRouting();
};

} // end of namespace

#endif /* _AMBA_TRAFFIC_PROFILE_TEST_ATP_HH_ */