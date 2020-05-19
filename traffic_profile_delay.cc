/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * traffic_profile_delay.cc
 *
 *  Created on: 9 Aug 2016
 *      Author: matand03
 */

#include "traffic_profile_delay.hh"
#include "traffic_profile_manager.hh"

namespace TrafficProfiles {

TrafficProfileDelay::TrafficProfileDelay(TrafficProfileManager* manager,
        const uint64_t index, const Profile* p, const uint64_t clone_num) :
        TrafficProfileDescriptor(manager, index, p, clone_num),
        delay(parseTime(p->delay().time())),
        startTime(0),
        time(0) {

    if (getWaitedCount(Event::PROFILE)==0) {
        // signal profile activation
        emitEvent(Event::ACTIVATION);
    }
}

TrafficProfileDelay::~TrafficProfileDelay() {
}

void TrafficProfileDelay::reset() {
    // base class reset
    TrafficProfileDescriptor::reset();
    LOG("TrafficProfileDelay::reset", name, "reset requested");
    startTime=0;
    time=0;
    if (getWaitedCount(Event::PROFILE) == 0) {
        emitEvent(Event::ACTIVATION);
    }
}


bool TrafficProfileDelay::receiveEvent(const Event& e) {
    bool ok = false;
    // super class handle
    ok = EventManager::receiveEvent(e);
    // signal profile first activation if not locked by other profiles
    if (getWaitedCount(Event::PROFILE) == 0) {
        emitEvent(Event::ACTIVATION);
    }
    return ok;
}

bool TrafficProfileDelay::send(bool& locked, Packet*& p, uint64_t& next) {
    // avoid unused parameter warning
    (void) p;
    // get current time
    const uint64_t& t = tpm->getTime();
    // store current time
    time = t;
    // update the start time until this profile is started
    if (!started) {
        startTime = t;
        // record start time
        stats.start(t);
    }

    // check if active, this also triggers termination, eventually
    if (active(locked)) {

        LOG("TrafficProfileDelay::send [", name,"] time", t,
                "started at",startTime,"delay", delay);

        next = startTime+delay;
    } else {
        next = 0;
    }

    LOG("TrafficProfileDelay::send [", this->name,
            "] set next to ", next);

    // signal no transmission
    return false;
}


bool TrafficProfileDelay::active(bool& l) {
    bool isActive = true;
    l = (getWaitedCount(Event::PROFILE) > 0);

    // this profile will be activated
    if (!l && !started) {
        activate();
    }
    // this profile is expired if it was started and the current time is
    // past or equal its expiration time
    const bool expired = (started && (time >= startTime + delay));

    // a delay profile is active until its configured delay is expired
    isActive = !l && !expired;

    if (expired && !terminated) {
        // fire deactivation event
        emitEvent(Event::TERMINATION);

        LOG("TrafficProfileDelay::active [", name,
                "] firing deactivation event with id", id);

        terminated = true;
        // record termination time
        stats.setTime(time);
    }

    LOG("TrafficProfileDelay::active [", name, "]",
            (terminated ? "terminated" :
                    (isActive ? "is active" :
                            (l? "is locked": "is not active"))));

    return isActive;
}


bool TrafficProfileDelay::receive(
        uint64_t& next,
        const Packet* packet, const double delay) {
    // avoid unused parameter warnings
    (void) next;
    (void) packet;
    (void) delay;

    ERROR("TrafficProfileDelay::receive [", this->name, "] "
            "can't be called on a delay profile");
    return false;
}


} /* namespace TrafficProfiles */
