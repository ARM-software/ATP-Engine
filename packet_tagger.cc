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

// Packet metadata defaults set in header
PacketTagger::PacketTagger(): currentId(0), low_id(0), high_id(0) {}

PacketTagger::~PacketTagger() {
}

uint64_t
PacketTagger::getId() {
    if (currentId<low_id){
        currentId=low_id;
    }
    // wrap to low in case of high threshold reached
    if (currentId > high_id) {
        currentId = low_id;
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
PacketTagger::tagGlobalPacket(Packet* pkt) {
    // global UID is always overwritten
    pkt->set_uid(getUid());
}

void
PacketTagger::tagPacket(Packet* pkt) {
    // set profile Packet flow_id if availalbe
    if (isValid(this->flow_id))
        pkt->set_flow_id(flow_id);

    // set profile Packet iommu_id if availalbe
    if (isValid(this->iommu_id))
        pkt->set_iommu_id(iommu_id);

    // set profile Packet stream_id if availalbe
    if (isValid(this->stream_id))
        pkt->set_stream_id(stream_id);

    // check if profile has low and high ids to support tagging packets for Packet Desc
    if (isValid(this->low_id) && isValid(this->high_id)){
        // set profile PacketID if not already set
        if (!pkt->has_id()) {
            pkt->set_id(getId());
        }
    }
}

} /* namespace TrafficProfiles */
