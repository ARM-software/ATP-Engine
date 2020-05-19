/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *
 *  Created on: 22 Jan 2016
 *      Author: Matteo Andreozzi
 */

#include "event.hh"
#include "logger.hh"
#include <string>

namespace TrafficProfiles {

// default event configuration separator
const string Event::separator = " ";

const string Event::text[]= {
        [NONE] = "NONE",
        [ACTIVATION] = "ACTIVATION",
        [TERMINATION] ="TERMINATION",
        [FIFO_EMPTY] = "FIFO_EMPTY",
        [FIFO_FULL] = "FIFO_FULL",
        [FIFO_NOT_EMPTY] = "FIFO_NOT_EMPTY",
        [FIFO_NOT_FULL] = "FIFO_NOT_FULL",
        [PROFILE_LOCKED] = "PROFILE_LOCKED",
        [PROFILE_UNLOCKED] = "PROFILE_UNLOCKED",
        [PACKET_REQUEST_RETRY] = "PACKET_REQUEST_RETRY",
        [TICK] = "TICK"
};

const Event::Category Event::category[] = {
        [NONE]                  = NO_CATEGORY,
        [ACTIVATION]            = PROFILE,
        [TERMINATION]           = PROFILE,
        [FIFO_EMPTY]            = FIFO_LEVEL,
        [FIFO_FULL]             = FIFO_LEVEL,
        [FIFO_NOT_EMPTY]        = FIFO_LEVEL,
        [FIFO_NOT_FULL]         = FIFO_LEVEL,
        [PROFILE_LOCKED]        = SEND_STATUS,
        [PROFILE_UNLOCKED]      = SEND_STATUS,
        [PACKET_REQUEST_RETRY]  = PACKET,
        [TICK]                  = CLOCK
};

const bool Event::allowConcurrency[] = {
        [NO_CATEGORY] = true,
        [PROFILE] = true,
        [FIFO_LEVEL] = false,
        [SEND_STATUS] = false,
        [PACKET] = true,
        [CLOCK] = false
};


Event::Event(const Type ty, const Action a,
             const uint64_t eId, const uint64_t t):
    type(ty), action(a), id(eId), time(t) {
}

bool
Event::parse(Type& t, string& profile, const string& in) {

    bool ok = false;

    size_t pos = in.find_first_of(separator);
    // try to extract Profile name, even from
    // a full string in case the separator is
    // not present
    profile = in.substr(0, pos);

    // if the separator is present
    // try to extract the type
    if (pos != string::npos) {
        string typeStr = in.substr(pos+1,string::npos);
        for (uint64_t i=0; i<N_EVENTS;++i) {
            if (text[i] == typeStr){
                t = Type(i);
                ok = true;
                break;
            }
        }
    }
    else {
        // only profile ID was found in the passed
        // string, assume Type TERMINATION and return true
        t = TERMINATION;
        ok= true;
    }
    return ok;
}

Event::~Event() {
}

ostream& operator<<(ostream& os, const Event& ev){
    string y, a;

    y = Event::text[ev.type];

    switch (ev.action){
        case Event::AWAITED:
            a = "AWAITED";
            break;
        case Event::TRIGGERED:
            a = "TRIGGERED";
            break;
        default:
            break;
        }

    os << "(t: " << ev.time <<") "<< y << " " << a << "  [id:"<< ev.id << "]";
    return os;
}

} /* namespace TrafficProfiles */
