/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 2, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_PACKET_DESC_HH_
#define __AMBA_TRAFFIC_PROFILE_PACKET_DESC_HH_

// Traffic Profile includes
#include "proto/tp_config.pb.h"
#include "proto/tp_packet.pb.h"
#include "random_generator.hh"

using namespace std;

namespace TrafficProfiles {

class PacketTagger;

/*!
 *\brief Packet Descriptor
 *
 * Generates ATP packets
 * according to the configuration provided
 */
class PacketDesc {
public:
    //! type for both packet address and size generation methods
    enum Type {
        CONFIGURED = 0, RANDOM = 1
    };
protected:
    //! flag to indicate this Packet descriptor has been initialised
    bool initialized;

    //! flag to enable address alignment
    bool alignAddresses;
    /*!
     * bit-alignment for generated address - if set to 0
     * the alignment is to the packet size, rounded up to
     * the next power of two
     */
    uint64_t alignment;

    //! methodology for generating packet addresses
    Type addressType;
    //! methodology for generating packet sizes
    Type sizeType;

    /*!
     *\brief Packet tagger module
     *
     * Sets generated packet fields according to
     * local Packet Descriptor configuration
     */
    PacketTagger* tagger;

    //! base address which shall be used to generated address sequences
    uint64_t base;
    //! increment which is used to generate address sequences
    uint64_t increment;
    //! optional limit in bytes on which the address generation wraps around
    uint64_t range;
    //! optional start address
    uint64_t start;
    //! address striding enable flag
    bool striding;

    //! RandomGenDesc object used to generate random addresses
    Random::Generator randomAddress;


    //! constant packet size to be used
    uint64_t size;
    //! RandomGenDesc object used to generate random packet sizes
    Random::Generator randomSize;

    //! parent Traffic Profile ID
    uint64_t tpId;

    /*! The Command which will be set in all Packets generated
     *  by this Packet descriptor
     */
    Command cmd;
    /*!
     * This field indicates whether this Packet descriptor
     * shall expect replies to every generated Packet,
     * and of which type. It is set to NONE in case
     * the Packet descriptor does not expect replies
     */
    Command waitFor;

    //! next packet address
    uint64_t nextAddress;

    // optional address stride configuration
    /*! addresses to generate in a stride
     *  before the next address is generated
     */
    uint64_t strideN;
    /*! increment to be applied to the stride
     *  before the address is generated
     */
    uint64_t strideInc;

    /*! optional limit of the stride on which it
     * wraps around
     */
    uint64_t strideRange;

    //! first value of a stride
    uint64_t strideStart;

    //! position in the current stride
    uint64_t strideCount;

    //! total number of strides
    uint64_t strides;

    //! gets a newly generated address
    uint64_t getAddress();
    //! gets a newly generated packet size
    uint64_t getSize();
public:

    //! Default Constructor
    PacketDesc() :
      initialized(false), alignAddresses(false), alignment(0),
    addressType(CONFIGURED), sizeType(CONFIGURED), tagger(nullptr),
    base(0), increment(0), range(0), start(0), striding(false), size(0),
    tpId(0), cmd(Command::INVALID), waitFor(Command::INVALID),
    nextAddress(0), strideN(0), strideInc(0), strideRange(0),
    strideStart(0), strideCount(0), strides(0) {}

    //! Default Destructor
    virtual ~PacketDesc();

    //! Resets the packet descriptor to its initial state
    virtual void reset();

    /*!
     * Initialises the descriptor with data form the Google Protocol Buffer configuration
     * object
     *\param parentId the unique ID of the parent Traffic Profile
     *\param from constant reference to the Google Protocol Buffer configuration object
     */
    void init(const uint64_t, const PatternConfiguration&);
    /*! Request to get a new packet from the descriptor,
     * it can return false if the packet descriptor is not configure to generate
     * packets
     *\param p reference pointer to the packet to be sent
     *\param time the current time
     *\return true if any data could be sent, false otherwise
     */
    bool send(Packet*&, const uint64_t);
    /*!
     * Delivers a response packet to the descriptor. It can return false
     * if the descriptor was not waiting for this response
     *\param time the time the response packet was received
     *\param packet a pointer to the ATP response packet received
     *\return true if the response packet was expected, false otherwise
     */
    bool receive(const uint64_t, const Packet*);
    /*!
     * Returns this packet descriptor waited response
     *\return this packet descriptor waited response
     */
    inline const Command& waitingFor() const {
        return waitFor;
    }
    /*!
     * Getter method to query the initialisation status of this
     * Packet descriptor object
     *\return true if this Packet descriptor is initialised
     */
    inline const bool& isInitialized() const {
        return initialized;
    }

    /*!
     * Returns the generated command type
     *\return generated command type
     */
    inline const Command& command() const {
        return cmd;
    }

    /*!
     * Returns the configured address
     * generation type
     *\return address generation type
     */
    inline const Type& getAddressType() const {
        return addressType;
    }

    /*!
     * Returns the configured packet size
     * generation type
     *\return packet size generation type
     */
    inline const Type& getSizeType() const {
        return sizeType;
    }

    /*!
     * Returns the configured packet size
     *\return packet size
     */

    inline uint64_t getPacketSize() const {
        return size;
    }

    /*!
     * Sets the generated command type
     *\param c Command to set
     */
    inline void setCommand(const Command& c) {
        cmd = c;
        // override waitFor if not configured explicitly
        if (waitFor == Command::INVALID) {
            waitFor = (cmd == Command::READ_REQ ?
                Command::READ_RESP: Command::WRITE_RESP);
        }
    }

    /*!
     * Reconfigures the descriptor's address range
     *\param b new address base
     *\param r new address range
     */
    void addressReconfigure(const uint64_t, const uint64_t);

    /*!
     *\brief AutoRange support
     *
     * Checks if the packet descriptor address range needs to be reset
     * - if it is set already, checks if transactions to send * size is smaller,
     * then reduces its range, else aborts
     * - if it is not set already, sets it to transactions to send * size
     * the operation aborts if the packet size generation is set to RANDOM
     *
     *\param toSend packets to send
     *\param force (optional) forces autoRange to override any existing range
     *\return the new range set
     */
    uint64_t autoRange(const uint64_t, const bool = false);


};

} // end of namespace

#endif /* __AMBA_TRAFFIC_PROFILE_PACKET_DESC_HH_ */
