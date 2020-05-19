/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 8, 2015
 *      Author: Matteo Andreozzi
 */

#include "stats.hh"
#include "utilities.hh"
#include <sstream>
#include <algorithm>

namespace TrafficProfiles {

Stats::Stats():
        started(false), startTime(numeric_limits<uint64_t>::max()),
        timeScale(1), time(0),
        sent(0), received(0), dataSent(0),
        dataReceived(0), prevLatency(.0), jitter(.0),
        latency(.0), underruns(0), overruns(0),
        ot(0), otN(0), fifoLevel(0),
        fifoLevelN(0) {
}

Stats::~Stats() {
}

void Stats::setTime(const uint64_t t){
    // going backwards in time is prevented
    if (t >= time) {
        time = t;
    }
}

void
Stats::send (const uint64_t t, const uint64_t data, const uint64_t o) {
    // set start time if required
    start(t);
    // advance time if necessary
    setTime(t);
    // record event
    sent++;
    dataSent +=data;
    // update the OT count
    ot+=o;
    // update OT sample count only when increasing it
    otN++;
}

void
Stats::receive (const uint64_t t,const uint64_t data, const double l) {
    // set start time if required
    start(t);
    // advance time if necessary
    setTime(t);
    // record event
    received++;
    dataReceived +=data;

    /*
    * Jitter is computer iteratively based on RFC 1889 January 1996
    *
    * https://datatracker.ietf.org/doc/rfc1889/?include_text=1
    *
    * On nth packet reception,
    * the latency variation is added to the current jitter
    * following the formula:
    *
    *               J=J+(|D(n-1,n)|-J)/16
    *
    *   This algorithm is the optimal first-order estimator
    *   and the gain parameter 1/16 gives a good noise
    *   reduction ratio while maintaining a
    *   reasonable rate of convergence
    */
    jitter += ((fabs(l-prevLatency) - jitter)/16);
    prevLatency = l;
    latency += l;
}

void
Stats::fifoUpdate(const uint64_t l, const bool u, const bool o) {
    fifoLevel += l;
    fifoLevelN++;
    if (u) underruns++;
    if (o) overruns++;
}

const string
Stats::dump() const {
    stringstream ss;
    ss << "start time: "        << Utilities::toTimeString((double)startTime/(double)timeScale)
       << " finish time: "             << Utilities::toTimeString(getTime())
       << " sent: "             << sent
       << " received: "         << received
       << " data sent: "        << Utilities::toByteString(dataSent)
       << " data received: "    << Utilities::toByteString(dataReceived)
       << " avg response latency: "  << Utilities::toTimeString(avgLatency())
       << " avg response jitter: "  << Utilities::toTimeString(avgJitter())
       << " send rate:"        << Utilities::toByteString(sendRate()) << "ps"
       << " receive rate: "     << Utilities::toByteString(receiveRate()) << "ps"
       << " average OT: "       << avgOt()
       << " average FIFO level: " << avgFifoLevel()
       << " FIFO underruns: "   << underruns
       << " FIFO overruns: "    << overruns;
    return ss.str();
}

const StatObject*
Stats::xport() const {
    StatObject* ret = new StatObject();
    ret->set_start(getStartTime());
    ret->set_time(getTime());
    ret->set_sent(sent);
    ret->set_received(received);
    ret->set_datasent(dataSent);
    ret->set_datareceived(dataReceived);
    ret->set_latency(avgLatency());
    ret->set_jitter(avgJitter());
    ret->set_receiverate(receiveRate());
    ret->set_sendrate(sendRate());
    ret->set_underruns(underruns);
    ret->set_overruns(overruns);
    ret->set_ot(avgOt());
    ret->set_fifolevel(avgFifoLevel());
    return ret;
}

Stats Stats::operator+(const Stats& s) {
    Stats ret;
    // all stats objects should have the same time scale factor
    assert(this->timeScale == s.timeScale);
    ret.started = (s.started | this->started);
    ret.startTime = min(this->startTime, s.startTime);
    ret.timeScale = this->timeScale;
    ret.time = max(this->time, s.time);
    ret.sent = this->sent + s.sent;
    ret.received = this->received + s.received;
    ret.dataSent = this->dataSent + s.dataSent;
    ret.dataReceived = this->dataReceived + s.dataReceived;
    ret.latency = this->latency + s.latency;
    ret.jitter = this->jitter + s.jitter;
    ret.underruns = this->underruns + s.underruns;
    ret.overruns = this->overruns + s.overruns;
    ret.ot = this->ot + s.ot;
    ret.otN = this->otN + s.otN;
    ret.fifoLevel = this->fifoLevel + s.fifoLevel;
    ret.fifoLevelN = this->fifoLevelN + s.fifoLevelN;
    return ret;
}

Stats& Stats::operator+=(const Stats& s) {
    *this = *this + s;
    return *this;
}

} /* namespace TrafficProfiles */
