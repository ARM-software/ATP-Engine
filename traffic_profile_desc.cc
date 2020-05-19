/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 2, 2015
 *      Author: Matteo Andreozzi
 */

#include "traffic_profile_desc.hh"
#include "traffic_profile_manager.hh"
#include "logger.hh"
#include "utilities.hh"

#include <algorithm>
#include <stdexcept>

namespace TrafficProfiles {

constexpr char TrafficProfileDescriptor::Name::Reserved;
const string TrafficProfileDescriptor::Name::CloneSuffix {
    Reserved + string("clone")
};
const string TrafficProfileDescriptor::Name::Default {
    Reserved + string("profile")
};
uint64_t TrafficProfileDescriptor::Name::AnonymousCount { 0 };

TrafficProfileDescriptor::TrafficProfileDescriptor(
        TrafficProfileManager* manager, const uint64_t index, const Profile* p,
        const uint64_t clone_num) :
        EventManager(index,manager),
        config(p),
        role(NONE), type(p->type()), tpm(manager), id(index), name(p->name()),
        masterName(""), masterId(0),
        _masterIommuId(p->has_iommu_id() ?
                       p->iommu_id() : InvalidId<uint32_t>()),
        ot(0), started(false), startTime(0), terminated(false) {

    if (clone_num)
        name.append(Name::CloneSuffix).append(to_string(clone_num - 1));
    LOG("TrafficProfileDescriptor [", this->name, "]");

    if (manager->isProfilesAsMasters()){
        WARN("TrafficProfileDescriptor [", this->name, "] profile names used as master names");
    }

    // Initialise waited for events (base class only)
    for (int i = 0; i < p->wait_for_size(); ++i) {
        string eventStr = p->wait_for(i);
        string profile;
        Event::Type evType = Event::NONE;
        if (Event::parse(evType, profile, eventStr)) {
            // register events to the EventManager base class
            // NOTE: FIFO events will be registered separately in FIFO init function
            if (clone_num)
                profile.append(Name::CloneSuffix)
                       .append(to_string(clone_num - 1));
            waitEvent(evType, tpm->getOrGeneratePid(profile), true);
        } else {
            ERROR("TrafficProfileDescriptor", id, "unable to parse wait event",
                    eventStr);
        }
    }

    // configure time scale in stats using the global time resolution
    stats.timeScale = tpm->toFrequency(tpm->getTimeResolution());
}

TrafficProfileDescriptor::~TrafficProfileDescriptor() {

}

void TrafficProfileDescriptor::reset() {
    // restore waited termination events
    EventManager::reset();
    LOG("TrafficProfileDescriptor::reset [",
            this->name, "] reset requested");
    ot=0;
    started=false;
    if (terminated) tpm->signalReset(id);
    terminated=false;
}

void TrafficProfileDescriptor::addToMaster(const uint64_t mId,
        const string& name) {
    masterId = mId;
    masterName=name;
    LOG("TrafficProfileDescriptor::addToMaster [", this->name, "] added to", masterName);
}

void TrafficProfileDescriptor::addToStream(const uint64_t stream_id) {
    _streamId = stream_id;

    LOG("TrafficProfileDescriptor::", __func__, "[", this->name, "] added to", _streamId);
}

void
TrafficProfileDescriptor::activate()
{
    emitEvent(Event::ACTIVATION);
    started = true;
    startTime = tpm->getTime();
}

pair<uint64_t, uint64_t>
TrafficProfileDescriptor::parseRate(const string s) {
    LOG("TrafficProfileDescriptor::parseRate [", this->name,
            "] parsing",s);

    uint64_t  rate = 0, scale = 0, period = 0, multiplier = 0;
    // load global ATP time tick frequency (Hz)
    uint64_t atpFrequency = tpm->toFrequency(tpm->getTimeResolution());

    // convert rate to B
    tie(rate, multiplier) = Utilities::toRate<uint64_t>(s);

    // rate was configured by specifying its unit
    if (multiplier > 0) {
        if (atpFrequency > rate) {
            period = atpFrequency/multiplier;
        }
        else {
            scale = multiplier/atpFrequency;
        }
    } else {
        // no time unit configured, query TPM
        // for normalized scale
        // and period factors, load them
        tie(scale, period) = tpm->getTimeScaleFactors(id);
    }
    // simplify rate with period
    auto ret = Utilities::reduce(rate, period);


    LOG("TrafficProfileDescriptor::parseRate [", this->name,
            "] , configured FIFO rate to", ret.first, "every", ret.second,
            " time units");

    return ret;
}


uint64_t TrafficProfileDescriptor::parseTime(const string t) {
    LOG("TrafficProfileDescriptor::parseTime [", this->name,
                "] parsing",t);

    double ret = 0;
    // load global ATP time tick frequency (Hz)
    double atpFrequency = tpm->toFrequency(tpm->getTimeResolution());
    // convert passed time to frequency
    double frequency = Utilities::timeToHz<double>(t);
    // compute amount of time units - the error is +- 1 ATP time unit
    ret =  frequency > 0 ? atpFrequency/frequency : 0;
    LOG("TrafficProfileDescriptor::parseTime [", this->name,
            "], frequency",frequency,"computed time", ret,"ATP time units");
    return (uint64_t)round(ret);
}

} // end of namespace

