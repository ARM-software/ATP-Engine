/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Sep 25, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_PROFILE_GEN_HH__
#define __AMBA_PROFILE_GEN_HH__

// std includes
#include <map>
#include <memory>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// GEM5 includes
#include "base/statistics.hh"
#include "base/types.hh"
#include "mem/request.hh"
#include "mem/qport.hh"
#include "params/ProfileGen.hh"
#include "sim/core.hh"
#include "sim/sim_object.hh"

// ATP includes
#include "proto/tp_config.pb.h"
#include "traffic_profile_manager.hh"

/*!
 *\brief AMBA Traffic Profile Generator
 *
 * A synthetic traffic generator which processes traffic profile specifications
 * adherent to AMBA Traffic Profile specifications (Bruce Mattewson - ARM-ECM-0313315) and AMBA
 * Traffic Profile implementation design (Matteo Andreozzi - n.TBD)
 */

class ProfileGen : public SimObject {

protected:

    /*!
     * Schedules event for next update and executes an update on the
     * state graph, either performing a state transition or executing
     * the current state, depending on the current time.
     */
    void update();

    /*!
     * Implements a safety watchdog which detects unexpected
     * inactivity periods and dumps statistics before killing
     * the simulation
     */
    void watchDog();

    /*!
     * Receive a retry from the neighboring port and attempt to
     * re-send the waiting packet.
     *\param idx ATP Master port ID
     */
    void recvReqRetry(const PortID);

    /*!
     * Outputs a packet trace to ostream,
     * given a memory packet pointer
     *\param pkt pointer to memory packet
     */
    void tracePacket(const Packet*);

    /*!
     * Outputs a GEM5 packet trace to ostream
     * for M3I tracing, given a memory packet
     * pointer
     *\param pkt pointer to GEM5 packet
     */
    void traceM3iPacket(const Packet*);

    /*!
     * Computes a time value to be passed to ATP Engine, given the
     * current tick and the configured timeUnit
     *\return a time value to be passed to ATP calls
     */
    uint64_t getAtpTime() const;

    /*!
    * Computes a number of ticks from a time value
    * received from ATP Engine, given the
    * configured timeUnit
    *\param atpTime the ATP time value to be converted
    *\return a time value to be passed to ATP calls
    */
    uint64_t getSimTicks(const uint64_t) const;

    /*!
     * Records ATP statistics
     */
    void recordAtpStats();

    /*!
     * ATP Traffic Profile Manager
     */
    TrafficProfiles::TrafficProfileManager tpm;

    /*!
     * Pointer to the System object
     * in.
     */
    System* system;

    /*!
     * The Google Protocol Buffer configuration file to parse.
     */
    const std::vector<std::string> configFiles;

    //! Buffered next ATP time
    uint64_t nextAtpTime{ MaxTick };

    //! Time to send next packets
    Tick nextPacketTick{ 0 };

    //! Flag to signal at least one profile is locked
    bool locked{ false };

    //! Master port specialization for the traffic generator
    class ProfileGenPort : public MasterPort
    {
      public:
        /*!
         * Constructor
         *\param name port name
         *\param gen reference to ProfileGen parent object
         *\param idx the port id in an array of ports
         */
        ProfileGenPort(const std::string& name, ProfileGen& gen, const PortID idx)
            : MasterPort(name, &gen, idx), profileGen(gen)
        { }

      protected:
        //! receives a request retry callback from a stalled port
        inline void recvReqRetry() { profileGen.recvReqRetry(id); }

        /*!
         * Receives a response packet
         *\param pkt pointer to the response packet
         */
        bool recvTimingResp(PacketPtr pkt);
        /*!
         * Receives a snoop request packet
         *\param pkt pointer to the response packet
         */
        void recvTimingSnoopReq(PacketPtr pkt) { }

        /*!
         * Receives a functional snoop request packet
         *\param pkt pointer to the response packet
         */
        void recvFunctionalSnoop(PacketPtr pkt) { }

        /*!
         * Receives an atomic snoop request
         *\param pkt pointer to the response packet
         *\return always returns zero
         */
        Tick recvAtomicSnoop(PacketPtr pkt) { return 0; }

      private:

        ProfileGen& profileGen;

    };

    //! One port per configured ATP Master
    std::vector<std::unique_ptr<ProfileGenPort>> port;

    //! Master to port mapping
    std::map<RequestorID, PortID> interface;

    //! Packets waiting to be sent, organised per master
    std::multimap<std::string, TrafficProfiles::Packet*> localBuffer;

    //! Pointer to packet stalled on the ProfileGenPorts
    std::map<PortID, Packet*> retryPkt;

    //! Tick when the stalled packet was meant to be sent
    std::map<PortID, Tick> retryPktTick;

    /*!\brief UID-based routing support (optional)
     * If UID-based routing is enabled, ATP Engine will route
     * responses faster by exploiting the GID information
     * maintained by this adaptor
     *
     * the table is built as RequestorID, Address, Command, Request UID
     */
    std::map<RequestorID, std::multimap<uint64_t,
        std::multimap<TrafficProfiles::Command, uint64_t>>> routingTable;


    //! Event for scheduling updates
    EventWrapper<ProfileGen, &ProfileGen::update> updateEvent;

    //! Watchdog event
    EventWrapper<ProfileGen, &ProfileGen::watchDog> watchdogEvent;

    /*!
     * Watchdog timer - if expires, dumps diagnostic debug info and kills ATP
     */
    Tick watchdogEventTimer{ 0 };


    // per-ATP master statistics

    //! Count the number of retries
    Stats::Vector numRetries;

    //! Count the time incurred from back-pressure.
    Stats::Vector retryTime;

    //! Counts the number of times a packet is found in the local buffer, per ATP master
    Stats::Vector bufferedCount;

    //! Sum of packets  found in the local buffer, per ATP master
    Stats::Vector bufferedSum;

    //! Average size of the adaptor local packet buffer per ATP master
    Stats::Formula avgBufferedPackets;

    //! Number of packets sent by ATP Engine
    Stats::Vector atpSent;

    //! Number of packets received by ATP Engine
    Stats::Vector atpReceived;

    //! Send rate for ATP packets
    Stats::Vector atpSendRate;

    //! Receive rate for ATP packets
    Stats::Vector atpReceiveRate;

    //! Average request to response latency
    Stats::Vector atpLatency;

    //! Request to response Jitter (latency STD dev)
    Stats::Vector atpJitter;

    //! Total ATP FIFO buffer underruns
    Stats::Vector atpFifoUnderruns;

    //! Total ATP FIFO buffer overruns
    Stats::Vector atpFifoOverruns;

    //! Average OT
    Stats::Vector atpOt;

    //! Average FIFO level
    Stats::Vector atpFifoLevel;

    //! ATP masters start time
    Stats::Vector atpStartTime;

    //! ATP masters termination time
    Stats::Vector atpFinishTime;

    //! ATP masters run time
    Stats::Vector atpRunTime;

    //! Enables the ATP to exit simulation if all profiles are depleted
    const bool exitWhenDone;

    //! Allows ATP Engine to generate out-of-range addresses
    const bool outOfRangeAddresses;

    /*! Enables the ATP to exit simulation if at least one master depletes all its
     *  configured traffic profiles
     */
    const bool exitWhenOneMasterEnds;

    //! Activates ProfileGen dumping a trace of ATP generated packets
    const bool traceAtp;

    //! Activates ProfileGen dumping a trace of GEM5 generated packets
    const bool traceGem;
    //! File name for dumping packets trace
    const std::string traceGemFileName;

    //! Activates ProfileGen dumping per ATP master M3I traces
    const bool traceM3i;
    //! M3I bus width used to dump the per ATP master M3I traces
    const uint8_t traceM3iBusWidth;
    //! enables Debug Mode: create a Master per each profile
    const bool profilesAsMasters;
    //! enables the ATP Tracker latency for all ATP Masters
    const bool trackerLatency;
    //! Enables ATP Engine debug mode
    const bool coreEngineDebug;
    //! Initialises ATP Engine but does not trigger an update immediately after
    const bool initOnly;
    //! Disables ProfileGen watchdog
    const bool disableWatchdog;
    //! Disables check for ATP Engine valid physical memory addresses
    const bool disableMemCheck;

    //! Stores the last tick used to dump an M3I packet
    uint64_t traceM3iLastTick{ 0 };

    //! Time unit to be used for interactions with the ATP Engine
    TrafficProfiles::Configuration::TimeUnit timeUnit{
        TrafficProfiles::Configuration::CYCLES
    };

    //! Active Stream tracking
    std::unordered_set<uint64_t> activeStreams;

    //! Per-Stream registered termination callbacks
    using TerminateCb = std::function<void()>;
    std::unordered_map<uint64_t, TerminateCb> onTerminate;

    //! Per-Stream registered build request callbacks
    using BuildReqCb = std::function<void(RequestPtr)>;
    std::unordered_map<uint64_t, BuildReqCb> onBuildReq;

    /*!
     *  builds a GEM5 Packet from an ATP one
     *\param p pointer to ATP packet
     *\return pointer to GEM5 packet
     */
    Packet* buildGEM5Packet(const TrafficProfiles::Packet* );

    /*!
     *  builds a ATP Packet from a GEM5 one
     *\param p pointer to GEM5 packet
     *\return pointer to ATP packet
     */
    TrafficProfiles::Packet* buildATPPacket(const Packet* );

    /*!
     * inserts an entry into the ATP UID routing table
     *\param p ATP Packet to be processed
     */
    void addRoutingEntry(const TrafficProfiles::Packet*);

    /*!
    * Looks up an entry into the ATP UID routing table,
    * removes it and returns the corresponding UID
    *\param p GEM5 Packet to be looked up
    *\return the corresponding UID
    */
    uint64_t lookupAndRemoveRoutingEntry(const Packet*);

    /*!
     * Gets the corresponding gem5 memory command given an ATP packet
     *\param p ATP packet
     *\return a gem5 memory command
     */
    MemCmd::Command getGem5Command(const TrafficProfiles::Packet*) const;

    /*!
     * Gets the corresponding ATP packet command given a gem5 packet
     *\param p gem5 packet
     *\return an ATP packet command
     */
    TrafficProfiles::Command getAtpCommand(const Packet*) const;

public:

    /*!
     *  Constructor
     *\param p configuration parameters
     */
    ProfileGen(const ProfileGenParams*);
    /*!
     * Returns a reference to the master port
     *\param if_name name of the interface
     *\param idx port index
     *\return reference to the master port
     */
    virtual Port& getPort(const string& if_name, PortID idx = InvalidPortID) override;

    //! Initializes the Profile Generator
    virtual void init() override;
    virtual void startup() override;
    //! Starts draining the Profile Generator
    DrainState drain() override;
    /*!
     * Serializes to checkpoint
     *\param cp reference to the checkpoint
     */
    virtual void serialize(CheckpointOut &cp) const override;
    /*!
     * Un-serializes to checkpoint
     *\param cp reference to the checkpoint
     */
    virtual void unserialize(CheckpointIn &cp) override;

    //! Registers statistics
    void regStats() override;

    /*!
     * Schedules (or reschedules if neccessary) the update event
     *\param when Tick when to schedule the update
     */
    void scheduleUpdate(const Tick when=curTick());

    /* Interface for initialising Streams from Python */
    void initStream(const std::string &master_name,
                    const std::string &root_prof_name,
                    const Addr read_base, const Addr read_range,
                    const Addr write_base, const Addr write_range,
                    const uint32_t task_id);

    /*
     * Retrieves Root Profile ID associated to unique Stream instance
     * within ATP Engine
     *\param master_name Master name to associate Stream with
     *\param root_prof_name Root Profile name associated to the Stream
     *\return Root Profile ID associated to unique Stream instance
     */
    uint64_t uniqueStream(const std::string &master_name,
                          const std::string &root_prof_name);

    /*
     * Configures a Stream of Traffic Profiles via the Traffic Profile Manager
     *
     *\param root_prof_id Root Profile ID associated to the Stream
     *\param base Stream base address
     *\param range Stream address range
     *\param type Operation type to configure (READ or WRITE)
     */
    void configureStream(const uint64_t root_prof_id,
                         const uint64_t base, const uint64_t range,
                         const TrafficProfiles::Profile_Type type);

    /*!
     * Activates a Stream of Traffic Profiles via the Traffic Profile Manager
     *
     *\param root_prof_id Root Profile ID associated to the Stream
     *\param on_terminate optional Stream termination callback
     *\param on_build_req optional Request annotation callback
     *\param auto_reset optional Automatically resets Stream on termination
     */
    void activateStream(const uint64_t root_prof_id,
                        const TerminateCb &on_terminate=nullptr,
                        const BuildReqCb &on_build_req=nullptr,
                        const bool auto_reset=false);
};

#endif /* __AMBA_PROFILE_GEN_HH__ */
