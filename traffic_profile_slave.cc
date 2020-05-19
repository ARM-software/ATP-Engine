/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 4 Aug 2016
 *      Author: Matteo Andreozzi
 */

#include "traffic_profile_manager.hh"
#include "traffic_profile_slave.hh"
#include "utilities.hh"

namespace TrafficProfiles {

TrafficProfileSlave::TrafficProfileSlave(TrafficProfileManager* manager,
        const uint64_t index, const Profile* p, const uint64_t clone_num):
                TrafficProfileDescriptor (manager, index, p, clone_num),
                bandwidth(parseRate(p->slave().rate())),
                maxOt(1),
                width(64) {
    role = SLAVE;

    // configure the slave latency response
    if (p->slave().has_latency()) {
        // constant latency response
        staticLatency = parseTime(p->slave().latency());
        latencyType = CONFIGURED;
    } else if (p->slave().has_random_latency()) {
        // random latency response
        random.latency.init(p->slave().random_latency());
        random.latencyUnit = parseTime(p->slave().random_latency_unit());
        latencyType = RANDOM;
    }

    if (p->slave().has_ot_limit()) {
        maxOt = p->slave().ot_limit();
    } else if (p->slave().has_txnlimit()) {
        maxOt = p->slave().txnlimit();
    }

    if (p->slave().has_granularity()) {
        width = p->slave().granularity();
    } else if (p->slave().has_txnsize()) {
        width = p->slave().txnsize();
    }


    // Initialise the slave FIFO
    fifo.init(this,Profile::READ,
            bandwidth.first,
            bandwidth.second, 0,
            maxOt*width, false);

    if (p->slave().master_size() > 0
            && (p->slave().has_low_address() ||
                    p->slave().has_high_address())) {
        ERROR("TrafficProfileSlave::TrafficProfileSlave slave",name,
                "can't have both assigned masters and an address range");
    }

    // register masters with the TPM
    for (int i = 0; i < p->slave().master_size(); ++i) {
        tpm->registerMasterToSlave(p->slave().master(i), id);
    }
    //  register the slave with the configured address ranges
    if (p->slave().has_low_address()) {
        uint64_t high_address = 0;
        if (p->slave().has_high_address() &&
                !p->slave().has_address_range()) {
            high_address = p->slave().high_address();
        } else if (!p->slave().has_high_address() &&
                p->slave().has_address_range()) {
            high_address = p->slave().low_address() +
                    Utilities::toBytes<double>(p->slave().address_range());
        } else {
            ERROR("TrafficProfileSlave::TrafficProfileSlave slave",
                    name,"can't have both high address bound",
                    p->slave().high_address(),
                    "and address range", p->slave().address_range());
        }
        tpm->registerSlaveAddressRange(
                p->slave().low_address(),
                high_address, id);
    }

    // the slave is always active
    // handle profile start
    activate();
}

TrafficProfileSlave::~TrafficProfileSlave() {
}

void TrafficProfileSlave::reset() {
    TrafficProfileDescriptor::reset();
    fifo.reset();
    responses.clear();
    emitEvent(Event::ACTIVATION);
    started=true;
}

bool
TrafficProfileSlave::send(bool& locked, Packet*& p, uint64_t& next) {
    // get current time
    const uint64_t& t = tpm->getTime();
    LOG("TrafficProfileSlave::send responses at time",t);
    locked = false;
    bool ok = false;
    p = nullptr;
    if (!responses.empty()) {
        if ((responses.front())->time() <= t) {
            // response available - serve it
            p = responses.front();
            responses.pop_front();
            LOG("TrafficProfileSlave::send response", Command_Name(p->cmd()),
                    "time",p->time(),"available");

            // update slave tracking FIFO
            bool overrun=false, underrun=false;
            uint64_t no_packets = (p->size()+width-1)/width;

            fifo.receive(underrun,overrun, t, no_packets*width);

            locked = false;
            ok = true;
        } else {
            LOG("TrafficProfileSlave::send no responses available at time",t);
        }
        // next gets set to next available response or 0 if nothing available
        if (!responses.empty()) {
            next=responses.front()->time();
            LOG("TrafficProfileSlave::send next available response at time",next);
        } else {
            // a slave will signal "locked" on a send if no responses can be generated
            locked = true;
            next = 0;
        }
    } else {
        LOG("TrafficProfileSlave::send no responses queued");
    }
    return (ok);
}

bool
TrafficProfileSlave::receive(uint64_t& next, const Packet* packet, const double delay) {
    // avoid unused parameter warnings
    (void) delay;

    bool overrun=false, underrun=false, ok = false;
    uint64_t request_time = 0;
    // get current time
    const uint64_t& t = tpm->getTime();
    // number of packets the slave will handle based on slave width
    uint64_t no_packets = (packet->size()+width-1)/width;
    bool locked = false;

    if ((packet->cmd() != Command::READ_REQ)
            && (packet->cmd() != Command::WRITE_REQ)) {
        ERROR("TrafficProfileSlave::receive [", this->name,
                "] unexpected packet "
                        "received of type", Command_Name(packet->cmd()),
                "UID", packet->uid(), "address ",
                Utilities::toHex(packet->addr()));
    }

    LOG("TrafficProfileSlave::receive request",Command_Name(packet->cmd()),
            "at time",t,"size",packet->size(),
            "no packets",no_packets);

    // short circuit on active(locked) prevents the FIFO to account for data
    // which cannot be issued due to maxOT reached
    if (active(locked) &&
            (fifo.send(underrun, overrun, next,
                    request_time, t, no_packets*width))) {
        // a request can be accepted
        // generate a response corresponding to the request and buffer it
        // copy request address, size, masterId, UID and ID fields
        Packet* res = new Packet(*packet);

        // set response type according to request type
        res->set_cmd(packet->cmd()==Command::READ_REQ ?
                Command::READ_RESP:Command::WRITE_RESP);

        // Select either static or random response latency
        const uint64_t latency = (latencyType==CONFIGURED ?
                staticLatency:random.latency.get()*random.latencyUnit);

        // set response time to request time + processing latency
        res->set_time(t+latency);
        LOG("TrafficProfileSlave::receive request accepted, response UID",
                res->uid(),"command",
                Command_Name(res->cmd()),"generated at time",
                res->time());
        // buffer the response
        responses.push_back(res);
        // return next response available time
        next = responses.front()->time();
        // mark request accepted
        ok = true;
    } else if ((locked || (next == 0)) && !responses.empty()) {
        next = responses.front()->time();
        LOG("TrafficProfileSlave::receive slave is locked, "
                "next response will be sent at", next);
    }

    if (!ok) {
        // wait for the request to be retransmitted
        tpm->wait(id, t,  packet->uid(),
                TrafficProfileManager::REQUEST);
    } else {
        // signal request received
        tpm->signal(id, packet->uid(),
               TrafficProfileManager::REQUEST);
    }

    return ok;
}

bool
TrafficProfileSlave::active(bool& l) {
    // a slave is locked if the current OT
    // has reached the maximum configured one
    l = (fifo.getOt() >= maxOt && (maxOt>0));
    // a slave is always active unless locked
    return !l;
}


} /* namespace TrafficProfiles */
