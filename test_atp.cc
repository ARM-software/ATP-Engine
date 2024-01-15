/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015, 2017 ARM Limited
 * All rights reserved
 *  Created on: Oct 19, 2015
 *      Author: Matteo Andreozzi
 *              Riken Gohil
 */

#include "test_atp.hh"
#include "event.hh"
#include "stats.hh"
#include "fifo.hh"
#include "packet_desc.hh"
#include <vector>
#include <sstream>
#include <algorithm>
#include <cassert>
#include "traffic_profile_desc.hh"
#include "packet_tagger.hh"
#include "utilities.hh"
#include "kronos.hh"
#include "types.hh"

#ifndef CPPUNIT_ASSERT
#define CPPUNIT_ASSERT(x)
#endif

using namespace std;

namespace TrafficProfiles {

TestAtp::TestAtp(): tpm(nullptr), configuration(nullptr) {
}

TestAtp::~TestAtp() {
    tearDown();
}

// test helper functions


PatternConfiguration* TestAtp::makePatternConfiguration(PatternConfiguration* t, const Command cmd, const Command wait) {
    t->set_cmd(cmd);
    t->set_wait_for(wait);
    return t;
}


FifoConfiguration* TestAtp::makeFifoConfiguration(FifoConfiguration* t, const uint64_t fullLevel,
        const FifoConfiguration::StartupLevel level,const uint64_t ot, const uint64_t total, const uint64_t rate) {
    t->set_full_level(fullLevel);
    t->set_start_fifo_level(level);
    t->set_ot_limit(ot);
    t->set_total_txn(total);
    t->set_rate(std::to_string(rate));

    return t;
}

void
TestAtp::makeProfile(Profile *p, const ProfileDescription &desc) const {

    p->set_name(desc.name);
    p->set_type(desc.type);

    if (desc.master) {
        p->set_master_id(*desc.master);
    }
    else {
        p->set_master_id(desc.name);
    }

    if (desc.waitFor) {
        for (const auto& a: *desc.waitFor) {
            p->add_wait_for(a);
        }
    }

    if (desc.iommu_id)
        p->set_iommu_id(desc.iommu_id);

    if (desc.flow_id)
         p->set_flow_id(desc.flow_id);
}

// test specific functions

void  TestAtp::setUp() {
    // create new TPM
    tpm = new TrafficProfileManager();
    // load the Configuration into the TPM
    if (configuration) {
        tpm->configure(*configuration);
        // Dump the TPM configuration
        string out;
        if (!tpm->print(out)) {
            ERROR("unable to dump TPM");
        }
        // display the TPM
        LOG(out);
    }
}


void  TestAtp::tearDown() {
    if (tpm) {
        delete tpm;
        tpm = nullptr;
    }

    if (configuration) {
        delete configuration;
        configuration = nullptr;
    }
}

bool TestAtp::buildManager_fromFile(const string& fileName) {
    // create new TPM if not created yet
    if (nullptr == tpm) {
        tpm = new TrafficProfileManager();
    }
    // load the Configuration into the TPM
    return (tpm->load(fileName));
}

void TestAtp::dumpStats() {
    if (tpm) {
        LOG ("TestAtp::dumpStats dumping TPM stats");
        PRINT("Global TPM Stats:", tpm->getStats().dump());
        for (auto&m: tpm->getMasters()) {
            PRINT(m,"Stats:",tpm->getMasterStats(m).dump());
        }
    }
}

void TestAtp::testAgainstInternalSlave(const string& rate,
        const string& latency) {
    PRINT("ATP Engine running in standalone execution mode. "
            "Internal slave configuration:",rate,latency);
    // the TPM should already be created and loaded with masters
    // query the TPM for masters and assign all unassigned masters to
    // an internal slave
    Profile slave;
    // fill the configuration
    makeProfile(&slave, ProfileDescription { "TestAtp::InternalSlave::",
                                             Profile::READ });
    // fill in the slave field
    SlaveConfiguration* slave_cfg = slave.mutable_slave();
    slave_cfg->set_latency(latency);
    slave_cfg->set_rate(rate);
    slave_cfg->set_granularity(64);
    slave_cfg->set_ot_limit(0);

    // request list of all masters and add unassigned ones to internal slave
    auto& masters = tpm->getMasters();
    auto& masterSlaves = tpm->getMasterSlaves();

    for (auto&m : masters) {
        if (masterSlaves.find(m)==masterSlaves.end()) {
            slave_cfg->add_master(m);
        }
    }
    // register (overwrite) slave with TPM
    tpm->configureProfile(slave, make_pair(1,1), true);

    // request packets to masters and route to the internal slave
    tpm->loop();

    dumpStats();
}

// unit tests

void
TestAtp::testAtp_fifo() {
    Fifo fifo;
    // start with an empty FIFO, which fills every time by 1000
    fifo.init(nullptr, Profile::READ,1000,1,0,2000,true);
    uint64_t next = 0;
    uint64_t request_time=0;
    bool underrun=false, overrun=false;
    uint64_t i = 0;
    for (; i< 2; ++i){
        // read 1000 every time
        fifo.send(underrun, overrun, next, request_time, i, 1000);
        CPPUNIT_ASSERT(fifo.getOt() == 1);
        fifo.receive(underrun, overrun, i, 1000);
        CPPUNIT_ASSERT(fifo.getOt() == 0);
    }
    // checks to see if send allocation was successful
    CPPUNIT_ASSERT(fifo.getLevel() == 1000);

    // consume last data
    fifo.send(underrun, overrun, next, request_time, i, 0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);
    fifo.receive(underrun, overrun, i,0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);

    // we should have an empty FIFO here
    CPPUNIT_ASSERT(fifo.getLevel() == 0);
    CPPUNIT_ASSERT(!underrun);
    CPPUNIT_ASSERT(!overrun);

    // checks for having multiple outstanding transactions
    fifo.send(underrun, overrun, next, request_time, i, 1000);
    fifo.send(underrun, overrun, next, request_time, ++i, 1000);
    CPPUNIT_ASSERT(fifo.getOt() == 2);
    fifo.receive(underrun, overrun, i, 1000);
    fifo.receive(underrun, overrun, ++i, 1000);
    CPPUNIT_ASSERT(fifo.getOt() == 0);

    // we should underrun now
    fifo.send(underrun, overrun, next, request_time, ++i, 0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);
    fifo.receive(underrun, overrun, i,0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);
    CPPUNIT_ASSERT(underrun);
    CPPUNIT_ASSERT(!overrun);

    // clear flags
    underrun=overrun=false;

    // start with a full WRITE FIFO, which fills every time by 1000
    fifo.init(nullptr, Profile::WRITE,1000,1,2000,2000,true);
    i=0;
    for (;i< 2; ++i){
        // read 1000 every time, receive partial 500 twice
        fifo.send(underrun, overrun, next, request_time, i, 1000);
        CPPUNIT_ASSERT(fifo.getOt() == 1);
        fifo.receive(underrun, overrun, i, 500);
        fifo.receive(underrun, overrun, i, 500);
        CPPUNIT_ASSERT(fifo.getOt() == 0);
    }
    // checks to see if send removal was successful
    CPPUNIT_ASSERT(fifo.getLevel() == 1000);
    // cause FIFO update
    fifo.send(underrun, overrun, next, request_time, i, 0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);
    fifo.receive(underrun, overrun, i,0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);

    // we should have a full FIFO here
    CPPUNIT_ASSERT(fifo.getLevel() == 2000);
    CPPUNIT_ASSERT(!underrun);
    CPPUNIT_ASSERT(!overrun);

    // we should overrun now
    fifo.send(underrun, overrun, next, request_time, ++i, 0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);
    fifo.receive(underrun, overrun, i,0);
    CPPUNIT_ASSERT(fifo.getOt() == 0);
    CPPUNIT_ASSERT(!underrun);
    CPPUNIT_ASSERT(overrun);
    CPPUNIT_ASSERT(fifo.getLevel() == 2000);
    // consume 1000
    fifo.send(underrun, overrun, next, request_time, i, 1000);
    CPPUNIT_ASSERT(fifo.getOt() == 1);
    CPPUNIT_ASSERT(fifo.getLevel() == 2000);
    fifo.receive(underrun, overrun, i,1000);
    CPPUNIT_ASSERT(fifo.getLevel() == 1000);
    // reset the FIFO
    fifo.reset();
    // verify that the FIFO is back to startup values
    CPPUNIT_ASSERT(fifo.getOt() == 0);
    CPPUNIT_ASSERT(fifo.getLevel() == 2000);
    for (;i< 2; ++i){
        // read 1000 every time, receive partial 500 twice
        fifo.send(underrun, overrun, next, request_time, i, 1000);
        CPPUNIT_ASSERT(fifo.getOt() == 1);
        fifo.receive(underrun, overrun, i, 500);
        fifo.receive(underrun, overrun, i, 500);
        CPPUNIT_ASSERT(fifo.getOt() == 0);
    }

    // test mid-update cycle activation
    // start with a full READ FIFO, which depletes every 10 units by 1000
    fifo.init(nullptr, Profile::READ,1000,10,2000,2000,true);
    // send request at time 3
    bool ok = fifo.send(underrun, overrun, next, request_time, 13, 1000);
    // verify that we get next time 23 and not 10
    CPPUNIT_ASSERT(!ok);
    CPPUNIT_ASSERT(next == 23);
    ok = fifo.send(underrun, overrun, next, request_time, 21, 1000);
    // verify that we get next time 23 and not 10
    CPPUNIT_ASSERT(!ok);
    CPPUNIT_ASSERT(next == 23);
    // verify that the next send time is correct and that the FIFO transmits
    ok = fifo.send(underrun, overrun, next, request_time, next, 1000);
    CPPUNIT_ASSERT(ok);
    // note : as the FIFO is now at level 1000 with 1000 in-flight, next is 0
    CPPUNIT_ASSERT(next==0);
    // set the time to 33 and see if we get another transaction
    fifo.receive(underrun, overrun, 33, 1000);
    // verify that the 2nd next send time is correct
    ok = fifo.send(underrun, overrun, next, request_time, 33, 1000);
    CPPUNIT_ASSERT(ok);
}

void
TestAtp::testAtp_event() {
    Event ev1(Event::NONE,Event::AWAITED,0,0),
            ev2(Event::NONE,Event::AWAITED,0,0),
            ev3(Event::NONE,Event::TRIGGERED,0,0),
            ev4(Event::TERMINATION,Event::AWAITED,0,0),
            ev5(Event::NONE,Event::TRIGGERED,1,0);

    // test comparison of event objects
    CPPUNIT_ASSERT(ev1==ev2);
    // a triggered and awaited event compare equal
    CPPUNIT_ASSERT(ev1==ev3);
    CPPUNIT_ASSERT(ev1!=ev4);
    CPPUNIT_ASSERT(ev1!=ev5);
    // test parsing event objects from string
    string event = "testAtp_event TERMINATION";
    string name ="";
    Event::Type type = Event::NONE;
    CPPUNIT_ASSERT(Event::parse(type, name, event));
    CPPUNIT_ASSERT(type == Event::TERMINATION);
    CPPUNIT_ASSERT(name == "testAtp_event");
    event = "ERROR ERROR";
    CPPUNIT_ASSERT(!Event::parse(type, name, event));
}

void TestAtp::testAtp_packetDesc() {
    Profile config;
    // fill the configuration
    makeProfile(&config, ProfileDescription { "testAtp_packetDesc_profile",
                                              Profile::READ });

    // fill the FIFO configuration - pointer, maxLevel,
    // startup level, OT, transactions, rate
    makeFifoConfiguration(config.mutable_fifo(),
            0, FifoConfiguration::EMPTY, 0, 0, 0);

    // ATP Packet descriptor
    PacketDesc pd;
    // fill the Google Protocol Buffer configuration object
    makePatternConfiguration(config.mutable_pattern(),
            Command::READ_REQ, Command::READ_RESP);

    // ATP Packet Tagger
    PacketTagger pt;

    // configure the packet address and size generation policies
    auto* pk = config.mutable_pattern();
    pk->set_size(64);
    PatternConfiguration::Address* address = pk->mutable_address();
    address->set_base(0);
    // set increment
    address->set_increment(0x1FBE);
    // set local id limits
    pk->set_lowid(10);
    pk->set_highid(11);

    // initialize the Packet Descriptor and checks if initialized correctly
    pd.init(0, *config.mutable_pattern(), &pt);
    CPPUNIT_ASSERT(pd.isInitialized());
    CPPUNIT_ASSERT(pd.waitingFor() == Command::READ_RESP);

    // register profile
    tpm->configureProfile(config);

    // request a packet three times
    for (uint64_t i=0; i< 3 ; ++i) {
        Packet * p(nullptr);
        bool ok = pd.send(p,0);

        CPPUNIT_ASSERT(ok);
        // test local id generation
        CPPUNIT_ASSERT(p->id()== (10+i>11?10:10+i));

        // set correct type of packet and receive
        p->set_cmd(Command::READ_RESP);
        ok = pd.receive(0, p);
        CPPUNIT_ASSERT(ok);
        // delete packet
        delete p;
    }
    // descriptor re-initialisation to test range reset
    pd.addressReconfigure(0xBEEF, 0x3F7C);
    for (uint64_t i=0; i< 3 ; ++i) {
        Packet * p(nullptr);
        bool ok = pd.send(p,0);

        CPPUNIT_ASSERT(ok);
        // test address reconfiguration
        CPPUNIT_ASSERT(p->addr() ==
                (i == 0 || i == 2 ? 0xBEEF: 0xDEAD));

        // set correct type of packet and receive
        p->set_cmd(Command::READ_RESP);
        ok = pd.receive(0, p);
        CPPUNIT_ASSERT(ok);
        // delete packet
        delete p;
    }

    /*
     * test autoRange - first call fails as autoRange
     * cannot extend an existing range
     * so we still get the old range returned
     */
    CPPUNIT_ASSERT(pd.autoRange(1023) == 0x3F7C);
    // force the packet descriptor autoRange
    CPPUNIT_ASSERT(pd.autoRange(1023, true) == 0x7ED842);

    // test new range applies
    for (uint64_t i=0; i< 3 ; ++i) {
        Packet * p(nullptr);
        // reset before the third packet is sent
        if (i==2) pd.reset();

        bool ok = pd.send(p,0);

        CPPUNIT_ASSERT(ok);
        // test address reconfiguration
        // second packet shouldn't wrap around anymore
        // third packet should now have the base address due to reset
        if (i==0) {
            CPPUNIT_ASSERT(p->addr() == 0xDEAD);
        } else if (i==1) {
            CPPUNIT_ASSERT(p->addr() == 0xfe6b);
        } else {
            CPPUNIT_ASSERT(p->addr() == 0xBEEF);
        }
        // set correct type of packet and receive
        p->set_cmd(Command::READ_RESP);
        ok = pd.receive(0, p);
        CPPUNIT_ASSERT(ok);
        // delete packet
        delete p;
    }

    // test striding autorange - re-init the pd
    auto* stride = pk->mutable_stride();
    pk->mutable_address()->set_increment(10);
    stride->set_increment(64);
    stride->set_range("640");
    pd.init(0, *pk, &pt);

    // test that the range is 640 times the strides (10) times the increment
    CPPUNIT_ASSERT(pd.autoRange(100)==6400);

    // reduce the increment to  and test the autoRange again
    address->set_increment(640);
    pd.init(0, *pk, &pt);
    pd.autoRange(100);

    Packet * p(nullptr);
    // TEMP test packet generation doesn't wrap
    for (uint64_t i=0; i< 100 ; ++i) {
        bool ok = pd.send(p,0);
        // verify that no wrap-around has occurred
        CPPUNIT_ASSERT(p->addr() == i*64);
        delete p;
    }
}

void TestAtp::testAtp_packetTagger(){
    // instatiate packet to be tagged
    Packet* packet = new Packet();

    // setup tagger to perform tagging
    PacketTagger * tagger = new PacketTagger();


    // verify packet metadata blank before tagging
    CPPUNIT_ASSERT(packet->has_flow_id() == false);
    CPPUNIT_ASSERT(packet->has_iommu_id() == false);
    CPPUNIT_ASSERT(packet->has_stream_id() == false);

    // attempt to tag before setting up packet tagger metadata vars
    // -> should cause no effect
    tagger->tagPacket(packet);
    CPPUNIT_ASSERT(packet->has_flow_id() == false);
    CPPUNIT_ASSERT(packet->has_iommu_id() == false);
    CPPUNIT_ASSERT(packet->has_stream_id() == false);

    // configure packet tagger correctly and check with several values
    for (uint64_t i : {0, 1, 2}){
        delete packet;
        packet = new Packet();

        tagger->flow_id = tagger->stream_id = i;
        tagger->iommu_id = static_cast<uint32_t>(i);
        tagger->tagPacket(packet);

        // check if packet is tagged correctly
        CPPUNIT_ASSERT(packet->flow_id() == i);
        CPPUNIT_ASSERT(packet->stream_id() == i);
        CPPUNIT_ASSERT(packet->iommu_id() == static_cast<uint32_t>(i));
    }

    // save last iteration values to check them later
    uint64_t test_flow_id = tagger->flow_id,
             test_stream_id = tagger->stream_id,
             offset = 10;
    uint32_t test_iommu_id = tagger->iommu_id;

    // attempt to retag packet with different values
    tagger->flow_id = tagger->flow_id + offset;
    tagger->iommu_id = tagger->iommu_id + offset;
    tagger->stream_id = tagger->stream_id + offset;
    tagger->tagPacket(packet);

    // Old values should not be retained
    CPPUNIT_ASSERT(packet->flow_id() != test_flow_id);
    CPPUNIT_ASSERT(packet->iommu_id() != test_iommu_id);
    CPPUNIT_ASSERT(packet->stream_id() != test_stream_id);

    // New values should be written to packet
    CPPUNIT_ASSERT(packet->flow_id() == (test_flow_id + offset));
    CPPUNIT_ASSERT(packet->iommu_id() == (test_iommu_id + offset));
    CPPUNIT_ASSERT(packet->stream_id() == (test_stream_id + offset));

    // attempt to tag with invalid values
    tagger->flow_id  = tagger->stream_id = InvalidId<uint64_t>();
    tagger->iommu_id = InvalidId<uint32_t>();

    delete packet;
    packet = new Packet();
    tagger->tagPacket(packet);

    // Invalid values should not be written to packet
    CPPUNIT_ASSERT(packet->has_flow_id() == false);
    CPPUNIT_ASSERT(packet->has_iommu_id() == false);
    CPPUNIT_ASSERT(packet->has_stream_id() == false);

    // cleanup test
    delete tagger;
    delete packet;
}

void TestAtp::testAtp_stats() {
    Stats s1, s2, s3, s4;
    // record sending 1000 bytes every 2 ticks
    for (uint64_t i=0; i <= 10; i+=2) {
        s1.send(i, 1000);
    }
    // data transferred should be 6KB and rate should be
    // 600 B/tick
    CPPUNIT_ASSERT(s1.dataSent == 6000);
    CPPUNIT_ASSERT(s1.sendRate() == 600);
    CPPUNIT_ASSERT(s1.sent == 6);
    CPPUNIT_ASSERT(s1.received == 0);
    CPPUNIT_ASSERT(s1.dataReceived == 0);

    s1.reset();
    // data should now be back to 0
    CPPUNIT_ASSERT(s1.dataSent == 0);

    // test reception
    s1.receive(0, 0, 0.0);
    s1.receive(10, 1000, 0.0);

    CPPUNIT_ASSERT(s1.dataReceived == 1000);
    CPPUNIT_ASSERT(s1.receiveRate() == 100);
    CPPUNIT_ASSERT(s1.sent == 0);
    CPPUNIT_ASSERT(s1.received == 2);

    // tests to make sure add and sum operator are working correctly
    s1.reset();
    for (uint64_t i=0; i <= 10; i+=2) {
        s1.send(i, 1000);
        s2.send(i+12, 1000);
    }
    for (uint64_t i=0; i <= 22; i+=2) {
        s3.send(i, 1000);
    }
    s4 = s1 + s2;
    CPPUNIT_ASSERT(s4.dump() == s3.dump());
    s2 += s1;
    CPPUNIT_ASSERT(s2.dump() == s3.dump());

}

void TestAtp::testAtp_trafficProfile() {
    // configuration object
    Profile config;
    // fill the configuration
    makeProfile(&config, ProfileDescription { "testAtp_trafficProfile",
                                              Profile::READ });

    CPPUNIT_ASSERT(!config.has_pattern());
    CPPUNIT_ASSERT(!config.wait_for_size());
    CPPUNIT_ASSERT(!config.has_fifo());


    // fill the FIFO configuration - pointer,
    // maxLevel, startup level, OT, transactions, rate
    makeFifoConfiguration(config.mutable_fifo(), 1000,
            FifoConfiguration::EMPTY, 1, 1, 10);

    // fill the Packet Descriptor configuration
    PatternConfiguration* pk = makePatternConfiguration(config.mutable_pattern(),
            Command::READ_REQ,
            Command::READ_RESP);

    // configure the packet address and size generation policies
    pk->set_size(64);
    PatternConfiguration::Address* address = pk->mutable_address();
    address->set_base(0);
    address->set_increment(0);

    // register this profile in the TPM
    tpm->configureProfile(config);

    // add a wait on a PLAY event and register as new profile
    config.add_wait_for("testAtp_trafficProfile_to_play ACTIVATION");
    config.set_name("testAtp_trafficProfile_to_play");

    // register this profile in the TPM
    tpm->configureProfile(config);

    // remove the wait for field from the config object
    config.clear_wait_for();
    // delete the packet configuration from the config object
    config.clear_pattern();
    // change name
    config.set_name("testAtp_trafficProfile_checker");
    // set the checker to monitor test_profile
    config.add_check("testAtp_trafficProfile");

    // register this profile in the TPM
    tpm->configureProfile(config);

    // get the ATP Profile Descriptors

    TrafficProfileDescriptor* pd = tpm->profiles.at(0);
    TrafficProfileDescriptor* wait = tpm->profiles.at(1);
    TrafficProfileDescriptor* checker = tpm->profiles.at(2);

    // test packet send and receive
    bool locked = false;
    Packet* p(nullptr),*empty(nullptr);
    uint64_t next=0;

    CPPUNIT_ASSERT(pd->send(locked, p, next));
    CPPUNIT_ASSERT(checker->send(locked, p, next));

    p->set_cmd(Command::READ_RESP);

    // profile should be locked, not active
    CPPUNIT_ASSERT(!pd->active(locked));
    CPPUNIT_ASSERT(locked);

    // profile should not accept a send request at this time
    CPPUNIT_ASSERT(!pd->send(locked, empty, next));

    // receive two partial responses
    p->set_size(32);
    CPPUNIT_ASSERT(pd->receive(next, p, .0));
    CPPUNIT_ASSERT(pd->receive(next, p, .0));
    // update checker
    checker->receive(next, p, .0);

    // profile should now terminate - not active, not locked
    CPPUNIT_ASSERT(!pd->active(locked));
    CPPUNIT_ASSERT(!locked);
    // checker should have terminated
    CPPUNIT_ASSERT(!checker->active(locked));
    CPPUNIT_ASSERT(!locked);

    // reset the checker
    checker->reset();
    // checker is active again
    CPPUNIT_ASSERT(checker->active(locked));

    // test locked profile
    CPPUNIT_ASSERT(!wait->send(locked, p, next));
    CPPUNIT_ASSERT(locked);
    // issue PLAY event
    CPPUNIT_ASSERT(wait->receiveEvent(Event(Event::ACTIVATION,
            Event::TRIGGERED,wait->getId(),0)));
    // test packet is now sent
    CPPUNIT_ASSERT(wait->send(locked, p, next));
    CPPUNIT_ASSERT(!locked);
}

void TestAtp::testAtp_packetTaggerCreation(){
    // setup test metadata fields
    const uint64_t test_iommu_id = 0, test_flow_id = 1, test_stream_id = 2;

    // packet tagger should be created when profle_desc constructor is called
    // and flow_id and/or iommu_id are set
    ProfileDescription profDesc = ProfileDescription{
                                    "testAtp_tagger_creation_1", Profile::READ,
                                    nullptr, nullptr};
    profDesc.iommu_id = test_iommu_id;
    profDesc.flow_id = test_flow_id;

    Profile config;
    makeProfile(&config, profDesc);

    // configure required ProfileDesc properties for it to be a Master and
    // register it with the TPM
    makeFifoConfiguration(config.mutable_fifo(), 1000,
        FifoConfiguration::EMPTY, 1, 1, 10);

    PatternConfiguration* pk = makePatternConfiguration(
        config.mutable_pattern(),
        Command::READ_REQ, Command::READ_RESP
    );

    pk->set_size(64);
    PatternConfiguration::Address* address = pk->mutable_address();
    address->set_base(0);
    address->set_increment(0);
    tpm->configureProfile(config);

    // profile streamId should be invalid; still to be been added to stream
    TrafficProfileDescriptor* profile = tpm->profiles.at(0);
    CPPUNIT_ASSERT(!isValid(profile->_streamId));

    // extract packet tagger and check metadata values
    PacketTagger* taggerPop = profile->packetTagger;
    Packet* packet = new Packet();
    taggerPop->tagPacket(packet);

    CPPUNIT_ASSERT(packet->has_stream_id() == false);
    CPPUNIT_ASSERT(packet->iommu_id() == test_iommu_id);
    CPPUNIT_ASSERT(packet->flow_id() == test_flow_id);
    delete packet;

    // packet tagger should not be recreated if profile_desc has existing
    // tagger and addToStream is called after; streamId should be added to
    // existing tagger
    profile->addToStream(test_stream_id);
    // check whether streamId is correctly set
    CPPUNIT_ASSERT(isValid(profile->_streamId));
    CPPUNIT_ASSERT(profile->_streamId == test_stream_id);

    // packet tagger should have been updated to reflect new value for streamId
    packet = new Packet();
    taggerPop->tagPacket(packet);

    // old metadata values should be intact
    CPPUNIT_ASSERT(packet->stream_id() == test_stream_id);
    CPPUNIT_ASSERT(packet->iommu_id() == test_iommu_id);
    CPPUNIT_ASSERT(packet->flow_id() == test_flow_id);
    delete packet;
}

void
TestAtp::testAtp_tpm() {

    const string profile_0 = "testAtp_tpm_profile_0";
    const string profile_1 = "testAtp_tpm_profile_1";

    const unordered_set<string> profiles = {profile_0, profile_1};

    // profile configuration object
    Profile config_0, config_1;
    // fill the configuration
    makeProfile(&config_0, ProfileDescription { profile_0, Profile::READ });

    // fill the FIFO configuration - pointer, maxLevel,
    // startup level, OT, transactions, rate
    makeFifoConfiguration(config_0.mutable_fifo(), 1000,
            FifoConfiguration::EMPTY, 1, 4, 10);

    // fill the Packet Descriptor configuration
    PatternConfiguration* pk =
            makePatternConfiguration(config_0.mutable_pattern(),
                    Command::READ_REQ,
                    Command::READ_RESP);

    // configure the packet address and size generation policies
    pk->set_size(32);
    PatternConfiguration::Address* address = pk->mutable_address();
    address->set_base(0);
    address->set_increment(64);
    PatternConfiguration::Stride* stride = pk->mutable_stride();
    stride->set_increment(1);
    stride->set_range("3B");
    // register 1st profile
    tpm->configureProfile(config_0);

    // change fields for 2nd profile
    // 2nd profile waits on 1st to complete
    config_1 = config_0;

    config_1.set_name(profile_1);
    config_1.set_master_id(profile_1);
    config_1.add_wait_for(profile_0);

    // register 2nd profile
    tpm->configureProfile(config_1);

    // checks to see if list of masters was set correctly
    CPPUNIT_ASSERT(tpm->getMasters() == profiles);

    // checks to see if the number of slaves is 0
    CPPUNIT_ASSERT(tpm->getMasterSlaves().empty());

    // request packets to TPM
    bool locked = false;
    uint64_t next = 0, time = 0;

    // every time a packet is generated, send a response
    // to terminate test_profile and unlock test_profile2
    // reuse request as response, so that it gets deallocated by ATP
    // each profile sends 3 packets , then terminates


    for (uint64_t i = 0; i < 4 ; ++i) {

        auto packets = tpm->send(locked, next, time);
        // only one masters sent packets
        CPPUNIT_ASSERT(packets.size() == 1);
        // test_profile sent a packet
        CPPUNIT_ASSERT(packets.find(profile_0) != packets.end());
        // one master is locked, so flag on
        CPPUNIT_ASSERT(locked);

        Packet* p = (packets.find(profile_0)->second);
        CPPUNIT_ASSERT(p->addr()==(i<3?i:64));
        p->set_cmd(Command::READ_RESP);
        tpm->receive(0, p);
    }

    for (uint64_t i = 0; i < 4 ; ++i) {

        // "test_profile2" is now unlocked
        auto packets = tpm->send(locked, next, time);
        // test_profile2 sent a packet
        CPPUNIT_ASSERT(packets.find(profile_1) != packets.end());
        // test_profile2 is locked, waiting for response
        CPPUNIT_ASSERT(locked);
        // TPM is waiting for responses
        CPPUNIT_ASSERT(tpm->waiting());

        // send the response
        Packet* p = (packets.find(profile_1)->second);
        CPPUNIT_ASSERT(p->addr()==(i<3?i:64));
        p->set_cmd(Command::READ_RESP);
        tpm->receive(0, p);
    }

    CPPUNIT_ASSERT(!tpm->waiting());

    // no packets left to send
    auto packets = tpm->send(locked, next, time);
    CPPUNIT_ASSERT(packets.empty());
    // check that the stream composed by profile 0 and 1 is completed
    const uint64_t pId = tpm->profileId("testAtp_tpm_profile_0");
    CPPUNIT_ASSERT(tpm->streamTerminated(pId));
    // reset the stream from profile 0
    tpm->streamReset(pId);
    CPPUNIT_ASSERT(!tpm->streamTerminated(pId));
    // play profile 0 again.
    for (uint64_t i = 0; i < 4 ; ++i) {
        auto packets = tpm->send(locked, next, time);
        // only one masters sent packets
        CPPUNIT_ASSERT(packets.size() == 1);
        // test_profile sent a packet
        CPPUNIT_ASSERT(packets.find(profile_0) != packets.end());
        // one master is locked, so flag on
        CPPUNIT_ASSERT(locked);
        Packet* p = (packets.find(profile_0)->second);
        CPPUNIT_ASSERT(p->addr()==(i<3?i:64));
        p->set_cmd(Command::READ_RESP);
        tpm->receive(0, p);
    }

    for (uint64_t i = 0; i < 4 ; ++i) {
        // "test_profile2" is now unlocked
        auto packets = tpm->send(locked, next, time);
        // test_profile2 sent a packet
        CPPUNIT_ASSERT(packets.find(profile_1) != packets.end());
        // test_profile2 is locked, waiting for response
        CPPUNIT_ASSERT(locked);
        // TPM is waiting for responses
        CPPUNIT_ASSERT(tpm->waiting());
        // send the response
        Packet* p = (packets.find(profile_1)->second);
        CPPUNIT_ASSERT(p->addr()==(i<3?i:64));
        p->set_cmd(Command::READ_RESP);
        tpm->receive(0, p);
    }
    CPPUNIT_ASSERT(tpm->streamTerminated(pId));

    // reset the TPM - this causes profiles to be reloaded
    tpm->reset();
    // register 1st profile
    tpm->configureProfile(config_0);
    // register 2nd profile
    tpm->configureProfile(config_1);
    // define a new profile, which also waits on profile 0
    const string profile_2 = "testAtp_tpm_profile_2";
    Profile config_2(config_1);
    config_2.set_name(profile_2);
    config_2.set_master_id(profile_2);
    // change type to WRITE
    config_2.set_type(Profile::WRITE);
    // register 3nd profile
    tpm->configureProfile(config_2);

    // define a new profile, which waits on profile 1 and 2
    const string profile_3 = "testAtp_tpm_profile_3";
    Profile config_3(config_0);
    config_3.set_name(profile_3);
    config_3.set_master_id(profile_3);
    config_3.add_wait_for(profile_1);
    config_3.add_wait_for(profile_2);
    // register 3nd profile
    tpm->configureProfile(config_3);

    /*
     * profile 1 and profile 2 wait on profile 0 - all READ profiles,
     * whilst profile 3 is of type WRITE and waits on 1 and 2.
     * They all are configured with 4 transactions of 64 bytes each.
     * They form a diamond-shaped stream.
     * Each of them streams 3 packets with a stride of 1 byte distance,
     * then jumps with an increment of 64 to issue the fourth.
     * We therefore reconfigure the stream to constrain it to
     * 64 (first increment) + 1 (left over packet) = 65 bytes per profile
     * with a starting address base of 0x00FF
     */
    const uint64_t rootId = tpm->profileId("testAtp_tpm_profile_0");
    const uint64_t newRange =
            tpm->addressStreamReconfigure(rootId, 0x00FF, 388);
    // verify that the new range has been applied
    CPPUNIT_ASSERT(newRange == 260);
    // send all packets
    for (uint64_t i = 0; i < 16 ; ++i) {
        auto packets = tpm->send(locked, next, time);
        if (!locked) time = next;
        for (auto p: packets) {
            p.second->set_cmd(Command::READ_RESP);
            tpm->receive(time, p.second);
        }

    }

    /* uniqueStream */
    config_0.add_wait_for(profile_0 + " ACTIVATION");
    auto reset = [this, &config_0, &config_1]() {
        tpm->reset();
        tpm->configureProfile(config_0); tpm->configureProfile(config_1);
    };
    /* 1. First usage should return Root Profile ID of Original */
    reset(); tpm->streamCacheUpdate();
    uint64_t orig_id { tpm->profileId(profile_0) };
    TrafficProfileDescriptor *orig { tpm->getProfile(orig_id) };
    uint64_t clone0_id { tpm->uniqueStream(orig_id) };
    TrafficProfileDescriptor *clone0 { tpm->getProfile(clone0_id) };
    CPPUNIT_ASSERT(tpm->getStreamCache().size() == 1);
    CPPUNIT_ASSERT(tpm->getProfileMap().size() == 2);
    CPPUNIT_ASSERT(orig_id == clone0_id); CPPUNIT_ASSERT(orig == clone0);
    /* 2. Non-first usage should create Clone and return its Root Profile ID
          Clone name should be different from Original */
    clone0_id = tpm->uniqueStream(orig_id);
    clone0 = tpm->getProfile(clone0_id);
    CPPUNIT_ASSERT(tpm->getStreamCache().size() == 2);
    CPPUNIT_ASSERT(tpm->getProfileMap().size() == 4);
    CPPUNIT_ASSERT(orig_id != clone0_id); CPPUNIT_ASSERT(orig != clone0);
    uint64_t clone1_id = tpm->uniqueStream(orig_id);
    TrafficProfileDescriptor *clone1 { tpm->getProfile(clone1_id) };
    CPPUNIT_ASSERT(tpm->getStreamCache().size() == 3);
    CPPUNIT_ASSERT(tpm->getProfileMap().size() == 6);
    CPPUNIT_ASSERT(orig_id != clone1_id); CPPUNIT_ASSERT(orig != clone1);
    CPPUNIT_ASSERT(clone0_id != clone1_id); CPPUNIT_ASSERT(clone0 != clone1);
    /* 3. Clone state should be independent */
    auto diff_conf = [this](const uint64_t id0, const uint64_t id1) {
        tpm->addressStreamReconfigure(id0, 0x11, 0x123, Profile::READ);
        tpm->addressStreamReconfigure(id1, 0xFF, 0x321, Profile::READ);
    };
    /* 3.1 Original activated, one Packet per send, Original terminated,
           Clones not terminated */
    diff_conf(orig_id, clone0_id); orig->activate();
    locked = false ; next = time = 0;
    for (uint64_t txn { 0 }; txn < config_0.fifo().total_txn(); ++txn) {
        auto packets = tpm->send(locked, next, time);
        CPPUNIT_ASSERT(packets.size() == 1);
        for (auto &packet : packets) {
            Packet *p { packet.second }; p->set_cmd(Command::READ_RESP);
            tpm->receive(0, p);
        }
    }
    // Handle remaining transactions
    for (uint64_t txn { 0 }; txn < config_1.fifo().total_txn(); ++txn) {
        for (auto &packet : tpm->send(locked, next, time)) {
            Packet *p { packet.second }; p->set_cmd(Command::READ_RESP);
            tpm->receive(0, p);
        }
    }
    CPPUNIT_ASSERT(tpm->streamTerminated(orig_id));
    CPPUNIT_ASSERT(!tpm->streamTerminated(clone0_id));
    CPPUNIT_ASSERT(!tpm->streamTerminated(clone1_id));
    // 3.2 Original and Clone activated, two Packets per Send, different
    //     addresses as configured, reset of Original does not reset Clone
    reset(); tpm->streamCacheUpdate(); orig_id = tpm->profileId(profile_0);
    tpm->uniqueStream(orig_id); clone0_id = tpm->uniqueStream(orig_id);
    orig = tpm->getProfile(orig_id); clone0 = tpm->getProfile(clone0_id);
    diff_conf(orig_id, clone0_id); orig->activate(); clone0->activate();
    locked = false ; next = time = 0;
    for (uint64_t txn { 0 }; txn < config_0.fifo().total_txn(); ++txn) {
        auto packets = tpm->send(locked, next, time);
        CPPUNIT_ASSERT(packets.size() == 2);
        Packet *p0 { packets.begin()->second },
               *p1 { (++packets.begin())->second };
        CPPUNIT_ASSERT(p0->addr() != p1->addr());
        if (txn < 3) {
            CPPUNIT_ASSERT(
                (p0->addr() == 0x11 + txn && p1->addr() == 0xFF + txn) ||
                (p0->addr() == 0xFF + txn && p1->addr() == 0x11 + txn)
            );
        }
        for (auto &packet : packets) {
            Packet *p { packet.second }; p->set_cmd(Command::READ_RESP);
            tpm->receive(time, p);
        }
    }
    // Handle remaining transactions
    for (uint64_t txn { 0 }; txn < config_1.fifo().total_txn(); ++txn) {
        auto packets = tpm->send(locked, next, time);
        CPPUNIT_ASSERT(packets.size() == 2);
        for (auto &packet : packets) {
            Packet *p { packet.second }; p->set_cmd(Command::READ_RESP);
            tpm->receive(0, p);
        }
    }
    CPPUNIT_ASSERT(tpm->streamTerminated(orig_id));
    CPPUNIT_ASSERT(tpm->streamTerminated(clone0_id));
    tpm->streamReset(orig_id);
    CPPUNIT_ASSERT(!tpm->streamTerminated(orig_id));
    CPPUNIT_ASSERT(tpm->streamTerminated(clone0_id));
    /* 4. Diamond-shaped Stream Clones */
    reset(); tpm->configureProfile(config_2); tpm->configureProfile(config_3);
    tpm->streamCacheUpdate(); orig_id = tpm->profileId(profile_0);
    tpm->uniqueStream(orig_id); clone0_id = tpm->uniqueStream(orig_id);
    CPPUNIT_ASSERT(tpm->getStreamCache().size() == 2);
    CPPUNIT_ASSERT(tpm->getProfileMap().size() == 8);
    orig = tpm->getProfile(orig_id); clone0 = tpm->getProfile(clone0_id);
    orig->activate(); clone0->activate();
    locked = false ; next = time = 0;
    for (uint64_t txn { 0 }; txn < 16; ++txn) {
        auto packets = tpm->send(locked, next, time);
        for (auto &packet : packets) {
            Packet *p { packet.second }; p->set_cmd(Command::READ_RESP);
            tpm->receive(time, p);
        }
        if (next) time = next;
    }
    CPPUNIT_ASSERT(tpm->streamTerminated(orig_id));
    CPPUNIT_ASSERT(tpm->streamTerminated(clone0_id));
    tpm->streamReset(clone0_id);
    CPPUNIT_ASSERT(tpm->streamTerminated(orig_id));
    CPPUNIT_ASSERT(!tpm->streamTerminated(clone0_id));
    /* 5. Stream Clones in different Masters */
    reset(); orig_id = tpm->profileId(profile_0);
    clone0_id = tpm->uniqueStream(orig_id, tpm->masterId(profile_1));
    CPPUNIT_ASSERT(tpm->getStreamCache().size() == 2);
    CPPUNIT_ASSERT(tpm->getProfileMap().size() == 4);
    clone0 = tpm->getProfile(clone0_id); clone0->activate();
    locked = false ; next = time = 0;
    for (uint64_t txn { 0 }; txn < config_0.fifo().total_txn(); ++txn) {
        for (auto &packet : tpm->send(locked, next, time)) {
            Packet *p { packet.second };
            CPPUNIT_ASSERT(p->master_id() == profile_1);
            p->set_cmd(Command::READ_RESP); tpm->receive(0, p);
        }
    }
}

void TestAtp::testAtp_trafficProfileDelay() {

    // profile configuration object
    Profile config;
    // fill the configuration
    makeProfile(&config, ProfileDescription {
        "testAtp_trafficProfileDelay_pause", Profile::READ });
    // fill in the delay field
    DelayConfiguration* delay = config.mutable_delay();
    delay->set_time("2s");
    // register delay profile
    tpm->configureProfile(config);
    // delay will return next == 2s in ATP time units
    bool locked = false;
    uint64_t next = 0, time = 0;
    auto packets = tpm->send(locked, next, time);
    // we should now find that delay is active
    CPPUNIT_ASSERT(!tpm->isTerminated("testAtp_trafficProfileDelay_pause"));
    // no transmission
    CPPUNIT_ASSERT(packets.size() == 0);
    // next is 2s in picoseconds (default ATP time resolution)
    CPPUNIT_ASSERT(next==2*(1e+12));
    // we should now find that delay is terminated
    time = next;
    packets = tpm->send(locked, next, time);
    CPPUNIT_ASSERT(packets.size() == 0);
    CPPUNIT_ASSERT(tpm->isTerminated("testAtp_trafficProfileDelay_pause"));

    // register another profile
    delay->set_time("3.7ns");
    config.set_name("testAtp_trafficProfileDelay_pause_2");
    config.set_master_id("testAtp_trafficProfileDelay_pause_2");
    // register delay profile 2
    tpm->configureProfile(config);
    // call send
    packets = tpm->send(locked, next, time);
    // we should now find that delay is active
    CPPUNIT_ASSERT(!tpm->isTerminated("testAtp_trafficProfileDelay_pause_2"));
    // no transmission
    CPPUNIT_ASSERT(packets.size() == 0);
    // next is 3.7ns + 2s in picoseconds (default ATP time resolution)
    CPPUNIT_ASSERT(next==(2*(1e+12) + 3700));
    // we should now find that delay is terminated
    time = next;
    packets = tpm->send(locked, next, time);
    CPPUNIT_ASSERT(packets.size() == 0);
    CPPUNIT_ASSERT(tpm->isTerminated("testAtp_trafficProfileDelay_pause_2"));
    // reset the profile
    const uint64_t delayId = tpm->profileId("testAtp_trafficProfileDelay_pause_2");
    tpm->getProfile(delayId)->reset();
    // the profile should now be active again and have a next time equal to its delay
    packets = tpm->send(locked, next, time);
    // we should now find that delay is active
    CPPUNIT_ASSERT(!tpm->isTerminated("testAtp_trafficProfileDelay_pause_2"));
    // no transmission
    CPPUNIT_ASSERT(packets.size() == 0);
    // next is NOW (time) + 3.7ns in picoseconds (default ATP time resolution)
    CPPUNIT_ASSERT(next==time+3700);
    // allow for the delay to expire
    time = next;
    packets = tpm->send(locked, next, time);

    // register another profile - white-spaces in delay specifier
    delay->set_time("0.191       us");
    config.set_name("testAtp_trafficProfileDelay_pause_3");
    config.set_master_id("testAtp_trafficProfileDelay_pause_3");
    // register delay profile 2
    tpm->configureProfile(config);
    // call send
    packets = tpm->send(locked, next, time);
    // we should now find that delay is active
    CPPUNIT_ASSERT(!tpm->isTerminated("testAtp_trafficProfileDelay_pause_3"));
    // no transmission
    CPPUNIT_ASSERT(packets.size() == 0);
    // next is 3.7ns + 2s + 0.191 us in picoseconds (default ATP time resolution)
    CPPUNIT_ASSERT(next==(2*(1e+12) + 2*3700 + 191000));
    // we should now find that delay is terminated
    time = next;
    packets = tpm->send(locked, next, time);
    CPPUNIT_ASSERT(packets.size() == 0);
    CPPUNIT_ASSERT(tpm->isTerminated("testAtp_trafficProfileDelay_pause_3"));
}

void TestAtp::testAtp_unitConversion() {

    pair<uint64_t, uint64_t> result(0, 0);

    // tests converting string to lowercase
    CPPUNIT_ASSERT(Utilities::toLower("aBcDEFg")=="abcdefg");

    // test reducing a fraction
    auto h = Utilities::reduce<uint64_t>;
    result = h(30,6);
    CPPUNIT_ASSERT(result.first==5);
    CPPUNIT_ASSERT(result.second==1);

    // test number in string detection
    CPPUNIT_ASSERT(Utilities::isNumber("10"));
    CPPUNIT_ASSERT(!Utilities::isNumber("a"));

    // test trimming of string
    CPPUNIT_ASSERT(Utilities::trim(" t e s t ")=="test");

    // test floating point string conversion to pair number, scale
    auto g = Utilities::toUnsignedWithScale<uint64_t>;
    result = g("1.34");
    CPPUNIT_ASSERT(result.first==134);
    CPPUNIT_ASSERT(result.second==100);

    // test byte conversion from string
    CPPUNIT_ASSERT(Utilities::toBytes<int>("512 Bytes")==512);

    // test power of two rate detection
    auto f = Utilities::toRate<uint64_t>;
    result = f("3    TiB@s");
    CPPUNIT_ASSERT(result.first==3);
    CPPUNIT_ASSERT(result.second==1099511627776);
    // test power of two rate, bits detection
    result = f( "    5 Kibit/s");
    CPPUNIT_ASSERT(result.first==5);
    CPPUNIT_ASSERT(result.second==128);
    // test rate in Gbytes detection
    result = f( "    12.3 GBps");
    // test rate in bits detection
    CPPUNIT_ASSERT(result.first==123);
    CPPUNIT_ASSERT(result.second==1e8);
    // test rate in bytes detection
    result = f( "    4.23 Bpus");
    CPPUNIT_ASSERT(result.first==423);
    CPPUNIT_ASSERT(result.second==1e4);
    // test separator detection
    result = f(  "44  Bpps");
    CPPUNIT_ASSERT(result.first==44);
    CPPUNIT_ASSERT(result.second==1e12);

    result = f("16 Tbit ps");
    CPPUNIT_ASSERT(result.first==16);
    CPPUNIT_ASSERT(result.second==(1e12)/8);

    // tests hex conversion from int
    CPPUNIT_ASSERT(Utilities::toHex(167489)=="0x28e41");

    // tests next power of two calculation
    CPPUNIT_ASSERT(Utilities::nextPowerTwo(27004)==16384);

}

void TestAtp::testAtp_kronos() {
    // set Kronos bucket width and calendar size
    tpm->setKronosConfiguration("3ps","15ps");
    // create a Kronos system
    Kronos k (tpm);
    // initialize Kronos and checks for no schedules events
    CPPUNIT_ASSERT(!k.isInitialized());
    k.init();
    CPPUNIT_ASSERT(k.isInitialized());
    CPPUNIT_ASSERT(!k.next());

    // generate events and register to Kronos
    for (uint64_t i=0; i < 30; i+=3) {
        Event ev(Event::TICK,Event::TRIGGERED,i,i);
        k.schedule(ev);
        CPPUNIT_ASSERT(k.getCounter()==(i+3)/3);
    }

    // get events while advancing TPM time
    for (uint64_t i=3; i < 30; i+=6) {
        tpm->setTime(i);
        list<Event> q;
        k.get(q);
        CPPUNIT_ASSERT(!q.empty() && (q.size()==2));
        for (auto& ev:q){
            CPPUNIT_ASSERT(ev.action==Event::TRIGGERED);
            CPPUNIT_ASSERT((ev.time==i) || (ev.time==(i-3)));
            CPPUNIT_ASSERT((ev.id==i) || (ev.id==(i-3)));
        }
        CPPUNIT_ASSERT((k.getCounter()==0) || k.next()==(i+3));
    }

    // should be no scheduled events
    CPPUNIT_ASSERT(!k.next());

}


void TestAtp::testAtp_trafficProfileSlave() {
    // profile configuration object
    Profile config;
    bool locked = false;

    // fill the configuration for testing the buffer
    makeProfile(&config, ProfileDescription {
        "testAtp_testAtp_trafficProfileSlave", Profile::READ });
    // fill in the slave field
    SlaveConfiguration* slave_cfg = config.mutable_slave();
    slave_cfg->set_latency("80ns");
    slave_cfg->set_rate("32GBps");
    slave_cfg->set_granularity(16);
    slave_cfg->set_ot_limit(6);
    // register slave profile
    tpm->configureProfile(config);
    // get pointer to slave
    auto slave = tpm->getProfile(0);
    Packet* req = nullptr;
    uint64_t next = 0;
    bool ok = false;

    /*
     * Here the first 2 packets will be sent, but no further.
     * This is because of the size 33 and granularity 16. So
     * each packet will actually take up 3 slots. So after 2
     * packets have been sent, no more packets can be be in
     * transit as the buffer is full. So here we will test
     * to make sure this happens.
     */
    for (uint64_t i=0; i<slave_cfg->ot_limit(); i++) {
        req = new Packet;
        req->set_cmd(READ_REQ);
        req->set_addr(0);
        req->set_size(33);
        ok = slave->receive(next,req,0);
        CPPUNIT_ASSERT(i>1?!ok:ok);
    }

    // fill the configuration for testing the OT limit
    makeProfile(&config, ProfileDescription {
        "testAtp_testAtp_trafficProfileSlave_2", Profile::READ });
    // fill in the slave field
    SlaveConfiguration* slave_cfg_2 = config.mutable_slave();
    slave_cfg_2->set_latency("80ns");
    slave_cfg_2->set_rate("32GBps");
    slave_cfg_2->set_granularity(16);
    slave_cfg_2->set_ot_limit(6);
    // register slave profile
    tpm->configureProfile(config);
    // get pointer to slave
    auto slave_2 = tpm->getProfile(1);
    req = nullptr;
    next = 0;

    /*
     * Here we see that the size of the packets fit
     * perfectly into the buffer, so 6 packets can be
     * in transit. We will test to make sure  that once
     * the limit has been reached that the slave then
     * becomes locked
     * attempt another send after reset,
     * which should succeed
     * only one response should then be available for transmission
     */
    for (uint64_t i=0; i<slave_cfg_2->ot_limit()+2; i++) {
        req = new Packet;
        req->set_cmd(READ_REQ);
        req->set_addr(0);
        req->set_size(16);

        if (i>slave_cfg_2->ot_limit()) {
            // resets the slave for the last request
            slave_2->reset();
        }

        bool ok = slave_2->receive(next,req,0);

        if (i<slave_cfg_2->ot_limit()) {
            // requests within the OT limit are accepted
            CPPUNIT_ASSERT(ok);
        } else if (i==slave_cfg_2->ot_limit()) {
            // request is rejected due to OT limit reached
            CPPUNIT_ASSERT(!ok);
        } else if (i>slave_cfg_2->ot_limit()) {
            // last request is accepted after reset
            CPPUNIT_ASSERT(ok);
        }
    }

    // get responses - advance TPM time to memory latency
    tpm->setTime(80000);
    locked = false;
    Packet* res = nullptr;
    next = 0;
    for (uint64_t i=0; i<slave_cfg_2->ot_limit(); i++) {
        ok = slave_2->send(locked,res,next);
        if (ok) {
            delete res;
        }
        // only one response is available (one request after reset)
        CPPUNIT_ASSERT(i==0||!ok);
    }
}


void TestAtp::testAtp_trafficProfileManagerRouting() {
    // number of master/slave pairs to test
    const uint8_t nPairs = 10;
    // fill the configurations
    const string master = "testAtp_trafficProfileManagerRouting_master";
    const string slave = "testAtp_trafficProfileManagerRouting_slave";

    // profile configuration objects
    Profile masters[nPairs];
    Profile slaves[nPairs];

    std::set<pair<string,string>> slaveToMasterMap;

    for (uint8_t i = 0; i < nPairs; ++i) {
        const string num = to_string(i);
        string mName = master + num;
        string sName = slave + num;
        slaveToMasterMap.insert(pair <string,string> (mName,sName));
        makeProfile(&masters[i], ProfileDescription {
            mName, Profile::READ, &mName });
        makeProfile(&slaves[i], ProfileDescription {
            sName, Profile::READ, &sName });

        // fill in the master field
        // fill the FIFO configuration - pointer, maxLevel,
        // startup level, OT, transactions, rate
        makeFifoConfiguration(masters[i].mutable_fifo(), 1000,
                FifoConfiguration::EMPTY, 2, 10, 2);

        // fill the Packet Descriptor configuration
        PatternConfiguration* pk =
                makePatternConfiguration(masters[i].mutable_pattern(),
                        Command::READ_REQ,
                        Command::READ_RESP);

        // configure the packet address and size generation policies
        pk->set_size(64);

        PatternConfiguration::Address* address = pk->mutable_address();
        address->set_base(0);
        address->set_increment(64);

        // fill in the slave field
        SlaveConfiguration* slave_cfg = slaves[i].mutable_slave();
        slave_cfg->set_latency("80ns");
        slave_cfg->set_rate("32GBps");
        slave_cfg->set_granularity(16);
        slave_cfg->set_ot_limit(6);
        slave_cfg->add_master(mName);

        tpm->configureProfile(masters[i]);
        tpm->configureProfile(slaves[i]);

    }

    // Checks to make sure each master and slave pair were added correctly, and
    // then removes it.
    for (const auto& pair: tpm->getMasterSlaves()) {
        CPPUNIT_ASSERT(slaveToMasterMap.find(pair) != slaveToMasterMap.end());
        slaveToMasterMap.erase(pair);
    }

    // Checks to make sure that the mapping is now empty
    CPPUNIT_ASSERT(slaveToMasterMap.empty());

    // start TPM internal routing
    tpm->loop();
}

CppUnit::TestSuite* TestAtp::suite() {
    CppUnit::TestSuite* suiteOfTests = new CppUnit::TestSuite("TestAtp");

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 0 - Tests the ATP FIFO",
            &TestAtp::testAtp_fifo ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 1 - Tests the ATP Event",
            &TestAtp::testAtp_event ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 2 - Tests the ATP Packet Descriptor",
            &TestAtp::testAtp_packetDesc ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 3 - Tests the ATP Packet Tagger for tagging metadata fields",
            &TestAtp::testAtp_packetTagger ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 4 - Tests the ATP Stats object",
            &TestAtp::testAtp_stats ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 5 - Tests the ATP Traffic Profile",
            &TestAtp::testAtp_trafficProfile ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 6 - Tests creation logic for ATP Packet Tagger",
            &TestAtp::testAtp_packetTaggerCreation ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 7 - Tests the ATP Traffic Profile Manager",
            &TestAtp::testAtp_tpm ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 8 - Tests the ATP Traffic Profile Delay",
            &TestAtp::testAtp_trafficProfileDelay ));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 9 - Tests the ATP Unit Conversion Utilities",
            &TestAtp::testAtp_unitConversion));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 10 - Tests the Kronos engine",
            &TestAtp::testAtp_kronos));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 11 - Tests the ATP Traffic Profile Slave",
            &TestAtp::testAtp_trafficProfileSlave));

    suiteOfTests->addTest(new CppUnit::TestCaller<TestAtp>(
            "Test 12 - Tests the ATP Traffic Profile Manager routing",
            &TestAtp::testAtp_trafficProfileManagerRouting));

    return suiteOfTests;
}


} // end of namespace
