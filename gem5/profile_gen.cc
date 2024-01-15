/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Sep 25, 2015
 *      Author: Matteo Andreozzi
 */

#include "gem5/profile_gen.hh"

#include <set>
#include <utility>

#include "base/intmath.hh"
#include "base/random.hh"
#include "sim/stats.hh"
#include "sim/system.hh"
#include "base/debug.hh"
#include "base/output.hh"
#include "debug/ATP.hh"
#include "debug/Checkpoint.hh"
#include "sim/sim_exit.hh"

// ATP includes
#include "proto/tp_packet.pb.h"
#include "stats.hh"
#include "traffic_profile_desc.hh"

/*
 * We use many gem5 definitions, so we import this namespace for convenience.
 * This should be safe because we do not import the TrafficProfiles namespace,
 * which would be the most likely to produce clashes.
 */
using namespace gem5;

ProfileGen::ProfileGen(const Params &p) :
        SimObject(p), system(p.system), configFiles(p.config_files),
        updateEvent(this), watchdogEvent(this),
        exitWhenDone(p.exit_when_done),
        outOfRangeAddresses(p.out_of_range_addresses),
        exitWhenOneMasterEnds(p.exit_when_one_master_ends),
        traceAtp(p.trace_atp),
        traceGem(p.trace_gem), traceGemFileName(p.trace_gem_file),
        traceM3i(p.trace_m3i), traceM3iBusWidth(p.trace_m3i_bus),
        profilesAsMasters(p.profiles_as_masters),
        trackerLatency(p.tracker_latency),
        coreEngineDebug(p.core_engine_debug),
        initOnly(p.init_only), disableWatchdog(p.disable_watchdog),
        disableMemCheck(p.disable_mem_check) {


    // allocate ATP masters ports
    port.reserve(p.port_port_connection_count);
    for (int i = 0; i < p.port_port_connection_count; ++i) {
        std::string portName = csprintf("%s.port[%d]", name(), i);
        ProfileGenPort* bp = new ProfileGenPort(portName, *this, i);
        port.push_back(std::unique_ptr<ProfileGenPort>(bp));
    }
}

Port&
ProfileGen::getPort(const string& if_name, PortID idx) {
    if (if_name == "port" && idx < port.size()) {
        return *port[idx];
    } else {
        // SimObject raises fatal on unrecognized port
        return SimObject::getPort(if_name, idx);
    }
}

const unordered_set<string> ProfileGen::getMasters (){
    return tpm.getMasters();
}


void ProfileGen::init() {
    for (auto &p : port) {
        if (!p->isConnected())
            fatal("ProfileGen::%s Port %s is not connected!\n", __func__,
                  p->name());
    }

    // check logging and enable ATP debug mode if needed - logs ATP init phase
    if (coreEngineDebug) {
        TrafficProfiles::Logger::get()->
                setLevel(TrafficProfiles::Logger::DEBUG_LEVEL);
    }

    // configure special debug mode which creates a master per each loaded profile
    if (profilesAsMasters) {
        DPRINTF(ATP,"ProfileGen::%s enabling special debug mode: "
                "creating one master per configured profile\n", __func__);
        tpm.enableProfilesAsMasters();
    }

    if (trackerLatency) {
        DPRINTF(ATP,"ProfileGen::%s enabling "
                "ATP Tracker Latency for ATP masters\n", __func__);
        tpm.enableTrackerLatency();
    }

    if (traceAtp) {
        const string& outDir = simout.directory();
        tpm.enableTracer(outDir);
        DPRINTF(ATP,"ProfileGen::%s enabling ATP Packet Traces, "
                "with output directory %s\n", __func__, outDir);
    }

    // Load the ATP specifications from configured files
    for (auto&c : configFiles) {
        tpm.load(c);
    }

    // load time unit from TPM
    timeUnit = tpm.getTimeResolution();

    // retrieve ATP master names and order them
    const auto masters{ tpm.getMasters() };
    std::set<std::string> masterNames{ masters.begin(), masters.end() };

    // register masters in gem5 and associate with ports
    for (auto& m : masterNames) {
        RequestorID id = system->getGlobalRequestorId(m);
        // register port number to current master
        if (port.size() > interface.size()) {
            uint64_t ports = interface.size();
            interface[id] = ports;
            DPRINTF(ATP,
                    "ProfileGen::%s Master %s connected to port %d\n",
                    __func__, m, interface[id]);
        } else {
            fatal("ProfileGen::%s unable to allocate a port for master %s "
                    "- total configured ports: %d", __func__, m,
                    port.size());
        }
    }

    // Register a callback to record ATP statistics
    statistics::registerDumpCallback([this]() { recordAtpStats(); });

    // initializing watchdog timer to 1s , based on gem5 configured time resolution
    watchdogEventTimer = sim_clock::as_float::s;
}

void
ProfileGen::startup() {
    SimObject::startup();

    // schedule first event
    if (!initOnly) scheduleUpdate();
}

DrainState ProfileGen::drain() {
    if (!updateEvent.scheduled()) {
        // no event has been scheduled yet (e.g. switched from atomic mode)
        return DrainState::Drained;
    }
    if (retryPkt.empty()) {
        // shut things down
        nextPacketTick = MaxTick;
        deschedule(updateEvent);
        return DrainState::Drained;
    } else {
        return DrainState::Draining;
    }
}

//TODO how do I serialize the TPM ? shall I just reload the config file?
void ProfileGen::serialize(CheckpointOut &cp) const {
    warn("ProfileGen::%s gem5 checkpoints support is not implemented in ATP",
         __func__);

    DPRINTF(Checkpoint, "ProfileGen::%s Serializing TrafficGen\n", __func__);

    // save next event tick
    Tick nextEvent = updateEvent.scheduled() ? updateEvent.when() : 0;

    DPRINTF(ATP, "ProfileGen::%s Saving nextEvent=%llu\n", __func__,
            nextEvent);

    SERIALIZE_SCALAR(nextEvent);

    SERIALIZE_SCALAR(nextPacketTick);
}

// TODO how to unserialize the TPM?
void ProfileGen::unserialize(CheckpointIn &cp) {
    // TODO restore the TPM
    warn("ProfileGen::%s gem5 checkpoints support is not implemented in ATP",
         __func__);
    // restore scheduled events
    Tick nextEvent;
    UNSERIALIZE_SCALAR(nextEvent);
    if (nextEvent != 0) {
        scheduleUpdate(nextEvent);
    }
    UNSERIALIZE_SCALAR(nextPacketTick);
}

MemCmd::Command ProfileGen::getGem5Command(const TrafficProfiles::Packet* p) const {
   MemCmd::Command cmd = MemCmd::NUM_MEM_CMDS;
   // set command
   switch (p->cmd()) {
       case TrafficProfiles::Command::READ_REQ:
           cmd = MemCmd::ReadReq;
           break;
       case TrafficProfiles::Command::WRITE_REQ:
           cmd = MemCmd::WriteReq;
           break;
       default:
           // abort
           fatal("ProfileGen::%s Unexpected ATP packet command", __func__);
           break;
   }
   return cmd;
}

TrafficProfiles::Command ProfileGen::getAtpCommand(const Packet* p) const {
    TrafficProfiles::Command cmd = TrafficProfiles::Command::NONE;

     if (p->isRead()) {
         cmd = TrafficProfiles::Command::READ_RESP;
     } else if (p->isWrite()) {
         cmd = TrafficProfiles::Command::WRITE_RESP;
     } else {
         // abort
         fatal("ProfileGen::%s Unexpected GEM5 packet command", __func__);
     }
     return cmd;
}


Packet* ProfileGen::buildGEM5Packet(const TrafficProfiles::Packet* p) {
    // Create new request
    RequestorID requestorId = system->getGlobalRequestorId(p->master_id());
    const uint64_t streamId{ p->stream_id() };

    RequestPtr req = std::make_shared<Request>(
        p->addr(), p->size(), 0, requestorId);

    // Dummy PC to have PC-based pre-fetchers latch on
    // get entropy into higher bits
    req->setPC(((Addr) requestorId) << 2);

    // IOMMU ID
    if (p->has_iommu_id())
        req->setStreamId(p->iommu_id());

    // only add PartitionFieldExtentions if header file is available in gem5
    #ifdef __MEM_CACHE_TAGS_PARTITIONING_POLICIES_FIELD_EXTENTION_HH__
    // FlowID (ATP) -> PartitionFieldExtension (GEM5)
    if (p->has_flow_id()){
        // add field information using gem5 extension mechanim
        std::shared_ptr<gem5::partitioning_policy::PartitionFieldExtention>
            ext(new gem5::partitioning_policy::PartitionFieldExtention);
        ext->setPartitionID(p->flow_id());
        req->setExtension(ext);
    }
    #endif

    if (onBuildReq[streamId]) onBuildReq[streamId](req);

    MemCmd::Command cmd = getGem5Command(p);

    // Embed it in a packet
    PacketPtr pkt = new Packet(req, MemCmd(cmd));

    uint8_t* pkt_data = new uint8_t[req->getSize()];
    pkt->dataDynamic(pkt_data);

    return pkt;
}

TrafficProfiles::Packet* ProfileGen::buildATPPacket(const Packet* p) {

    TrafficProfiles::Packet * pkt = new TrafficProfiles::Packet();

    pkt->set_uid(lookupAndRemoveRoutingEntry(p));
    pkt->set_addr(p->getAddr());
    pkt->set_size(p->getSize());
    pkt->set_time(getAtpTime());

    pkt->set_master_id(system->getRequestorName(p->req->requestorId()));

    TrafficProfiles::Command cmd = getAtpCommand(p);

    pkt->set_cmd(cmd);
    return pkt;
}

uint64_t ProfileGen::getAtpTime() const {
    double t = curTick();
    // apply appropriate conversion factor
    // to the current tick as per configured
    // timeUnit
    switch (timeUnit) {
        case TrafficProfiles::Configuration::PS:
            t /= sim_clock::as_float::ps;
            break;
        case TrafficProfiles::Configuration::NS:
            t /= sim_clock::as_float::ns;
            break;
        case TrafficProfiles::Configuration::US:
            t /= sim_clock::as_float::us;
            break;
        case TrafficProfiles::Configuration::MS:
            t /= sim_clock::as_float::ms;
            break;
        case TrafficProfiles::Configuration::S:
            t /= sim_clock::as_float::s;
            break;
        case TrafficProfiles::Configuration::CYCLES: //intentional fall-trough
        default:
            break;
    }

    // Fix time conversion inaccuracies
    uint64_t ret{ static_cast<uint64_t>(t) };
    if (ret < tpm.getTime()) ret = tpm.getTime();

    DPRINTF(ATP,
            "ProfileGen::%s current tick %d " "converted in time %f (int %d) "
            "%s\n", __func__, curTick(), t, ret,
            TrafficProfiles::Configuration::TimeUnit_Name(timeUnit));
    return ret;
}

uint64_t ProfileGen::getSimTicks(const uint64_t atpTime) const {
    double ticks = atpTime;
    // apply appropriate conversion factor
    // to the current tick as per configured
    // timeUnit
    switch (timeUnit) {
        case TrafficProfiles::Configuration::PS:
            ticks *= sim_clock::as_float::ps;
            break;
        case TrafficProfiles::Configuration::NS:
            ticks *= sim_clock::as_float::ns;
            break;
        case TrafficProfiles::Configuration::US:
            ticks *= sim_clock::as_float::us;
            break;
        case TrafficProfiles::Configuration::MS:
            ticks *= sim_clock::as_float::ms;
            break;
        case TrafficProfiles::Configuration::S:
            ticks *= sim_clock::as_float::s;
            break;
        case TrafficProfiles::Configuration::CYCLES: //intentional fall-trough
        default:
            break;
    }
    DPRINTF(ATP,
            "ProfileGen::%s ATP ticks %d converted in ticks %f " "(int %d)\n",
            __func__, atpTime, ticks, (uint64_t)ticks);
    return ticks;
}

void ProfileGen::tracePacket(const Packet* pkt) {
    if (traceGem) {
        std::ofstream * traceGemFile =
                static_cast<std::ofstream *>(simout.findOrCreate(
                        traceGemFileName)->stream());
        *traceGemFile << curTick() << pkt->print();
    }
}

void ProfileGen::traceM3iPacket(const Packet* pkt) {
    if (traceM3i) {
        RequestorID mId = pkt->req->requestorId();
        string m3iName = system->getRequestorName(mId) + ".m3i";
        // if this master M3I file trace file is open
        std::ofstream * traceM3iFile =
                static_cast<std::ofstream *>(simout.findOrCreate(m3iName)->stream());

        // compute the M3I file fields L (length) and V (cycles difference)
        uint64_t V = curTick() - traceM3iLastTick;
        uint64_t L = pkt->getSize() / traceM3iBusWidth;
        const string cmd = (pkt->isRead() ? "AR" : "AW");

        *traceM3iFile << cmd << " 0x" << std::hex << pkt->getAddr() << std::dec
                << " L" << L << " incr C0000 ID00 P000 V" << V << std::endl;

    }
    // update m3i last tick
    traceM3iLastTick = curTick();
}

void ProfileGen::watchDog(){
    // enable debug if not enabled and reschedule the watchdog, else exit the simulation
    if (!debug::ATP.tracing()) {
        setDebugFlag("ATP");
        if (coreEngineDebug) {
            TrafficProfiles::Logger::get()->setLevel(
                    TrafficProfiles::Logger::DEBUG_LEVEL);
        }

        DPRINTF(ATP, "ProfileGen::%s TRIGGERED! current tick %d watchdog timer %d\n",
                __func__, curTick(), watchdogEventTimer);
        reschedule(watchdogEvent, curTick() + watchdogEventTimer, true);
    } else {
        DPRINTF(ATP, "ProfileGen::%s DUMP: nextPacketTick %d, TPM is in wait state %d\n",
                __func__, nextPacketTick, tpm.waiting());
        exitSimLoop("ProfileGen::watchDog TRIGGERED!");
    }
}

void ProfileGen::addRoutingEntry(const TrafficProfiles::Packet* p) {

    RequestorID mId = system->getGlobalRequestorId(p->master_id());

    auto& masterTable = routingTable[mId];

    auto addressTable = masterTable.find(p->addr());

    if (addressTable == masterTable.end()) {
        std::multimap<TrafficProfiles::Command, uint64_t> cmdMap;
        masterTable.insert(std::make_pair(p->addr(), cmdMap));
        addressTable = masterTable.find(p->addr());
        DPRINTF(ATP, "ProfileGen::%s created address table for address %#X\n",
                __func__, p->addr());
    }
    TrafficProfiles::Command cmd = p->cmd()==TrafficProfiles::READ_REQ?
            TrafficProfiles::READ_RESP:TrafficProfiles::WRITE_RESP;
    addressTable->second.insert(std::make_pair(cmd ,p->uid()));

    DPRINTF(ATP, "ProfileGen::%s packet master %d uid %d address %#X, cmd "
            "%s\n", __func__, mId, p->uid(), p->addr(),
            TrafficProfiles::Command_Name(cmd));
}

uint64_t ProfileGen::lookupAndRemoveRoutingEntry(const Packet* p) {
    uint64_t ret = 0;

    try {
        // O(1) lookup in nested hashmaps
        auto& masterTable = routingTable.at(p->req->requestorId());

        auto addressTable = masterTable.find(p->getAddr());

        DPRINTF(ATP, "ProfileGen::%s packet "
                "master %d address %#X, size %d\n", __func__,
                p->req->requestorId(), p->getAddr(), p->getSize());

        if (addressTable != masterTable.end()) {
            auto entry = addressTable->second.find(getAtpCommand(p));
            ret = entry->second;
            // remove UID entry
            addressTable->second.erase(entry);
            // clean-up if needed
            if (addressTable->second.empty()) {
                masterTable.erase(addressTable);
            }
            if (masterTable.empty()) {
                routingTable.erase(routingTable.find(p->req->requestorId()));
            }
        } else {
            DPRINTF(ATP, "ProfileGen::%s master table content for master %d\n",
                    __func__, p->req->requestorId());

            for (auto m = begin(masterTable); m!=end(masterTable); ++m) {
                DPRINTF(ATP, "\t entry for address %#X, size %d\n",
                        m->first, m->second.size());
            }

            throw std::out_of_range("Lookup failed in the UID routing");
        }
    }
    catch (const std::out_of_range& o) {
        fatal("ProfileGen::%s error %s packet %s", __func__, o.what(),
              p->print());
    }

    return ret;
}


void ProfileGen::update() {
    DPRINTF(ATP, "ProfileGen::%s current tick %d\n", __func__, curTick());
    locked = false;
    bool suppressed = false;
    std::string suppressedAddress("");
    // check logging and enable ATP debug mode if needed -
    // can be triggered by delayed debug activation
    if (coreEngineDebug) {
        // enable colours
        TrafficProfiles::Logger::get()->setColours(true);
        // enable the ATP logger
        TrafficProfiles::Logger::get()->
                setLevel(TrafficProfiles::Logger::DEBUG_LEVEL);
    }

    // Avoid gem5 / ATP time conversion skews
    if (nextAtpTime == MaxTick) nextAtpTime = getAtpTime();

    DPRINTF(ATP, "ProfileGen::%s requesting packets to AMBA TPM\n", __func__);
    auto temp = tpm.send(locked, nextAtpTime, nextAtpTime);

    localBuffer.insert(temp.begin(), temp.end());
    DPRINTF(ATP, "ProfileGen::%s got %d packets [total buffered %d] from AMBA TPM\n",
            __func__, temp.size(), localBuffer.size());

    // Invalidate when ATP Engine is blocked (nextAtpTime = 0)
    if (!nextAtpTime)
        nextAtpTime = nextPacketTick = MaxTick;
    // update next packet ticks from ATP next time hint
    else
        nextPacketTick = getSimTicks(nextAtpTime);

    DPRINTF(ATP, "ProfileGen::%s next packet tick is %lld (ATP Time %d)\n",
            __func__, nextPacketTick, nextAtpTime);

    uint64_t sent = 0;
    // use a temporary master map to check which masters to serve
    auto toServe = interface;
    //  loop until all masters are busy or all ATP packets are depleted
    auto i = toServe.begin();
    while (!localBuffer.empty() && !toServe.empty()) {
        RequestorID mId = i->first;
        PortID pId = i->second;
        string master = system->getRequestorName(mId);
        // save current iterator and increment i, wrap at the end
        auto current = i++;
        if (i == toServe.end()) i = toServe.begin();
        DPRINTF(ATP,
                "ProfileGen::%s checking packets for master %s [ID %d] port %d\n",
                __func__, master, mId, pId);
        // per each master in the packets ATP map, check if its port is free
        // and if there's a packet to send on it
        auto p = localBuffer.find(master);
        bool portBusy = retryPkt.find(pId) != retryPkt.end();

        if (p!=localBuffer.end()) {
            // record how many packets queued per master on average
            bufferedSum[interface[mId]]+=localBuffer.count(master);
            bufferedCount[interface[mId]]++;
        }

        if (p != localBuffer.end() && !portBusy) {

            // suppress packets that are not destined for a memory
            if (disableMemCheck || system->isMemAddr(p->second->addr())) {
                retryPkt[pId] = buildGEM5Packet(p->second);
                // add to the UID routing table
                addRoutingEntry(p->second);

                // access port for current ATP Master
                auto &sendPort = port.at(pId);
                DPRINTF(ATP,
                        "ProfileGen::%s attempting to send packet for "
                        "master %s with address %#X, on port %d still %d to send\n",
                        __func__, master, p->second->addr(), pId,
                        localBuffer.size());
                // attempt to send packet to corresponding master port
                if (!sendPort->sendTimingReq(retryPkt[pId])) {
                    retryPktTick[pId] = curTick();
                    DPRINTF(ATP, "Packet stalled for retry at %lld\n",
                            retryPktTick[pId]);
                } else {
                    sent++;
                    // log packet to trace if enabled
                    tracePacket(retryPkt[pId]);
                    traceM3iPacket(retryPkt[pId]);
                    // remove it from retry structures
                    retryPkt.erase(pId);
                    retryPktTick.erase(pId);
                }
            } else {
                DPRINTF(ATP, "Suppressed packet %d address %#X\n", p->second->cmd(),
                        p->second->addr());
                suppressed = true;
                suppressedAddress=std::to_string(p->second->addr());
            }
            // remove packet from list
            delete p->second;
            localBuffer.erase(p);
        } else {
            // this master's port is busy retransmitting or no packets
            // are available for it
            toServe.erase(current);
            if (portBusy) {
                DPRINTF(ATP, "ProfileGen::%s master %s port %d busy retransmitting, queued packets %d\n",
                        __func__, master,pId,localBuffer.count(master));
            } else {
                DPRINTF(ATP, "ProfileGen::%s master %s port %d no packets "
                        "available\n", __func__, master, pId);
            }
        }
    }
    DPRINTF(ATP,
            "ProfileGen::%s sent %d packets, still to be sent %d, locked status is %d\n",
            __func__, sent, localBuffer.size(), locked);

    // update the TPM time if needed
    tpm.setTime(getAtpTime());

    // check if any active stream is terminated and trigger their callbacks
    auto stream = activeStreams.begin();
    while (stream != activeStreams.end()) {
        const uint64_t stream_id{ *stream };
        if (tpm.streamTerminated(stream_id)) {
            stream = activeStreams.erase(stream);
            if (onTerminate[stream_id]) onTerminate[stream_id]();
        } else {
            stream++;
        }
    }

    // Schedule next update if we were able to send some of the packets
    if (nextPacketTick != MaxTick) {
        // schedule next update event based on the next packet to be transmitted
        DPRINTF(ATP, "Next event scheduled at %lld\n", nextPacketTick);
        scheduleUpdate(nextPacketTick);
    }

    if(exitWhenOneMasterEnds) {
        for (auto&m:tpm.getMasters()){
            if (tpm.isTerminated(m)){
                const std::string reason = "AMBA TPM signals master "+m+" is terminated.";
                exitSimLoop(reason);
            }
        }
    }

    // if the TPM is not locked on waits and nextPacketTick is invalid,
    // it means there nothing more to transmit
    // unless we still have some data in the buffer (atpPackets)
    // exit the simulation if configured to do so
    if ((suppressed && !outOfRangeAddresses)|| (exitWhenDone && retryPkt.empty() && localBuffer.empty()
            && (!locked) && (MaxTick == nextPacketTick)
            && !tpm.waiting())) {
        const std::string reason =
                (suppressed && !outOfRangeAddresses) ?
                        " ALERT! ProfileGen detected and suppressed packet with address "
                        +suppressedAddress+
                        "\nwith out-of-memory addresses, check your ATP files!\n" :
                        "AMBA TPM signals no more profiles are active.";
        exitSimLoop(reason);
    }

    // reschedule the watchdog - avoid wrap around
    if (!disableWatchdog && (curTick() + watchdogEventTimer) > curTick()) {
        DPRINTF(ATP, "Watchdog scheduled at %lld\n", curTick() + watchdogEventTimer);
        reschedule(watchdogEvent, curTick() + watchdogEventTimer, true);
    } else if (watchdogEvent.scheduled()) {
        DPRINTF(ATP, "Watchdog timer wrapped at %lld! Disabling Watchdog...\n", curTick());
        deschedule(watchdogEvent);
    }

}

void ProfileGen::recordAtpStats() {
    // record ATP stats
    for (auto&m : interface) {
        const TrafficProfiles::Stats s = tpm.getMasterStats(
                system->getRequestorName(m.first));
        atpSent[m.second] = s.sent;
        atpReceived[m.second] = s.received;
        atpSendRate[m.second] = s.sendRate();
        atpReceiveRate[m.second] = s.receiveRate();
        atpLatency[m.second] = s.avgLatency();
        atpJitter[m.second] = s.avgJitter();
        atpFifoUnderruns[m.second] = s.underruns;
        atpFifoOverruns[m.second] = s.overruns;
        atpOt[m.second] = s.avgOt();
        atpFifoLevel[m.second] = s.avgFifoLevel();
        atpStartTime[m.second] = s.getStartTime();
        atpFinishTime[m.second] = s.getTime();
        atpRunTime[m.second] = s.getTime() - s.getStartTime();
        DPRINTF(ATP,
                "ProfileGen::%s recording stats for master %s: %s\n",
                __func__, m.first, s.dump());
    }
    DPRINTF(ATP, "ProfileGen::%s ATP global stats: %s\n",
            __func__, tpm.getStats().dump());
}

void ProfileGen::recvReqRetry(const PortID idx) {
    DPRINTF(ATP, "ProfileGen::%s Received retry for port %d\n", __func__, idx);
    if (retryPkt.find(idx) != retryPkt.end()) {
        numRetries[idx]++;
        // attempt to send the packet, and if we are successful start up
        // the machinery again
        if (port[idx]->sendTimingReq(retryPkt[idx])) {
            DPRINTF(ATP, "ProfileGen::%s Retry Successful\n", __func__);
            tracePacket(retryPkt[idx]);
            traceM3iPacket(retryPkt[idx]);
            //Remove element from retry list
            retryPkt.erase(idx);
            // remember how much delay was incurred due to back-pressure
            // when sending the request, we also use this to derive
            // the tick for the next packet
            Tick delay = curTick() - retryPktTick[idx];
            retryPktTick.erase(idx);
            retryTime[idx] += (delay / sim_clock::as_float::s);

            if (drainState() == DrainState::Draining) {
                // shut things down
                nextPacketTick = MaxTick;
                signalDrainDone();
            } else {
                // check if any Profile has been unlocked
                scheduleUpdate();
            }
        } else {
            DPRINTF(ATP,"ProfileGen::%s WARNING!!: received retry at %d for busy port %d\n",
                    __func__, curTick(), idx);
        }
    } else {
        DPRINTF(ATP,
                "ProfileGen::%s WARNING!!: received bogus retry at %d for port %d\n",
                __func__, curTick(), idx);
    }
}

void ProfileGen::regStats() {
    DPRINTF(ATP, "ProfileGen::%s", __func__);
    SimObject::regStats();

    // Initialise all the stats
    using namespace statistics;

    // register per-master statistics

    numRetries.init(interface.size()).name(name() + ".numRetries").desc("Number of retries per ATP master");

    retryTime.init(interface.size()).name(name() + ".retryTime").desc(
            "Time spent waiting due to back-pressure (s), per ATP master").precision(12);

    bufferedCount.init(interface.size()).name(name() + ".bufferedCount")
            .desc("Counts the number of times a packet is found in the local buffer, per ATP master");

    bufferedSum.init(interface.size()).name(name() + ".bufferedSum")
        .desc("Sum of packets  found in the local buffer, per ATP master");

    avgBufferedPackets.name(name() + ".avgBufferedPackets")
            .desc("Average size of the adaptor local packet buffer per ATP master");

    avgBufferedPackets = bufferedSum/bufferedCount;

    atpSent.init(interface.size()).name(name() + ".atpSent").desc(
            "Number of packets sent by ATP master");

    atpReceived.init(interface.size()).name(name() + ".atpReceived").desc(
            "Number of packets received by ATP master");

    atpSendRate.init(interface.size()).name(name() + ".atpSendRate").desc(
            "Send rate per ATP master");

    atpReceiveRate.init(interface.size()).name(name() + ".atpReceiveRate").desc(
            "Receive rate per ATP master");

    atpLatency.init(interface.size()).name(name() + ".atpLatency").desc(
                "Request to response latency per ATP master").precision(12);

    atpJitter.init(interface.size()).name(name() + ".atpJitter")
            .desc("Request to response jitter per ATP master").precision(12);

    atpFifoUnderruns.init(interface.size()).name(name() + ".atpFifoUnderruns").desc(
            "Total ATP FIFO underruns per ATP master");

    atpFifoOverruns.init(interface.size()).name(name() + ".atpFifoOverruns").desc(
            "Total ATP FIFO overruns per ATP master");

    atpOt.init(interface.size()).name(name() + ".atpOt").desc(
               "Average OT per ATP master");

    atpFifoLevel.init(interface.size()).name(name() + ".atpFifoLevel").desc(
               "Average FIFO level per ATP master");

    atpStartTime.init(interface.size()).name(name() + ".atpStartTime").desc(
                "ATP masters start time (s)");

    atpFinishTime.init(interface.size()).name(name() + ".atpFinishTime").desc(
            "ATP masters finish time (s)");

    atpRunTime.init(interface.size()).name(name() + ".atpRunTime").desc(
                    "ATP masters run time (s)");

    // register per-master statistics
    for (auto&m : interface) {
        const string master = system->getRequestorName(m.first);
        numRetries.subname(m.second, master);
        retryTime.subname(m.second, master);
        bufferedCount.subname(m.second, master);
        bufferedSum.subname(m.second, master);
        avgBufferedPackets.subname(m.second, master);
        atpSent.subname(m.second, master);
        atpReceived.subname(m.second, master);
        atpSendRate.subname(m.second, master);
        atpReceiveRate.subname(m.second, master);
        atpLatency.subname(m.second, master);
        atpJitter.subname(m.second, master);
        atpFifoUnderruns.subname(m.second, master);
        atpFifoOverruns.subname(m.second, master);
        atpOt.subname(m.second, master);
        atpFifoLevel.subname(m.second, master);
        atpStartTime.subname(m.second, master);
        atpFinishTime.subname(m.second, master);
        atpRunTime.subname(m.second, master);
    }
}

void
ProfileGen::scheduleUpdate(const Tick when) {
    if (!system->isTimingMode()) {
        warn("ProfileGen::%s AMBA Traffic Profile Generator is only active in "
             "timing mode", __func__);
    // Skip if new time is later than currently scheduled
    } else if (!updateEvent.scheduled() || updateEvent.when() > when) {
        nextAtpTime = MaxTick;
        reschedule(updateEvent, when, true);
    }
}

void
ProfileGen::initStream(const std::string &master_name,
                       const std::string &root_prof_name,
                       const Addr read_base, const Addr read_range,
                       const Addr write_base, const Addr write_range,
                       const uint32_t task_id) {
    const uint64_t stream_id{ uniqueStream(master_name, root_prof_name) };
    if (read_base != MaxAddr && read_range != MaxAddr)
        configureStream(stream_id, read_base, read_range,
                        TrafficProfiles::Profile::READ);
    if (write_base != MaxAddr && write_range != MaxAddr)
        configureStream(stream_id, write_base, write_range,
                        TrafficProfiles::Profile::WRITE);
    activateStream(stream_id, nullptr,
                   [task_id](RequestPtr req) { req->taskId(task_id); });
}

uint64_t
ProfileGen::uniqueStream(const std::string& master_name,
                         const std::string& root_prof_name) {
    DPRINTF(ATP, "ProfileGen::%s Master %s Stream %s\n", __func__, master_name,
            root_prof_name);

    const uint64_t root_prof_id{ tpm.profileId(root_prof_name) },
                   master_id{ tpm.masterId(master_name) };
    return tpm.uniqueStream(root_prof_id, master_id);
}

void
ProfileGen::configureStream(const uint64_t root_prof_id,
                            const uint64_t base, const uint64_t range,
                            const TrafficProfiles::Profile_Type type) {
    DPRINTF(ATP, "ProfileGen::%s Stream ID %llu Base 0x%llx Range 0x%llx "
            "Type%s\n", __func__, root_prof_id, base, range,
            TrafficProfiles::Profile_Type_Name(type));

    tpm.addressStreamReconfigure(root_prof_id, base, range, type);
}

void
ProfileGen::activateStream(const uint64_t root_prof_id,
                           const TerminateCb &on_terminate,
                           const BuildReqCb &on_build_req,
                           const bool auto_reset) {
    DPRINTF(ATP, "ProfileGen::%s Stream ID %llu\n", __func__, root_prof_id);

    // Activate Root Profile
    tpm.getProfile(root_prof_id)->activate();

    // Track Stream as active
    activeStreams.emplace(root_prof_id);

    // Store callbacks
    if (auto_reset) {
        onTerminate[root_prof_id] = [this, root_prof_id, on_terminate]() {
            tpm.streamReset(root_prof_id);
            if (on_terminate) on_terminate();
        };
    } else {
        onTerminate[root_prof_id] = on_terminate;
    }
    onBuildReq[root_prof_id] = on_build_req;

    // Trigger update
    scheduleUpdate();
}

bool ProfileGen::ProfileGenPort::recvTimingResp(PacketPtr pkt) {
    profileGen.tracePacket(pkt);
    TrafficProfiles::Packet * atpPacket = profileGen.buildATPPacket(pkt);

    bool received = profileGen.tpm.receive(profileGen.getAtpTime(), atpPacket);

    DPRINTF(ATP,
            "ProfileGen::ProfileGenPort::%s %d accepted %d\n", __func__,
            curTick(), received);

    // we can reschedule the UPDATE event if the response was received successfully
    if (received) {
        // if this response was received, if no retry is scheduled,
        // try to send more data
        profileGen.scheduleUpdate();
    }

    // deallocate the response
    delete pkt;

    // always return true, this means the response has been handled and
    // deallocated
    return true;
}
