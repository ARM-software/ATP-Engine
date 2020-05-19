/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 4 Aug 2016
 *      Author: Matteo Andreozzi
 */

#include "traffic_profile_checker.hh"
#include "traffic_profile_manager.hh"
#include "utilities.hh"

namespace TrafficProfiles {

TrafficProfileChecker::TrafficProfileChecker(TrafficProfileManager* manager,
        const uint64_t index, const Profile* p, const uint64_t clone_num) :
        TrafficProfileDescriptor(manager, index, p, clone_num) {

    if (p->has_fifo() && p->has_type()) {
        // Initialise the FIFO
        fifo.init(this, type, &p->fifo(),
                manager->isTrackerLatencyEnabled());
    } else {
        ERROR("TrafficProfileChecker [", this->name,
                "] FIFO configuration not found");
    }

    if (p->check_size() > 0) {
        for (int i = 0; i < p->check_size(); ++i) {
            string toCheck { p->check(i) };
            if (clone_num)
                toCheck.append(Name::CloneSuffix)
                       .append(to_string(clone_num - 1));
            const uint64_t pid = tpm->getOrGeneratePid(toCheck);
            LOG("TrafficProfileChecker [", this->name,
                    "] registering profile to check: id", pid);
            // register this checker to wait for termination of the checked
            // master - this will determine its lifetime
            waitEvent(Event::TERMINATION, pid, true);
        }
    } else {
        ERROR("TrafficProfileChecker [", this->name,
                "] ATP checker configured with no profile to check");
    }

    role = CHECKER;
    // a checker is never "first activated",
    // it's activation starts with its watched profiles
    /* Not Needed
      if (getWaitedCount(Event::PROFILE)==0) {
       emitEvent(Event::ACTIVATION);
    }*/
}

TrafficProfileChecker::~TrafficProfileChecker() {
}

void TrafficProfileChecker::reset() {
    // call the base class reset
    TrafficProfileDescriptor::reset();
    LOG("TrafficProfileChecker [", this->name,"] "
            "reset requested");
    // reset the FIFO
    fifo.reset();
}

bool TrafficProfileChecker::send(bool& locked, Packet*& p, uint64_t& next) {
    // reset next transmission time
    next = 0;
    locked = false;
    bool ok = false;
    // get current time
    const uint64_t& t = tpm->getTime();

    if (active(locked)) {
        bool underrun = false, overrun = false;
        uint64_t request_time = 0;

        if (nullptr == p) {
            ERROR("TrafficProfileChecker::send checker [", this->name,
                    "] requested to record send with empty packet pointer");
        }
        ot++;

        LOG("TrafficProfileChecker::send checker [", this->name,
                "] recorded address ", Utilities::toHex(p->addr()), "OT", ot);

        ok = fifo.send(underrun, overrun, next, request_time, t, p->size());

        // update statistics
        stats.send(t, p->size(), ot);

        // updates FIFO stats
        stats.fifoUpdate(fifo.getLevel(),underrun, overrun);

    } else {
        LOG("TrafficProfileChecker::send [", this->name, "] is not active",
                locked ? "it is locked" : "it's terminated");
    }
    return ok;
}


bool TrafficProfileChecker::receive(
        uint64_t& next,
        const Packet* packet, const double delay) {
    bool underrun=false, overrun=false, locked = false;
    // get current time
    const uint64_t& t = tpm->getTime();
    next=0;
    // update OT count
    ot--;
    LOG("TrafficProfileChecker::receive checker [", this->name,
                    "] recorded address ",
                    Utilities::toHex(packet->addr()), "OT", ot);
    // this is a checker profile -> update the FIFO accordingly
    fifo.receive(underrun, overrun, t, packet->size());

    stats.receive(t, packet->size(), delay);

    // updates FIFO stats
    stats.fifoUpdate(fifo.getLevel(),underrun, overrun);

    // check if the profile is still active (triggers termination event)
    if (!active(locked) && !locked) {
        LOG("TrafficProfileChecker::receive [", this->name, "] terminated");
    }

    // a traffic profile checker should always accept requests
    return true;
}

bool TrafficProfileChecker::active(bool& l) {
    // avoid unused parameter warning
    (void) l;
    // a checker profile is active as long as its monitored profile is active
    bool isActive = waiting();

    if (!isActive && !terminated) {
        // fire deactivation event
        emitEvent(Event::TERMINATION);

        LOG("TrafficProfileChecker::active [", name,
                "] firing deactivation event with id", id);

        terminated = true;
    }

    LOG("TrafficProfileChecker::active [", name, "]",
            (terminated ?
                    "terminated" : (isActive ? "is active" :
                            "is not active")),
                            "OT",ot);

    return isActive;
}

bool TrafficProfileChecker::receiveEvent(const Event& e) {
    LOG("TrafficProfileChecker::receiveEvent [", this->name, "] Event", e);

    bool ok = false, fifoOk=false;
    // super class handle
    ok = EventManager::receiveEvent(e);
    // forward to FIFO
    // Fifo Handle
    fifoOk = fifo.receiveEvent(e);

    return ok || fifoOk;
}

} /* namespace TrafficProfiles */
