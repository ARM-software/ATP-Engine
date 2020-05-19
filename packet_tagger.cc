/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: 1 Mar 2016
 *  Author: Matteo Andreozzi
 */

#include "packet_tagger.hh"
#include "logger.hh"

namespace TrafficProfiles {

PacketTagger::PacketTagger():currentId(0), currentUid(0), lowId(0), highId(0) {

}

PacketTagger::~PacketTagger() {
}

uint64_t
PacketTagger::getId() {
    if (currentId<lowId){
        currentId=lowId;
    }
    // wrap to low in case of high threshold reached
    if (currentId > highId) {
        currentId = lowId;
    }

    LOG("PacketTagger::getId generated ID",currentId);
    // return and post-increment
    return currentId++;
}

uint64_t
PacketTagger::getUid() {
    LOG("PacketTagger::getUid generated UID", currentUid);
    //return and post-increment
    return currentUid++;
}

void
PacketTagger::tag(Packet* pkt) {
    // tags the passed packet

     // set PacketID if not already set
    if (!pkt->has_id()) {
        pkt->set_id(getId());
    }

    // global UID is always overwritten
    pkt->set_uid(getUid());
}

} /* namespace TrafficProfiles */
