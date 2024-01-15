/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Sep 30, 2015
 *      Author: Matteo Andreozzi
 */

#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "proto/tp_packet.pb.h"
#include "event.hh"

#include <algorithm>
#include <stdexcept>
#include <limits>
#include <queue>
#include "traffic_profile_manager.hh"
#include "traffic_profile_master.hh"
#include "traffic_profile_checker.hh"
#include "traffic_profile_slave.hh"
#include "traffic_profile_delay.hh"
#include "utilities.hh"

using namespace google::protobuf::io;
using namespace google::protobuf;

namespace TrafficProfiles {

const Configuration::TimeUnit TrafficProfileManager::defaultTimeResolution = Configuration::PS;
const uint64_t TrafficProfileManager::defaultFrequency = 1e9;

TrafficProfileManager::TrafficProfileManager() :
                                initialized(false), profilesAsMasters(false),
                                trackerLatency(false), kronosEnabled(false),
                                kronosBucketsWidth(0), kronosCalendarLength(0),
                                kronosConfigurationValid(false),
                                time(0), timeResolution(defaultTimeResolution),
                                forwardDeclaredProfiles(0),
                                tracer(this), streamCacheValid(false), kronos(this) {
}

TrafficProfileManager::~TrafficProfileManager() {
    for (auto& p : profiles) {
        delete p;
    }
}

void TrafficProfileManager::signalReset(const uint64_t pId) {
    try {
        nonTerminatedProfiles[profiles.at(pId)->getMasterId()]++;
    } catch (out_of_range& oor){
        ERROR("TrafficProfileManager::signalReset unknown profile id", pId);
    }
}

TrafficProfileManager::PacketType
TrafficProfileManager::packetType (const Command cmd) {
    PacketType ret = NO_TYPE;

    switch(cmd) {
    case Command::READ_REQ:
    case Command::WRITE_REQ:
        ret = REQUEST;
        break;
    case Command::READ_RESP:
    case Command::WRITE_RESP:
        ret = RESPONSE;
        break;
    default:
        break;
    }
    return ret;
}

bool TrafficProfileManager::isTerminated(const string& m) {
    bool terminated = true;
    try {
        const uint64_t mId = masterId(m);
        terminated = (nonTerminatedProfiles.at(mId) == 0);
    } catch (const out_of_range& oor) {
        ERROR("TrafficProfileManager::isTerminated unknown master requested",
                m);
    }

    LOG("TrafficProfileManager::isTerminated master", m,
            (terminated ? "is" : "is not"), "terminated");

    return terminated;
}

bool TrafficProfileManager::load(const string& fileName) {
    // input stream used to acquire traffic profile specifications
    ifstream stream(fileName.c_str());
    bool ok = false;
    if (stream.is_open()) {
        LOG("TrafficProfileManager::init loading Manager object from file",
                fileName);
        // zero copy input stream from istream
        IstreamInputStream zeroCopyStream(&stream);
        // allocate google protocol buffer configuration object
        Configuration c;
        // Allocate a parser object
        TextFormat::Parser parser;
        // configure the parser to be case insensitive
        parser.AllowCaseInsensitiveField(true);

        // TextFormat acquires from plain text Google Protocol Buffer specifications
        if (parser.Parse(&zeroCopyStream, &c)) {
            // load the configuration object into the Traffic Profiles descriptors
            loadConfiguration(c);
        } else {
            ERROR("TrafficProfileManager::load errors parsing file", fileName);
        }
        // close istream
        stream.close();
        // mark TPM as initialized
        initialized = true;
        // return ok
        ok = true;
    } else {
        WARN("TrafficProfileManager::init unable to access file ", fileName);
    }

    return ok;
}

void TrafficProfileManager::configure(const Configuration& from) {
    LOG("TrafficProfileManager::configure new Configuration object loaded in TPM");
    // mark as initialized and then populate the Traffic Profile descriptors
    initialized = true;
    loadConfiguration(from);
}

uint64_t TrafficProfileManager::getOrGeneratePid(const string& name) {
    try {
        return profileMap.at(name);
    } catch (const out_of_range& oor) {
        // allocate a new ID to the forward declared profile
        uint64_t id = profiles.size();
        profileMap[name] = id;
        profiles.push_back(nullptr);
        LOG("TrafficProfileManager::",__func__,"forward declaration of "
            "profile",name,"detected, assigning id",id);
        forwardDeclaredProfiles++;
        return id;
    }
}

uint64_t TrafficProfileManager::profileId(const string& name) {
    uint64_t ret { InvalidId<uint64_t>() };
    try {
        ret =  profileMap.at(name);
    } catch (const out_of_range& oor) {
        ERROR("TrafficProfileManager::profileId",name,"does not exist");
    }
    return ret;
}

uint64_t TrafficProfileManager::getOrGenerateMid(const string& name) {
    try {
        return masterMap.at(name);
    } catch (const out_of_range& oor) {
        // allocate a new ID to the forward declared master
        uint64_t id = masters.size();
        masterMap[name] = id;
        masters.push_back(name);
        LOG("TrafficProfileManager::",__func__,"Master",name,
            "registered with ID",id);
        return id;
    }
}

uint64_t TrafficProfileManager::masterId(const string& name) const {
    uint64_t ret { InvalidId<uint64_t>() };
    try {
        ret =  masterMap.at(name);
    } catch (const out_of_range& oor) {
        ERROR("TrafficProfileManager::",__func__,name,"does not exist");
    }
    return ret;
}

const string&
TrafficProfileManager::masterName(const uint64_t mId) const {
    try {
        return masters.at(mId);
    } catch (out_of_range& oor) {
        ERROR("TrafficProfileManager::masterName out-of-range master ID", mId);
        return masters[mId];
    }
}


const unordered_set<string>
TrafficProfileManager::getMasters() const {
    unordered_set<string> ret (masters.begin(),
                masters.end());
        return ret;
}

const Stats TrafficProfileManager::getMasterStats(const string& m) {
    Stats ret;

    bool terminated = isTerminated(m);
    LOG("TrafficProfileManager::getMasterStats master",m,"terminated",
            terminated?"Y":"N");
    ret.timeScale = toFrequency(timeResolution);
    const uint64_t mId = masterId(m);
    auto it = masterProfiles.find(mId);
    if (it != masterProfiles.end()) {
        for (auto& p : it->second) {
            if (!terminated) {
                profiles.at(p)->setStatsTime(time);
            }
            ret += profiles.at(p)->getStats();
        }
    } else {
        ERROR("TrafficProfileManager::getMasterStats Unknown master requested",
                m);
    }
    return ret;
}

TrafficProfileDescriptor* TrafficProfileManager::getProfile(
        const uint64_t index) const {
    TrafficProfileDescriptor* ret(nullptr);
    try {
        ret = profiles.at(index);
    } catch (out_of_range& oor) {
        ERROR("TrafficProfileManager::getProfile unable to find profile id",
                index, "in", oor.what());
    }
    return ret;
}

const Stats TrafficProfileManager::getProfileStats(const string& p) {
    Stats ret;
    try {
        const auto id = profileMap.at(p);
        ret = profiles.at(id)->getStats();
    } catch (out_of_range& oor) {
        ERROR(
                "TrafficProfileManager::getProfileStats Unknown profile requested",
                p);
    }
    return ret;
}

uint64_t TrafficProfileManager::toFrequency(const Configuration::TimeUnit t) {
    uint64_t ret = 1;
    switch (t) {
    case Configuration::PS:
        ret = 1e+12;
        break;
    case Configuration::NS:
        ret = 1e+9;
        break;
    case Configuration::US:
        ret = 1e+6;
        break;
    case Configuration::MS:
        ret = 1e+3;
        break;
    case Configuration::S: //intentional fall-trough
    case Configuration::CYCLES: //intentional fall-trough
    default:
        break;
    }
    return ret;
}

void TrafficProfileManager::loadTaggerConfiguration(const Configuration& c) {
    // configure the packet tagger with Packet ID boundaries from configuration
    // file. In case TPM was already initialized, the configuration can only extend
    // the existing range
    if (c.has_lowid()) {
        tagger.low_id =
                (!initialized) ?
                        c.lowid() : min(tagger.low_id, c.lowid());
    }

    if (c.has_highid()) {
        tagger.high_id =
                (!initialized) ?
                        c.highid() : max(tagger.high_id, c.highid());
    }

    LOG("TrafficProfileManager::loadPacketIDConfiguration",
            (initialized ? "Initialising" : "Extending"),
            "packet ID boundaries to", tagger.low_id, tagger.high_id);
}

void TrafficProfileManager::loadTracerConfiguration(const Configuration& c) {
    if (c.has_tracing() && c.tracing()) {
        // enable tracing
        tracer.enable();
        if (c.has_trace_dir()) {
            // set traces output directory
            tracer.setOutDir(c.trace_dir());
        }
        LOG("TrafficProfileManager::loadTracerConfiguration tracer enabled",
                (c.has_trace_dir()?"output directory "+c.trace_dir():""));
    }
}

void TrafficProfileManager::enableTracer(const string& out) {

    tracer.enable();
    tracer.setOutDir(out);

    LOG("TrafficProfileManager::enableTracer enabled "
            "packet tracer with output dir",out);
}

pair<uint64_t, uint64_t> TrafficProfileManager::loadTimeConfiguration(
        const Configuration& c) {

    // Print ATP time resolution
    LOG("TrafficProfileManager::loadTimeConfiguration ATP time resolution is set to",
            Configuration::TimeUnit_Name(timeResolution));

    // time scaling factor expressed as ratio (scale/period)
    uint64_t scale = 1;
    uint64_t period = 1;

    // time factors expressed in number of units per second (frequency)
    uint64_t frequencyFromConfig = 1;
    // set period
    uint64_t periodFromConfig = (c.has_period() ? c.period() : 1);

    uint64_t currentFrequency = toFrequency(timeResolution);

    // by default convert CYCLES to US if the frequency has been configured as well
    if (c.timeunit() == Configuration::CYCLES) {
        if (c.has_frequency()) {
            // use configured time unit to
            // compute the appropriate scale factor
            frequencyFromConfig = c.frequency();
        } else {
            WARN("TrafficProfileManager::loadTimeConfiguration: CYCLES configured with no\n"
                    "associated frequency value. Cycle measurements will be inaccurate as\n"
                    "dependent on host platform configuration, ATP Engine frequency will be set\n"
                    "to default", defaultFrequency);
            frequencyFromConfig = defaultFrequency;
        }
    } else {
        frequencyFromConfig = toFrequency(c.timeunit());
    }

    if (frequencyFromConfig > currentFrequency) {
        scale = frequencyFromConfig / currentFrequency;
    } else {
        period = currentFrequency / frequencyFromConfig;
    }

    // take configured period into account now

    period *= periodFromConfig;

    LOG("TrafficProfileManager::loadTimeConfiguration",
            Configuration::TimeUnit_Name(c.timeunit()),
            "configuration detected, time scale factor to",
            Configuration::TimeUnit_Name(timeResolution), "is [scale]", scale,
            "over [period]", period);

    return pair<uint64_t, uint64_t>(scale, period);
}

pair<uint64_t, uint64_t> TrafficProfileManager::getTimeScaleFactors(
        const uint64_t id) const {
    pair<uint64_t, uint64_t> ret;
    try {
        ret = timeScaleFactor.at(id);
    } catch (out_of_range& oo) {
        ERROR(
                "TrafficProfileManager::getTimeScaleFactor unable to find profile ID",
                id);
    }
    return ret;
}

void TrafficProfileManager::createProfile(
        const uint64_t id, const Profile& from, const uint64_t clone_num,
        const uint64_t master_id) {

    TrafficProfileDescriptor* temp = nullptr;

    if (isValid(master_id) && master_id >= masters.size())
        ERROR("TrafficProfileManager::",__func__,
              "Unknown Master ID", master_id);

    if (id >= profiles.size()){
        ERROR("TrafficProfileManager::",__func__,
        "profile descriptor ID",id,"not found");
    }

    bool slave = false;
    // a packet descriptor means this is a master profile
    if (from.has_pattern()) {
        temp = new TrafficProfileMaster(this, id, &from, clone_num);
    } else if (from.has_slave()) {
        temp = new TrafficProfileSlave(this, id, &from, clone_num);
        LOG("TrafficProfileManager::createProfile slave detected - enabling Kronos");
        kronosEnabled = true;
        // parsing a slave profile
        slave = true;
    } else if (from.has_delay()) {
        temp = new TrafficProfileDelay(this, id, &from, clone_num);
    } else if (from.check_size() > 0) {
        temp = new TrafficProfileChecker(this, id, &from, clone_num);
        //register this profile as a checker
        checkers.insert(id);

        // if this profile is a checker, register its checked profile.
        for (int i = 0; i < from.check_size(); ++i) {
            string toCheck { from.check(i) };
            if (clone_num)
                toCheck.append(TrafficProfileDescriptor::Name::CloneSuffix)
                       .append(to_string(clone_num - 1));
            checkedByMap.emplace(make_pair(getOrGeneratePid(toCheck), temp));

            LOG("TrafficProfileManager::createProfile registered profile",
                    temp->getName(), "as checker for profile", toCheck);
        }
    } else {
        ERROR("TrafficProfileManager::createProfile "
                "- unable to determine profile type");
    }

    if (!slave) {
        const string &mName {
            profilesAsMasters ? temp->getName() :
            isValid(master_id) ? masterName(master_id) :
            from.master_id()
        };
        // register masterId (string)
        const uint64_t mId = getOrGenerateMid(mName);
        masterProfiles[mId].insert(id);
        // add the Traffic Profile to the master
        temp->addToMaster(mId, mName);
        // update active profiles count
        nonTerminatedProfiles[mId]++;
    } else {
        slaves.insert(id);
    }
    // finally push the newly created profile
    profiles[id] = temp;
}

void TrafficProfileManager::configureProfile(const Profile& from,
        const pair<uint64_t, uint64_t> ts,
        bool overwrite, const uint64_t clone_num, const uint64_t master_id) {

    if (isValid(master_id) && master_id >= masters.size())
        ERROR("TrafficProfileManager::",__func__,
              "Unknown Master ID",master_id);

    uint64_t id = 0;
    string profName { from.name() };

    if (clone_num) {
        profName.append(TrafficProfileDescriptor::Name::CloneSuffix)
                .append(to_string(clone_num - 1));
    }
    auto it = profileMap.find(profName);
    // duplicated profile names are not allowed
    if (it == profileMap.end()) {
        // build new descriptor and insert with profile in the activeList
        id = profiles.size();
        // create space for a new profile descriptor
        profiles.push_back(nullptr);
        // register in name to id map
        profileMap[profName] = id;
    } else {
        id = it->second;

        if (profiles.at(id) == nullptr) {
            // this profile name was forward-declared earlier,
            // complete the creation process
            LOG("TrafficProfileManager::configureProfile completed creation of "
                "forward-declared profile", profName);
            forwardDeclaredProfiles--;
        } else if (!overwrite) {
            ERROR("TrafficProfileManager::configureProfile duplicate"
                  " profile name detected:", profName);
        } else {
            LOG("TrafficProfileManager::configureProfile overwriting profile",
                profName);
            // deallocate previously allocated profile
            delete profiles[id];
        }
    }
    // insert the time scale factor into the map
    // needed before profile creation
    timeScaleFactor[id] = (ts);

    // create the profile
    createProfile(id, from, clone_num, master_id);

    // enable TPM
    initialized = true;
}

void TrafficProfileManager::loadConfiguration(const Configuration& toLoad) {
    // store configuration object
    config.push_back(toLoad);
    // get a reference to it
    auto& c = config.back();

    // Configure the Time Unit and eventually the cycles to time
    // conversion factor from the loaded configuration object
    auto ts = loadTimeConfiguration(c);
    stats.timeScale = toFrequency(timeResolution);
    // Configure the packet tagger
    loadTaggerConfiguration(c);
    // Configure the tracer
    loadTracerConfiguration(c);

    // Traffic Profile Manager successfully populated

    // Load Traffic Profile descriptors from data contained
    // in the configuration object
    for (int i = 0; i < c.profile_size(); i++) {
        const Profile &from { c.profile(i) };
        for (const auto &v : { from.master_id(), from.name() })
            if (v.find(TrafficProfileDescriptor::Name::Reserved)
                    != string::npos)
                ERROR("TrafficProfileManager::",__func__,"Reserved char",
                      TrafficProfileDescriptor::Name::Reserved,"found in",v);
        if (!from.has_name())
            c.mutable_profile(i)->set_name(
                    TrafficProfileDescriptor::Name::Default + to_string(
                    TrafficProfileDescriptor::Name::AnonymousCount++));
        configureProfile(from, ts);
    }

    // register checkers to checked profiles
    for (auto c = begin(checkedByMap); c != end(checkedByMap); ++c) {
        profiles.at(c->first)->registerChecker(c->second->getId());

        LOG("TrafficProfileManager::loadConfiguration registered checker",
                profiles.at(c->first)->getName(), "to profile",
                c->second->getName());
    }

    // refresh the stream cache
    streamCacheUpdate();

    LOG("TrafficProfileManager::loadConfiguration active list initialised with",
            profiles.size(), "traffic profiles", masters.size(), "masters");
}

void TrafficProfileManager::flush() {
    LOG("TrafficProfileManager::flush requested", stats.dump());
    // clear the configuration
    config.clear();
    // reset the TPM
    reset();
}

void TrafficProfileManager::reset() {
    LOG("TrafficProfileManager::reset requested", stats.dump());
    // clear cycles to time conversion factors
    timeScaleFactor.clear();
    // deallocate profiles
    for (auto& p : profiles) {
        delete p;
    }
    // clear current configured Traffic Profiles
    profiles.clear();
    profileMap.clear();
    checkers.clear();
    checkedByMap.clear();
    masters.clear();
    masterMap.clear();
    masterProfiles.clear();
    masterSlaveMap.clear();
    streamCache.clear();
    streamLeavesCache.clear();
    clonedStreams.clear();
    streamCloneToOrigin.clear();
    streamCacheValid = false;
    nonTerminatedProfiles.clear();
    activeList.clear();
    waitedResponseUidMap.clear();
    // reset all waited for requests
    waitedRequestUidMap.clear();
    // reset all waited for events
    waitEventMap.clear();
    // clear next transmission times
    nextTimes = NextTimesPq();
    // backup current configuration
    auto temp = config;
    // clear current configuration
    config.clear();
    for (auto &c : temp) {
        // reload current configuration
        loadConfiguration(c);
    }
    // reset all counters
    stats.reset();
    // reset time
    time=0;
}

bool TrafficProfileManager::print(string& output) {
    // clear passed string
    output.clear();
    // TextFormat prints plain text Google Protocol Buffer specifications
    string temp;
    for (auto& c : config) {
        if (TextFormat::PrintToString(c, &temp)) {
            output += temp;
        } else
            return false;
    }
    return true;
}

void TrafficProfileManager::updateCheckers(const uint64_t profile,
        Packet* packet, const double delay) {

    LOG("TrafficProfileManager::updateCheckers master", packet->master_id(),
            "address", Utilities::toHex(packet->addr()));

    // Initialise request flags
    bool checkerLocked = false;
    uint64_t nextCheckerTime = 0;

    // select checkers for this profile
    auto range = checkedByMap.equal_range(profile);
    for (auto it = range.first, et = range.second; it != et; ++it) {

        bool ok = false;

        LOG("TrafficProfileManager::updateCheckers", "registering",
                Command_Name(packet->cmd()), "to checker",
                it->second->getName());

        // select action based on packet command
        switch (packet->cmd()) {
        case Command::READ_REQ: //intentional fallthrough
        case Command::WRITE_REQ:
            // update checkers if available
            ok = it->second->send(checkerLocked, packet, nextCheckerTime);
            break;
        case Command::READ_RESP: //intentional fallthrough
        case Command::WRITE_RESP:
            // update checker
            ok = it->second->receive(nextCheckerTime, packet,
                    delay);
            break;
        default:
            ERROR("TrafficProfileManager::updateCheckers unexpected"
                    "packet command");
        }

        if (!ok) {
            ERROR("TrafficProfileManager::updateCheckers time", time,
                    "checker", it->second->getName(), "rejected",
                    Command_Name(packet->cmd()));
        }

    }
}

bool
TrafficProfileManager::send(Packet*& pkt, bool& locked,
        const uint64_t pId) {
    pkt = nullptr;
    // reset next time hint
    uint64_t next = 0;
    locked = false;
    bool sent = false;
    if (initialized) {
        // skip checkers - they do not send packets
        if (!isChecker(pId)) {
            // attempts to send all available packets from a traffic profile
            auto& p = profiles.at(pId);
            if (p->send(locked, pkt, next)) {
                LOG("TrafficProfileManager::send time", time,
                        "got  packet from profile", p->getName(),
                        "timestamp", pkt->time());
                // update data sent statistics
                stats.send(time, pkt->size());

                // update checkers
                updateCheckers(pId, pkt);
                // signal transmission OK
                sent = true;
            }

            if (next > time) {
                LOG("TrafficProfileManager::send time ", time,
                        "profile", p->getName(), "next send time is",
                        next);
                nextTimes.push(next);
            }
            if (locked) {
                LOG("TrafficProfileManager::send time ", time,
                        "profile", p->getName(), "was found locked");
            }
        }
    }
    return sent;
}

void TrafficProfileManager::streamCacheUpdate() {
    LOG("TrafficProfileManager::streamCacheUpdate started");
    for (auto& root: activeList) {
        if (profiles.at(root)->getRole()!=TrafficProfileDescriptor::SLAVE) {
            // cache the stream hierarchy for this profile
            getStream(root);
        }
    }
}

multimap<string, Packet*> TrafficProfileManager::send(bool& locked,
        uint64_t& nextTransmission, const uint64_t packetTime) {
    multimap<string, Packet*> ret;
    // reset next time hint
    nextTransmission = 0;
    locked = false;
    // clean next transmission times
    nextTimes = NextTimesPq();
    // parse all active Profiles and query them for the next packet to be transmitted
    if (initialized) {
        // check if all forward-declared profiles have been instantiated
        if (forwardDeclaredProfiles>0) {
        ERROR("TrafficProfileManager::loadConfiguration inconsistent "
                "naming of waited for profiles detected. Please check the ATP configuration file.");
        }
        // reset the forward-declared profiles counter guard
        forwardDeclaredProfiles = 0;

        if (packetTime >= time) {
            if (!streamCacheValid) {
                // cache streams at TPM boot time
                streamCacheUpdate();
                streamCacheValid = true;
            }

            LOG("TrafficProfileManager::send request received at time",
                    packetTime);
            // update current time
            time = packetTime;
            Packet * pkt = nullptr;
            // next packet times
            vector<uint64_t> nextPackets;
            // total underruns/overruns
            uint64_t underruns = 0, overruns = 0;
            // handles event concurrency by repeating loop until time advances or TPM locks
            // handle Kronos events if needed
            if (kronos.isInitialized()) {
                list<Event> events;
                // get all Kronos events
                kronos.get(events);
                // parsing events will free ATP slaves response slots
                for (auto&e : events) {
                    handle(e);
                }
            }

            // cycle through the active list
            for (auto it = begin(activeList); it != end(activeList); ++it) {
                auto& p = profiles.at(*it);
                const uint64_t& pId = p->getId();
                bool profileLocked = false, sent = false;
                // attempts to send all available packets from a traffic profile
                do {
                    sent = send(pkt, profileLocked, pId);
                    if (sent) {
                        bool waitedFor = false;
                        double reqTime = 0.;
                        uint64_t destId = 0;

                        // trace the packet
                        tracer.trace(pkt);

                        waitedFor = getDestinationProfile(reqTime, destId, pkt);

                        if (waitedFor) {

                            if (!isValid(destId)) {
                              // packet to be discarded
                              delete pkt;
                              pkt = nullptr;
                              LOG("TrafficProfileManager::send packet "
                                  "to invalid ID discarded");
                            } else {
                              LOG("TrafficProfileManager::send internal routing");

                              // 1) master going to internal slave
                              // 2) slave going to internal master
                              // if none of the above applies, emplace into ret
                              // store packet
                              bool routed = route(pkt, &pId, &destId);
                              if (!routed) {
                                  LOG("TrafficProfileManager::send in internal routing"
                                          " between",pId,"and",destId,"delayed");
                              }
                            }
                        } else {
                            ret.emplace(make_pair(p->getMasterName(), pkt));
                        }
                    }
                } while (sent);
                // OR locked status.
                locked |= profileLocked;
                // update FIFO statistics
                underruns += p->getStats().underruns;
                overruns += p->getStats().overruns;

            }
            // refresh total overruns/underruns
            stats.underruns = underruns;
            stats.overruns = overruns;
            // compute overall nextTransmission send time
            if (!nextTimes.empty()) {
                nextTransmission = nextTimes.top();
            }
            LOG("TrafficProfileManager::send time", packetTime, "sending",
                    ret.size(), "packets. Underruns", underruns, "Overruns",
                    overruns,"next transmission time", nextTransmission);
        } else {
            ERROR("TrafficProfileManager::send - received event from the past:",
                    packetTime, "current time was", time);
        }
    } else {
        ERROR("TrafficProfileManager::send - TPM not initialised!");
    }
    return ret;
}


bool TrafficProfileManager::getDestinationProfile(double& rTime,
        uint64_t& dest, const Packet* pkt) const {
    dest = 0;
    bool found = false;

    auto& wMap = (packetType(pkt->cmd())==RESPONSE ?
            waitedResponseUidMap: waitedRequestUidMap);

    if (wMap.count(pkt->uid()) > 0) {
        tie(dest, rTime) = wMap.at(pkt->uid());
        found = true;
    }

    // check against internal slaves
    if (!found) {
        found = toInternalSlave(dest,pkt);
    }

    return found;
}


bool TrafficProfileManager::receive(const uint64_t packetTime, Packet* packet) {
    bool received = false, waitedFor = false;
    if (initialized) {
        if (packetTime >= time) {
            double requestTime = time;
            uint64_t pid = 0, next = 0;
            // advance clock
            time = packetTime;
            waitedFor = getDestinationProfile(requestTime, pid, packet);

            if (!waitedFor) {
                WARN("TrafficProfileManager::receive unexpected packet "
                        "for master", packet->master_id(),
                        "UID", packet->uid(),
                        "address", packet->addr());
            } else {
              if (!isValid(pid)) {
                  // packet to be discarded
                  delete packet;
                  packet = nullptr;
                  LOG("TrafficProfileManager::receive packet "
                      "to invalid ID discarded");
                } else {
                    double delay = packetTime - requestTime;

                    if (delay < 0) {
                      ERROR("TrafficProfileManager::receive response type",
                            Command_Name(packet->cmd()),
                            "detected negative request to response delay:",
                            delay);
                    }

                    LOG("TrafficProfileManager::receive response type",
                        Command_Name(packet->cmd()),
                        "UID",packet->uid(),
                        "response time",
                        packetTime, "request time", requestTime,
                        "for address",
                        Utilities::toHex(packet->addr()),
                        "request to response delay:", delay,
                        "destination resolved to",
                        profiles[pid]->getName());

                    if (isChecker(pid)) {
                      ERROR("TrafficProfileManager::receive received "
                          "a packet for checker profile",
                          profiles[pid]->getName());
                    }

                    // packet was expected - trace it
                    tracer.trace(packet);

                    if (profiles[pid]->receive(next, packet,
                                               delay)) {
                      // update received packets counter and time
                      stats.receive(packetTime, packet->size(), delay);
                      received = true;
                      // update checkers if available
                      updateCheckers(pid, packet, delay);
                      // delete response packet here
                      delete packet;
                    } else if (kronosEnabled &&
                          isInternalMaster(packet->master_id())) {
                        // Kronos init check
                        if (!kronos.isInitialized()) {
                          initKronos();
                        }
                        // requests coming from external masters
                        //  need to be handled in the adaptor, hence
                        // the following code is triggered only
                        // for internal masters' requests

                        // only slaves can reject a packet receive,
                        // therefore schedule a pending request event
                        LOG("TrafficProfileManager::receive failed, UID",
                            packet->uid(), "scheduling receive "
                            "in Kronos at", next);
                        // create a packet receive Kronos event
                        Event receive(Event::PACKET_REQUEST_RETRY, Event::TRIGGERED,
                                      packet->uid(), next);
                        // store packet
                        buffer[packet->uid()] = packet;
                        kronos.schedule(receive);
                    }

                    if (next >= time) {
                      LOG("TrafficProfileManager::receive time ", time, "profile",
                          profiles.at(pid)->getName(), "next receive time is",
                          next);
                      nextTimes.push(next);
                  }
                }
            }
        } else {
            ERROR(
                    "TrafficProfileManager::receive - received event from the past:",
                    packetTime, "current time was", time);
        }
    } else {
        ERROR("TrafficProfileManager::receive TPM is uninitialized");
    }
    return received;
}

void TrafficProfileManager::subscribe(const uint64_t profile, const Event& ev) {
    if (ev.action == Event::TRIGGERED) {
        ERROR(
                "TrafficProfileManager::subscribe attempted subscription to a trigger",
                ev);
    }
    // special handling for ACTIVATION events: if a profile is explicitly waiting to be activated
    // it will be placed in the activeList to enable reporting the profile is locked
    if (ev.type == Event::ACTIVATION) {
        activeList.push_back(profile);
    }

    // add the profile id to the set of profiles waiting for event <ev>
    LOG("TrafficProfileManager::subscribe event", ev, "profile", profile);
    waitEventMap[ev.id][ev].insert(profile);
}

void TrafficProfileManager::event(const Event& ev) {
    if (ev.action == Event::AWAITED) {
        ERROR("TrafficProfileManager::event "
                "attempted trigger of a subscription event", ev);
    }

    // // special handling in case of ACTIVATION events
    if (ev.type==Event::ACTIVATION) {
        activeList.push_back(ev.id);
        LOG("TrafficProfileManager::event profile id",ev.id,
                "added to active list");
    }

    // special handling in case of TERMINATION events
    if (ev.type == Event::TERMINATION) {
        const string mName = profiles.at(ev.id)->getMasterName();
        const uint64_t mId = profiles.at(ev.id)->getMasterId();

        if (0 == nonTerminatedProfiles[mId]--) {
            ERROR("TrafficProfileManager::event duplicated termination event"
                    " detected in profiles for master", mName);
        }

        LOG("TrafficProfileManager::event profile",
                profiles.at(ev.id)->getName(),
                "event", ev, nonTerminatedProfiles[mId],
                "profiles active for master", mName);

        // alert all other profiles waiting for events related to the terminated profile
        for (auto&w : waitEventMap[ev.id]) {
            for (auto&p : w.second) {
                LOG("TrafficProfileManager::event profile",
                        profiles.at(p)->getName(), "receives TERMINATION of",
                        ev.id, "due to waited event", w.first);
                profiles.at(p)->receiveEvent(ev);
            }
        }
        // remove all events related to the terminated profile
        waitEventMap.erase(ev.id);
    } else if (waitEventMap.find(ev.id) != waitEventMap.end()
            && waitEventMap.at(ev.id).find(ev)
            != waitEventMap.at(ev.id).end()) {
        LOG("TrafficProfileManager::event broadcasting event", ev);

        // propagates the event to all profiles listening for it
        list<pair<uint64_t, Event>> broadcastList;

        for (auto& profile : waitEventMap[ev.id][ev]) {
            broadcastList.push_back(make_pair(profile,ev));
        }
        // delete event from event map
        waitEventMap.at(ev.id).erase(ev);
        // delete id if last event served
        if (waitEventMap.at(ev.id).empty()) {
            waitEventMap.erase(ev.id);
        }

        // notify profiles in the broadcast list
        for (auto&l : broadcastList) {
            profiles.at(l.first)->receiveEvent(l.second);
        }

    } else {
        LOG("TrafficProfileManager::event no profile subscribed to event", ev);
    }
}

void TrafficProfileManager::event(const string& m, const Event& e) {
    try {
        // retrieve master id
        uint64_t mId = masterMap.at(m);
        // retrieve set of profiles associated to mId
        auto& mp = masterProfiles.at(mId);

        for (uint64_t pid : mp) {
            // craft a new event with the appropriate ID
            Event ev(e.type, e.action, pid, e.time);
            // notify the individual profile
            event(ev);
        }

    } catch (out_of_range& oor) {
        ERROR("TrafficProfileManager::event unknown master requested", m);
    }
}

void TrafficProfileManager::wait(const uint64_t profile, const uint64_t t,
        const uint64_t uid, const PacketType type) {
    auto& wMap = (type==RESPONSE ?
            waitedResponseUidMap: waitedRequestUidMap);

    if (wMap.find(uid)==wMap.end()){
        LOG("TrafficProfileManager::wait profile",
                (!isValid(profile)? "INVALID" :
                    profiles.at(profile)->getName()), "UID", uid, "time", t);
        wMap[uid] = make_pair(profile, t);
    }
}

void TrafficProfileManager::signal(const uint64_t profile,
        const uint64_t uid, const PacketType type) {


    LOG("TrafficProfileManager::signal profile",
            profiles.at(profile)->getName(), "UID", uid);

    auto& wMap = (type==RESPONSE ?
            waitedResponseUidMap: waitedRequestUidMap);
    wMap.erase(uid);
}

void TrafficProfileManager::setKronosConfiguration(const string& b,
        const string& c) {

    const double tpmFrequency = toFrequency(timeResolution);
    const double bHz = Utilities::timeToHz<double>(b);
    const double cHz = Utilities::timeToHz<double>(c);
    LOG("TrafficProfileManager::setKronosConfiguration "
            "bucket width", b, "Hz:", bHz, "calendar length", c, "Hz:", cHz);

    kronosBucketsWidth = (bHz > 0) ? tpmFrequency / bHz : 0;
    kronosCalendarLength = (cHz > 0) ? tpmFrequency / cHz : 0;

    LOG("TrafficProfileManager::setKronosConfiguration configured"
            "bucket width time units:", kronosBucketsWidth,
            "calendar length time units:", kronosCalendarLength);

    if (kronosCalendarLength < kronosBucketsWidth) {
        ERROR("TrafficProfileManager::setKronosConfiguration "
                "Kronos calendar length must be longer than its bucket width");
    } else {
        kronosConfigurationValid=true;
    }
}

void TrafficProfileManager::registerMasterToSlave(const string& master,
        const uint64_t sId) {
    LOG("TrafficProfileManager::registerMasterToSlave master",master,
            "to slave id", sId);
    const uint64_t mId = getOrGenerateMid(master);
    masterSlaveMap.insert(make_pair(mId, sId));
}

void TrafficProfileManager::registerSlaveAddressRange (
        const uint64_t low, const uint64_t high, const uint64_t slaveId) {
    LOG("TrafficProfileManager::registerSlaveAddressRange low",
            Utilities::toHex(low), "high",Utilities::toHex(high),
            "slave ID", slaveId);

    // check for overlapping ranges by looking up low and high
    for (auto&b : {make_pair(low, slaveAddressRanges.lower_bound(low)),
        make_pair(high,slaveAddressRanges.lower_bound(high))}) {
        /*   builds two pairs:
         *   1) the newly entered low bound paired with the range
         *   having the closest smaller low bound
         *   2) the newly entered high bound paired with the range
         *   having with the closest smaller low bound to that
         *
         *   If either the new low bound or the new high bound
         *   is contained in one of such ranges, that means there's
         *   overlap with that range, and the new entry cannot be
         *   accepted
         */

        // if a candidate overlapping range is found
        if (b.second!=slaveAddressRanges.end()) {
            // value to test
            const auto& to_test = b.first;
            // high end of the range
            const auto& h = b.second->second.first;
            if (to_test <= h) {
                ERROR ("TrafficProfileManager::registerSlaveAddressRange bound",
                        Utilities::toHex(to_test), "overlaps with range",
                        // low end of the range
                        Utilities::toHex(b.second->first),
                        // high end of the range
                        Utilities::toHex(h),
                        // slave of the range
                        "of slave", b.second->second.second);
            }
        }
    }
    slaveAddressRanges[low]=make_pair(high,slaveId);
}

bool TrafficProfileManager::toInternalSlave(uint64_t& dest, const Packet* pkt) const {
    bool match=false;
    // only route request type packets to slaves
    if (packetType(pkt->cmd())==REQUEST) {
        // store packet address
        const auto& address = pkt->addr();
        // store packet master
        const auto& master = pkt->master_id();
        //1) test the packet address against registered ranges -> get a slave id
        const auto lb = slaveAddressRanges.lower_bound(address);
        // low end of range found which could match the address
        // test inclusion in high end of range
        if ((lb!=slaveAddressRanges.end()) && (lb->second.first>=address)) {
            // address range inclusion verified.
            match = true;
            dest = lb->second.second;
        }

        if (!match) {
            try {
                // lookup internal numeric masterId
                const auto masterId = masterMap.at(master);
                //2) test the packet master against master to slave mapping -> get a slave id
                // look for master/slave association
                const auto ms = masterSlaveMap.find(masterId);
                // there is a slave assigned to this master
                if (ms!=masterSlaveMap.end()) {
                    match = true;
                    dest = ms->second;
                }
            } catch (out_of_range& oor) {
                LOG("TrafficProfileManager::toInternalSlave no internal master "
                        "matches", master);
            }
        }

        //no match on both 1 or 2 -> not associated to an internal slave
        if (match) {
            LOG("TrafficProfileManager::toInternalSlave resolved",
                    "packet from master", master,"address",
                    Utilities::toHex(address),
                    "to internal slave",profiles.at(dest)->getName());
        }
    }
    return (match);
}

const unordered_map<string, string>
TrafficProfileManager::getMasterSlaves() const {

    unordered_map<string, string> ret;

    auto lambda = [this](const pair<uint64_t, uint64_t>& p) {
        return make_pair(masters.at(p.first),
                profiles.at(p.second)->getName());};

    transform(masterSlaveMap.begin(),
            masterSlaveMap.end(),
            inserter(ret,ret.begin()),
            lambda);

    return ret;
}

bool TrafficProfileManager::isInternalMaster(const string m) const {
    return (masterMap.count(m)>0);
}

void TrafficProfileManager::autoKronosConfiguration() {
    /*
     * Max data in-flight per slave is
     *
     * Max Constrained OT in time T == Period
     * MIN(rate(bytes)/width, maxOT_parameter)
     *
     *  Max c OTs
     * |____|____|_____end
     *             end of time window: max(latency,period)
     *
     * Total in flights is MaxCOT in a time window
     *
     * To configure Kronos, add up all in-flights per slave
     * and compute the maximum time window:
     *
     * calendar length = time window
     * bucket size = time window/totalOT
     *
     */
    if (!slaves.empty()) {

        // maximum constrained latency per slave
        priority_queue<uint64_t> cLatencies;

        // maximum rate update period
        priority_queue<uint64_t> periods;

        // total number of rate-constrained maxOt
        uint64_t cTotOt(0);

        for (auto& sId : slaves) {
            const TrafficProfileSlave* const s =
                    dynamic_cast<const TrafficProfileSlave* const>(profiles.at(sId));
            // constrained latency to the bandwidth period
            const uint64_t cLat = max(s->getLatency(), s->getBandwidth().second);

            LOG("TrafficProfileManager::autoKronosConfiguration slave",
                    s->getName(),"constrained latency",cLat,"from latency",
                    s->getLatency(),"bandwidth period",s->getBandwidth().second);

            // max OT limited by rate/width
            const uint64_t cOt =
                    min(((s->getBandwidth().first*cLat) / s->getWidth()), s->getMaxOt());

            cTotOt += cOt;

            LOG("TrafficProfileManager::autoKronosConfiguration slave",
                    s->getName(),"constrained OT",cOt,"from rate",
                    s->getBandwidth().first,
                    "period",s->getBandwidth().second,
                    "latency",cLat,
                    "width",s->getWidth(),
                    "max OT", s->getMaxOt());

            cLatencies.push(cLat);
            periods.push(s->getBandwidth().second);
        }

        LOG("TrafficProfileManager::autoKronosConfiguration total OT",
                cTotOt,"max latency",cLatencies.top());

        // compute Kronos parameters
        kronosCalendarLength=cLatencies.top();
        kronosBucketsWidth=kronosCalendarLength/(cTotOt>0?cTotOt:periods.top());

        kronosConfigurationValid = true;
    }

}

void TrafficProfileManager::initKronos() {
    LOG("TrafficProfileManager::startKronos");
    if (kronosEnabled) {
        if (!kronosConfigurationValid) {
            // automatically compute kronos configuration
            autoKronosConfiguration();
        }
        kronos.init();
    } else {
        ERROR("TrafficProfileManager::startKronos "
                "can't start Kronos, it's not enabled");
    }
}

bool TrafficProfileManager::handle(const Event& ev) {
    bool handled = false;
    LOG("TrafficProfileManager::handle received event", ev);

    if (Event::category[ev.type] == Event::PACKET) {
        handled = true;
        try {
            switch (ev.type) {
            case Event::PACKET_REQUEST_RETRY: {
                // Route using buffered packet
                try {
                    double requestTime = .0;
                    uint64_t dst = 0;
                    tie(dst, requestTime) = waitedRequestUidMap.at(ev.id);
                    route(buffer.at(ev.id), nullptr, &dst);
                } catch (out_of_range& oor) {
                    ERROR("TrafficProfileManager::handle unable to find "
                            "route for packet UID", oor.what());
                }
                break;
            }
            default:
                ERROR("TrafficProfileManager::handle "
                        "unimplemented actions for", Event::text[ev.type],
                        "event");
                break;
            }
        } catch (out_of_range& oor) {
            ERROR("TrafficProfileManager::handle event", ev,
                    "unable to find matching packet in buffer");
        }
    }
    return handled;
}

bool TrafficProfileManager::route(Packet*& pkt, const uint64_t* src, const uint64_t* dst) {

    // Initialise returned values
    bool routed = false;
    uint64_t srcId = 0, dstId = 0;

    if (kronosEnabled) {
        if (!kronos.isInitialized()){
            initKronos();
        }

        if (pkt == nullptr && src == nullptr) {
            ERROR("TrafficProfileManager::routing error, "
                    "no packet and no source profile id provided");
        }

        LOG("TrafficProfileManager::routing",
                (src!=nullptr)?profiles[*src]->getName():"",
                        "to",(dst!=nullptr)?profiles[*dst]->getName():"");

        // reset routed success flag
        routed = false;
        // prepare locked flag
        bool locked = false, sent = false, received = false;

        if (pkt == nullptr && src != nullptr) {
            srcId = *src;
            sent = send(pkt, locked, srcId);
        } else if (pkt!=nullptr) {
            // packet available
            sent = true;
        }
        // request packet to source or use passed value
        if (sent) {
            if (dst != nullptr) {
                // destination specified, make sure it's in the routing table
                dstId = *dst;
                wait(dstId, pkt->time(), pkt->uid(), packetType(pkt->cmd()));
            }
            // save UID for later use
            const uint64_t uid = pkt->uid();

            // route to destination - receive deletes the packet!
            received = receive(time, pkt);

            if (sent && received) {
                // flag routed success
                routed = true;
                // remove any table/buffer entries
                buffer.erase(uid);
                // reset packet pointer
                pkt=nullptr;
            }
        }
    } else {
        ERROR("TrafficProfileManager::route Kronos not enabled, unable to route");
    }
    return routed;
}

void TrafficProfileManager::tick() {
    if (kronosEnabled) {
        list<Event> events;
        bool locked = false;
        uint64_t nextTime = 0;
        auto ret = send(locked, nextTime, time);

        if (!ret.empty()) {
            ERROR("TrafficProfileManager::tick detected packets for external adaptor");
        }

        // process next times
        if (!nextTimes.empty()) {
            kronos.schedule(
                    Event(Event::TICK, Event::TRIGGERED, 0,
                            nextTimes.top()));

            LOG("TrafficProfileManager::tick scheduling next tick at",
                    nextTimes.top());
        } else {
            LOG("TrafficProfileManager::tick no more ticks");
        }

    } else {
        ERROR("TrafficProfileManager::tick can't run Kronos, it's not enabled");
    }
}

void TrafficProfileManager::loop() {
    if (kronosEnabled) {
        // check if Kronos needs to be initialized
        if (!kronos.isInitialized()){
            initKronos();
        }

        uint64_t nextTick = 0;
        do {
            // set ATP time to Kronos next event
            setTime(nextTick);
            LOG("TrafficProfileManager::loop time",time);
            // advance time
            tick();
            // get next tick time from Kronos
            nextTick = kronos.next();
            LOG("TrafficProfileManager::loop end of loop time",time,"next",nextTick);
        } while (kronos.getCounter()>0);
    } else {
        WARN("TrafficProfileManager::loop - Kronos not enabled, exiting main event loop");
    }
}

uint64_t TrafficProfileManager::getOt(const uint64_t pId) const {
    return profiles.at(pId)->getOt();
}

const list<pair<uint64_t, bool>>&
TrafficProfileManager::getStream(const uint64_t root) {
    // validate profile id
    if (root >= profiles.size()) {
        ERROR("TrafficProfileManager::getStream "
                "unknown root profile ID", root);
    }

    if (streamCache.find(root) == streamCache.end()) {
        LOG("TrafficProfileManager::getStream root building stream",
                    profiles.at(root)->getName());

        // track visited nodes across the recursion
        set<uint64_t> visited;
        // recursion unroll stack
        queue<uint64_t> recursion;
        recursion.push(root);
        visited.insert(root);
        // stream to be returned (square brackets create the entry)
        auto& stream = streamCache[root];

        // recursion stack - avoids stack overflow for large profile streams
        while (!recursion.empty()) {
            auto node = recursion.front();
            // remove current element
            recursion.pop();
            // parse leaves
            auto events = waitEventMap.find(node);
            bool isLeaf = true;

            if (events!= waitEventMap.end()) {
                // access the waitEventMap from the root
                for (auto& leaves: events->second) {
                    // follow TERMINATION event chains
                    if (leaves.first.type == Event::TERMINATION) {
                        /* this is a set of profile IDs waiting on
                         * the root TERMINATION
                         */
                        // not a leaf node
                        isLeaf = false;

                        for (auto& leaf: leaves.second) {
                            // if not visited, start recursion
                            if (visited.count(leaf)==0) {
                                LOG("TrafficProfileManager::getStream "
                                    "recursion root",
                                        profiles.at(node)->getName(),
                                        "leaf",profiles.at(leaf)->getName());
                                // recursion on this leaf
                                recursion.push(leaf);
                                // add leaf to visited nodes
                                visited.insert(leaf);
                            } else {
                                LOG("TrafficProfileManager::getStream "
                                    "recursion root",
                                        profiles.at(node)->getName(),
                                        "skipping visited leaf",
                                        profiles.at(leaf)->getName());
                            }
                        }
                    }
                }
            }
            // add the Traffic Profile to the stream
            profiles.at(node)->addToStream(root);
            // add current node to nodes to be returned
            stream.push_back(make_pair(node, isLeaf));
            if (isLeaf) {
                streamLeavesCache[root].push_back(node);
            }
        }
    } else {
        LOG("TrafficProfileManager::getStream cache hit for root",
                profiles.at(root)->getName());
    }
    return streamCache.at(root);
}

uint64_t TrafficProfileManager::cloneStream(const uint64_t root,
                                            const uint64_t master_id) {
  uint64_t cloneRoot { InvalidId<uint64_t>() };

  // validate profile id
  if (root >= profiles.size()) {
      ERROR("TrafficProfileManager::",__func__,
            "Unknown Root Profile ID",root);
  } else if (streamCloneToOrigin.find(root) != streamCloneToOrigin.end()) {
      ERROR("TrafficProfileManager::",__func__,
            "From",root,"Nested clones unsupported");
  } else if (isValid(master_id) && master_id >= masters.size()) {
      ERROR("TrafficProfileManager::",__func__,
            "Unknown Master ID",master_id);
  }
  LOG("TrafficProfileManager::cloneStream cloning",
      profiles.at(root)->getName());

  // gets/sets the number of streams cloned from this root (0 if none)
  clonedStreams[root].first++;

  const uint64_t cloneNum { clonedStreams.at(root).first };
  for (auto& p : getStream(root)) {
    // profile pointer
    auto* from = getProfile(p.first);
    const auto &ts = timeScaleFactor.at(p.first);
    // build profile from source config
    configureProfile(*from->getConfig(), ts, false, cloneNum, master_id);
    // store clone root
    if (p.first == root) {
      string profName { from->getName() };
      profName.append(TrafficProfileDescriptor::Name::CloneSuffix)
              .append(to_string(cloneNum - 1));
      cloneRoot = profileId(profName);
      clonedStreams[root].second.push_back(cloneRoot);
      streamCloneToOrigin.emplace(cloneRoot, root);
    }
  }

  // register checkers for cloned profiles
  for (const auto &check : checkedByMap)
    getProfile(check.first)->registerChecker(check.second->getId());

  if (forwardDeclaredProfiles > 0) {
    // check if clones have been left pending by the clonesStream process
    // if that happened, it means wait_for paths have not been followed
    // (can happen if the requested stream to be cloned is multi-rooted)
    cloneRoot = InvalidId<uint64_t>();
    forwardDeclaredProfiles = 0;
    ERROR("TrafficProfileManager::",__func__,"Multi-Root clones unsupported");
  } else {
    // build the new Stream
    getStream(cloneRoot);
  }

  return cloneRoot;
}

uint64_t TrafficProfileManager::uniqueStream(const uint64_t root,
                                             const uint64_t master_id) {
  uint64_t uniqueRoot { InvalidId<uint64_t>() };
  // validate profile id
  if (root >= profiles.size()) {
      ERROR("TrafficProfileManager::",__func__,
            "Unknown Root Profile ID",root);
  } else if (isValid(master_id) && master_id >= masters.size()) {
      ERROR("TrafficProfileManager::",__func__,
            "Unknown Master ID",master_id);
  } else {
    const string &master { getProfile(root)->getConfig()->master_id() };
    if (clonedStreams.find(root) == clonedStreams.end() &&
        (!isValid(master_id) || master_id == masterId(master))) {
      // first use of this stream
      uniqueRoot = root;
      clonedStreams[root].first=0;
    } else {
      uniqueRoot = cloneStream(root, master_id);
    }
  }
  return uniqueRoot;
}

uint64_t TrafficProfileManager::addressStreamReconfigure (
        const uint64_t root, const uint64_t base,
        const uint64_t range,
        const Profile::Type type) {

    // acquire stream nodes
    const auto& stream = getStream(root);
    // accumulates range reconfiguration result
    uint64_t currentRange = 0;
    // keeps track of the current address base
    uint64_t currentBase = base;
    // process the stream
    for (auto& node : stream) {

        auto& profile = profiles.at(node.first);

        LOG("TrafficProfileManager::addressStreamReconfigure node",
                profile->getName(), "base", Utilities::toHex(currentBase),
                "range",currentRange,"bytes");

        if (profile->getRole()==TrafficProfileDescriptor::MASTER) {
            auto master = dynamic_cast<TrafficProfileMaster*>(profile);
            // type check
            if (type==Profile::NONE || master->getFifoType() == type) {
                // trigger profile autoRange
                uint64_t newRange = master->autoRange();
                // reconfigure profile base and range
                master->addressReconfigure(currentBase, newRange);
                // update base address and range
                currentBase += newRange;
                currentRange += newRange;
                if (range>=currentRange) {
                    LOG("TrafficProfileManager::addressStreamReconfigure node",
                            profile->getName(), "assigned base", Utilities::toHex(base),
                            "range", newRange,
                            "updated new base",Utilities::toHex(currentBase),
                            "residual range", range-currentRange);
                } else {
                    ERROR("TrafficProfileManager::addressStreamReconfigure node reconfiguration",
                            profile->getName(), "to assigned range", newRange,
                            "caused ranged overflow to ", currentRange, "when assigned was",range);
                }
            } else {
                LOG("TrafficProfileManager::addressStreamReconfigure node",
                        profile->getName(),"skipping as of type",
                        Profile::Type_Name(master->getFifoType()),
                        "whilst reconfiguration requested on type",
                        Profile::Type_Name(type),"only");
            }
        }

    }
    // return accumulated range reconfiguration result
    return currentRange;
}

void TrafficProfileManager::streamReset(const uint64_t root) {
    LOG("TrafficProfileManager::streamReset root",profiles.at(root)->getName());
    // stream nodes
    const auto& stream = getStream(root);
    // process the stream
    for (auto& node : stream) {
        auto& profile = profiles.at(node.first);
        LOG("TrafficProfileManager::streamReset resetting node",
                profile->getName());
        profile->reset();
    }
}

bool
TrafficProfileManager::streamTerminated(const uint64_t root) {
    LOG("TrafficProfileManager::streamTerminated root",
            profiles.at(root)->getName());

    bool terminated = true;
    // acquire/generate stream nodes
    getStream(root);
    const auto& streamLeaves = streamLeavesCache.at(root);
    // test if all the leaves of the stream have terminated
    for (auto& l: streamLeaves) {

        terminated &= profiles.at(l)->isTerminated();

        LOG("TrafficProfileManager::streamTerminated tested leaf",
                        profiles.at(l)->getName(),
                        profiles.at(l)->isTerminated()?
                                "terminated":"not terminated");

    }
    return terminated;
}


} // end of namespace

