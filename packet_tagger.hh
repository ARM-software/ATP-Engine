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
#include "types.hh"

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
    uint64_t low_id{ InvalidId<uint64_t>() };
    //! packet highest ID value
    uint64_t high_id{ InvalidId<uint64_t>() };
    //! flow_id value for profile packet tagging
    uint64_t flow_id{ InvalidId<uint64_t>() };
    //! iommu_id value for profile packet tagging
    uint32_t iommu_id{ InvalidId<uint32_t>() };
    //! stream_id value for profile packet tagging
    uint64_t stream_id{ InvalidId<uint64_t>() };


    //! default constructor
    PacketTagger();
    //! default destructor
    virtual ~PacketTagger();

    /*!
     * Reset PacketTagger currentID counter when reusing PacketTagger
     */
    inline void resetCurrentId (){ this->currentId = 0; };

    /*!
     * Tags a packet with profile
     * configured fields such as iommu_id or flow_id
     *\param pkt pointer to the packet to be tagged
     */
    void tagPacket(Packet*);

    /*!
     * Tags a packet with globally
     * configured fields
     *\param pkt pointer to the packet to be tagged
     */
    void tagGlobalPacket(Packet*);
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_PACKET_TAGGER_HH__ */
