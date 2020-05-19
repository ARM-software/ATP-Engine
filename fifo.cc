/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 19, 2015
 *      Author: Matteo Andreozzi
 */
#include "fifo.hh"
#include "logger.hh"
#include "traffic_profile_desc.hh"
#include <cmath>

namespace TrafficProfiles {

Fifo::Fifo():
        EventManager(),
        profile(nullptr), type(Profile::READ),
        startupLevel(0), time(0), level(0), carry(0.),
        initialFillLevel(0), maxLevel(0), rate(0), period(0),
        inFlightData(0), firstActivation(false), firstActivationTime(0),
        linked(false), active(false), trackerEnabled(false) {
}

Fifo::~Fifo() {

}

void Fifo::reset() {
    // reset level
    level = initialFillLevel = startupLevel;
    // reset time
    firstActivationTime = time = 0;
    // deactivate the FIFO
    active = false;
    firstActivation = false;
    // clear the tracker and the OT
    tracker.clear();
    ot.clear();
    // clear in-flight data
    inFlightData = 0;
    // clear carry data
    carry = 0;
}

void Fifo::setup() {
    // validate current configuration
    if (rate > maxLevel && maxLevel > 0) {
            ERROR("Fifo::setup",(profile?profile->getName():""),
                    "FIFO type", Profile::Type_Name(type),
                    "configured with a rate not congruent to the "
                    "maximum FIFO level of", maxLevel,
                    "bytes.\nEither change the configured rate "
                    "or set the max FIFO level to", rate,"bytes");
    }

    // reset the FIFO
    reset();

    // Initialise the event manager
    if (profile) {
        setTpm(profile->getTpm());
        setEventId(profile->getId());
        // import FIFO events from associated profile
        for (auto& evMap : profile->getWaited()) {
            for (auto& ev : evMap.second) {
                if (Event::category[ev.type] == Event::FIFO_LEVEL) {
                    LOG("Fifo::setup FIFO type", Profile::Type_Name(type),
                            "importing profile event", ev);
                    waitEvent(ev.type, ev.id);
                }
            }
        }
    }

    LOG("Fifo::setup FIFO type", Profile::Type_Name(type), "rate", rate,
                    "period", period, "start level", level, "max level", maxLevel,
                (linked ? "ATP Link Enabled" : ""));
}

void Fifo::init(TrafficProfileDescriptor* d,
        const Profile::Type t,
        const FifoConfiguration* conf, const bool e) {

    profile = d;
    type = t;
    trackerEnabled = e;
    // Initialise FIFO - parse configured rate
    auto timingParams = d->parseRate(conf->rate());
    // Initialise the FIFO
    rate    = timingParams.first;
    period  = timingParams.second;



    if (conf->has_full_level()) {
        maxLevel = conf->full_level();
    } else if (conf->has_full()) {
        maxLevel = conf->full();
    } else {
        ERROR("Fifo::init missing FIFO configuration "
                "parameter full level (Full)");
    }

    // initialise startup level with default value per FIFO Type
    auto confLevel =
            type == Profile::WRITE ?
            FifoConfiguration::FULL :
            FifoConfiguration::EMPTY;

    if (conf->has_start_fifo_level()) {
        confLevel = conf->start_fifo_level();
    } else if (conf->has_start()) {
        confLevel = conf->start();
    }

    // calculate and assigned startupLevel
    startupLevel =
            (confLevel == FifoConfiguration::FULL? maxLevel : 0);
    setup();
}


void Fifo::init(TrafficProfileDescriptor* d, const Profile::Type t,
        const uint64_t r, const uint64_t p, const uint64_t l,
        const uint64_t m, const bool e) {
    // re-configure FIFO
    profile = d;
    type = t;
    rate = r;
    period = p;
    startupLevel = level = l;
    maxLevel = m;
    trackerEnabled = e;

    setup();
}


bool Fifo::receive(bool& underrun, bool& overrun, const uint64_t t, const uint64_t data) {

    bool ret = false;

    LOG("Fifo::receive type",Profile::Type_Name(type),
            "FIFO received response for data",data,"current ot",ot.size());

    if (ot.size() > 0) {
        // record data reception
        inFlightData -= min(inFlightData, data);

        if (type == Profile::READ) {
            // limit level increase to maxLevel, unless the FIFO is unbounded (maxLevel == 0)
            level += min(data, maxLevel > 0 ? maxLevel-level : data);

        } else if (type == Profile::WRITE) {
            // limit level decrease to 0 (level can never be negative)
            level -= min(level, data);
        }
        // check/decrease OT
        uint64_t residual = data;
        while (residual > 0) {
            if (ot.front() > residual) {
                ot.front() -= residual;
                residual = 0;
            } else {
                residual -= ot.front();
                ot.pop_front();
                ret = true;
            }
        }

        LOG("Fifo::receive type",Profile::Type_Name(type),"level is now", level,
                           "in-flight data is", inFlightData, "OT", ot.size());

    } else if (data > 0) {
        ERROR("Fifo::receive type",Profile::Type_Name(type),
                "unexpected packet received when ot was",ot.size());
    }

    // update FIFO level
    update(underrun, overrun, t);

    // generate FIFO events if needed
    event();
    return ret;
}

void Fifo::update(bool& underrun, bool& overrun, const uint64_t t) {

    uint64_t update = 0;
    if (t < time) {
        ERROR("Fifo::update called from the past:",t);
    }

    uint64_t deltaT = t - time;

    // handle active flag and set up start time accordingly
    if (!active) {
        if (!linked || !firstActivation) {
            if (!firstActivation) {
                firstActivation = true;
                // handles nextTransmission next time computation
                // when activated in mid-update cycle
                firstActivationTime = t;
                LOG("Fifo::update FIFO first activated at time", t);
            } else {
                LOG("Fifo::update FIFO re-activated at time", t);
            }
            // activate the FIFO
            active = true;
        } else {
            LOG("Fifo::update FIFO is linked "
                    "and rate updates are deactivated");
        }
    } else if (rate > 0) {
        double fupdate = ((double)deltaT/(double)period) * (double)rate;
        update = fupdate;

        // store carry to correct byte rounding errors
        // rounding carry to two decimal places
        carry += floor((fupdate-(double)update) *
                errorCorrectionPrecision + 0.5) / errorCorrectionPrecision;

        LOG("Fifo::update type",Profile::Type_Name(type),
                "level",level, "computed carry",carry,
                "from fupdate",fupdate,"update",update);

        while (carry >= 1) {
            LOG("Fifo::update increased by one byte "
                    "due to previous carry",carry);
            carry -= 1;
            update += 1;
        }

        //serve from initial fill level
        if (initialFillLevel > 0) {
            initialFillLevel =
                    (initialFillLevel >= update) ?
                            initialFillLevel - update : 0;

            LOG("Fifo::update tracker queue still disabled,", update,
                    "bytes served from initial fill level"
                            " (now reduced to", initialFillLevel, ")");
        } else if (trackerEnabled) {
            // update data tracking queue
            for (uint64_t i = 0; i < (deltaT/period); ++i) {
                tracker.push_back(rate);
            }

            LOG("Fifo::update tracker queue now contains",
                    tracker.size(), "entries");
        }
    }
    // update FIFO level with rate
    if (type == Profile::READ) {
        // a READ profile consumes data from the FIFO at a given rate
        if (level < update) {
            LOG("Fifo::update buffer underrun level",level,
                    "due to update",update,"level reset to zero");
            underrun = true;
            level = 0;
            // reset latency tracker
            if (trackerEnabled) {
                tracker.clear();
            }
        } else {
            level = level - update;
        }
    }
    // else if this is a WRITE transaction,
    // check if we have at least <data> in the FIFO
    else if (type == Profile::WRITE) {
        // a rate profile fills the FIFO at a given rate
        if (level + update > maxLevel) {
            LOG("Fifo::update buffer overrun level",level,
                    "due to update",update,"level reset to",
                    maxLevel);
            overrun = true;
            level = maxLevel;
            if (trackerEnabled) {
                // cap latency tracker
                tracker.resize(maxLevel / rate);
            }
        } else {
            level = level + update;
        }
   }
   LOG("Fifo::update type",Profile::Type_Name(type),
        "current time", t, "last access time", time,
        "deltaT",deltaT,
        "rate",rate,
        "period",period,
        "update",update,
        "carry", carry,
        "new level", level);

    // update current time
    time = t;
}

uint64_t Fifo::nextTransmissionTime(const uint64_t data) const {
    uint64_t ret = 0;
    // number of rate periods which cover the current time since first act
    uint64_t n = (time - firstActivationTime + period -1)/period;

    uint64_t base = firstActivationTime + n*period;
    // data which can be sent from now till base time
    uint64_t forecast = base > time? ((base-time)*rate)/period : 0;

    if (type == Profile::READ) {
        // if the rate is positive and there is data in the FIFO to be consumed by the rate
        // in order to satisfy the request, compute how many update periods does it take
        // to free the missing bytes from the FIFO at the configured rate
        ret = (rate && ((maxLevel==0)||(maxLevel-inFlightData)>=data)) ?
                ( base + ((data-forecast+level+inFlightData-maxLevel+rate-1)/rate)*period) : 0;
    } else if (type == Profile::WRITE) {
        // if the rate is positive and it is still possible to write data in the FIFO
        // given the current inFlight data, compute how long would it take to write the
        // missing bytes to satisfy the request
        ret = (rate && ((maxLevel==0)||(maxLevel-inFlightData)>=data)) ?
                base + ((data-forecast-level+inFlightData+rate-1)/rate)*period : 0;
    }
    return ret;
}

bool
Fifo::send(bool& underrun, bool& overrun, uint64_t& next,
        uint64_t& request_time, const uint64_t t, const uint64_t data) {

    LOG("Fifo::send type",Profile::Type_Name(type),
               "FIFO received request for data",data,"current ot",ot.size());

    bool ok = false;
    // reset next available data time
    next = 0;
    // set request originated time to now.
    request_time = t;
    // update FIFO level
    update(underrun, overrun, t);

    if (type == Profile::READ) {
        // if this is a READ traffic profile,
        // check if we can allocate space in the FIFO to read <data>
        // maxLevel == 0 is a special value:
        // the FIFO can always satisfy the demand
        if ((level + data + inFlightData) <= maxLevel || (maxLevel == 0)) {
            ok = true;
        }
    } else if (type == Profile::WRITE) {
        // if this is a WRITE traffic profile,
        // check if we have data in the FIFO to satisfy
        // a requests for <data>
        // maxLevel == 0 is a special value:
        // the FIFO can always satisfy the demand

        if ((level >= (data + inFlightData)) || (maxLevel == 0)) {
            ok = true;
        }
    }
    // record in-flight data and update OT
    if (ok && data > 0) {
        inFlightData += data;
        ot.push_back(data);
    } else {
        next = nextTransmissionTime(data);
    }
    // if data was served && there is a finite request rate,
    // update latency tracker queue and compute latency
    LOG("Fifo::send type",Profile::Type_Name(type),
            "current level", level, "(max",
            maxLevel,"). Data", data,
            "requested. In-Flight data is",inFlightData,
            "OT is",ot.size(),"Result is", (ok?"OK":"DENY"),
            "next time to send is",next);

    if (ok && rate) {
        uint64_t toServe = data;
        uint64_t removed = 0;
        LOG("Fifo::send type",Profile::Type_Name(type),
                "data serviced",
                data,"tracker queue size",tracker.size());

        while (trackerEnabled && toServe && tracker.size()) {
            if (tracker.front() <= toServe) {
                toServe -= tracker.front();
                tracker.pop_front();
                removed++;
            }
            else {
                tracker.front() -= toServe;
                toServe = 0;
            }
        }
        LOG("Fifo::send type",Profile::Type_Name(type),
                "removed from tracker queue", removed,
                "entries");
        if (tracker.size()) {
            LOG("front size is", tracker.front(),
                    "(fill rate per time unit is",rate,")");

            // update request time based on removed entries value - default to 0 if data is served from
            // initial FIFO level - front of the tracker is always considered as current data
            request_time -= ((tracker.size()-1) * period);
        }

        if (time < request_time ) {
            ERROR("Fifo::send type",Profile::Type_Name(type),
                    "computed negative queuing latency",
                    ((double)time-(double)request_time));
        }

        LOG("Fifo::send type",Profile::Type_Name(type),
                "current time is",time,"request time is",
                request_time, "FIFO queuing latency is",
                time-request_time, "time units (period", period, ")");
    }

    // generate FIFO events if needed
    event();

    return ok;
}

bool Fifo::receiveEvent(const Event& e) {
    const string profileName =
                profile ? profile->getName() : "UNINITIALIZED";

    LOG("Fifo::receiveEvent [", profileName, "] Event", e);

    bool ok = false;
    // super class handle
    ok = EventManager::receiveEvent(e);

    // FIFO triggers are only valid if this profile has a linked ATP FIFO
    if (ok && linked) {
        // flip the FIFO active state: this will disable
        // the rate update
        active = !active;
        // generate a new FIFO wait event
        Event::Type toWait = Event::NONE;
        switch (e.type) {
            case Event::FIFO_EMPTY:
                toWait = Event::FIFO_NOT_EMPTY;
                break;
            case Event::FIFO_FULL:
                toWait = Event::FIFO_NOT_FULL;
                break;
            case Event::FIFO_NOT_EMPTY:
                toWait = Event::FIFO_EMPTY;
                break;
            case Event::FIFO_NOT_FULL:
                toWait = Event::FIFO_FULL;
                break;
            default:
                break;
        }

        waitEvent(toWait, e.id);


        if (getWaitedCount(Event::FIFO_LEVEL)==0) {
            // no longer waiting for FIFO_LEVEL events
            // unlink this FIFO
            active = true;
            linked = false;
        }

        LOG("Fifo::receiveEvent [", profileName,
                    "] FIFO state is now",
                    (active ? "active" : "not active"),
                    (linked? "linked": "unlinked"),
                    "rescheduling event", Event::text[toWait]);
    }
    return ok;
}

void Fifo::waitEvent(const Event::Type t,
        const uint64_t i, const bool retain) {

    LOG("Fifo::waitEvent type",Event::text[t],"profile",i);
    EventManager::waitEvent(t, i, retain);
    // waiting for a FIFO event means the FIFO is linked
    linked = true;
}

void Fifo::event() {
    // FIFO full/empty events
    if (!empty()) {
        emitEvent(Event::FIFO_NOT_EMPTY);
    } else {
        emitEvent(Event::FIFO_EMPTY);
    }

    if (!full()) {
        emitEvent(Event::FIFO_NOT_FULL);
    } else {
        emitEvent(Event::FIFO_FULL);
    }
}
} /* namespace TrafficProfiles */
