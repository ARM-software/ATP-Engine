/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: 1 Mar 2016
 *  Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_PACKET_TAGGER_HH__
#define __AMBA_TRAFFIC_PROFILE_PACKET_TAGGER_HH__

#include "proto/tp_packet.pb.h"

namespace TrafficProfiles {

/*!
 *\brief Implements the AMBA Traffic Profile Packets Tagger
 *
 * Tags ATP generated tagger with global configured fields
 * such as packet ID
 */
class PacketTagger {

protected:
    //! current packet ID value
    uint64_t currentId;

    //! current packet Unique ID (UID) value
    uint64_t currentUid;

    /*!
     * Generates a new packet ID
     *\return a new packet ID
     */
    uint64_t getId();

    /*!
     * Generates a new packet UID
     *\return a new packet UID
     */
    uint64_t getUid();

public:
    //! packet lowest ID value
    uint64_t lowId;
    //! packet highest ID value
    uint64_t highId;

    //! default constructor
    PacketTagger();
    //! default destructor
    virtual ~PacketTagger();

    /*!
     * Tags a packet with globally
     * configured fields
     *\param pkt pointer to the packet to be tagged
     */
    void tag(Packet*);
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_PACKET_TAGGER_HH__ */
