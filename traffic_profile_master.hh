/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 4 Aug 2016
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_MASTER_HH_
#define __AMBA_TRAFFIC_PROFILE_MASTER_HH_

#include "traffic_profile_desc.hh"
#include "fifo.hh"

namespace TrafficProfiles {

/*!
 *\brief Implements the AMBA Traffic Profile master profile
 *
 * Master Profile Descriptor: tracks the status of an active Profile in terms
 * of FIFO fill level, outstanding transactions
 */
class TrafficProfileMaster: public TrafficProfileDescriptor {

protected:
    //! Configured total number of transactions to send
    uint64_t toSend;
    //! Configured time to stop
    uint64_t toStop;
    //! configured maximum number of outstanding transactions
    uint64_t maxOt;
    //! Total number of transactions sent so far
    uint64_t sent;
    //! AMBA Traffic Profiles FIFO model
    Fifo fifo;
    //! Buffer for pending Packet. If the FIFO is full, a packet could be pended here for later transmission
    Packet* pending;
    //! flag to signal checkers' FIFOs have been activated
    bool checkersFifoStarted;
    //! flag to signal the profile is halted - i.e. all operations suspended
    bool halted;
    //! AMBA TP Packet Descriptor
    PacketDesc packetDesc;

public:

    /*!
     *\brief returns if the Traffic Profile is active
     *
     *  if the Traffic Profile descriptor is not waiting for any events,
     *  and hasn't sent toSend data yet,
     *  then it's active. toSend == 0 means this profile doesn't have a
     *  limit on the number of transactions to send
     *  if the profile transitions from being active to inactive,
     *  it fires an event to the TPM
     *\param l returns by reference if the Traffic Profile is locked on events
     *\return true if the Traffic Profile is active, false otherwise
     */
    virtual bool active(bool&);

    /*!
     * Signals this Traffic Profile that an event has
     * occurred
     * it removes the event from the waiting list
     * in case of waited for event
     *\param e the event
     */
    virtual bool receiveEvent(const Event&);

    /*!
     * Signals the descriptor to send a packet
     *\param locked returns true if the profile descriptor is locked on waits
     *\param p packet to send
     *\param next time a packet will be available
     *\return true if the packet can be sent according to ot and FIFO limits,
     *        false otherwise also returns next time a packet will be
     *        available to be sent
     */
    virtual bool send(bool&, Packet*&, uint64_t&);

    /*!
     * Signals the descriptor to receive a packet
     * returns true if the packet was expected.
     *\param next in case of rejection,a profile can return here next time a packet can be received
     *\param packet a pointer to the received ATP packet
     *\param delay measured request to response delay
     *\return true if the packet was accepted by the destination Packet Descriptor, false otherwise
     */
    virtual bool receive(uint64_t&, const Packet*, const double);

    /*!
     * Signals the TPM that a packet is waiting for a response related
     * to a specific address
     *\param time the time when this request was originated
     *\param uid the packet unique ID
     *\param address the address of the waited for response
     *\param size the size of the waited for response
     */
    void wait(const uint64_t, const uint64_t, const uint64_t, const uint64_t);

    /*!
     * Signals the TPM that a response with the specified UID should
     * be discarded if received
     *\param uid the packet unique ID
     */
    void discard(const uint64_t);

    /*!
     * Signals the descriptor that this Profile no longer waits for a response
     *\param uid the packet unique ID
     *\param address the address of the response which is no longer waited for
     *\param size the size of the response which is no longer waited for
     */
    void signal(const uint64_t, const uint64_t, const uint64_t);

    /*!
     *\brief Triggers the profile descriptor autoRange
     *
     * The traffic profile descriptor is reconfigured
     * with a new address range (if no more restrictive
     * range was already set or if the force flag is set)
     * based on the configured number of packets and size.
     * If the descriptor is configured to generate
     * random packet sizes, the operation aborts
     *
     *\param force forces the range to be applied
     *\return the range applied
     */
    inline uint64_t autoRange(const bool force = false) {
        return packetDesc.autoRange(toSend, force);
    }

    /*!
     * Reconfigures the packet descriptor with a new
     * base address and range
     */
    inline void addressReconfigure(const uint64_t base, const uint64_t range)
    {
        packetDesc.addressReconfigure(base, range);
    }

    /*! Constructor
     *\param manager the parent Traffic Profile Manager
     *\param index the unique ID of this Traffic Profile
     *\param p a pointer to the configuration object for this Traffic Profile
     *\param clone_num (Optional) Stream Clone number (0=original)
     */
    TrafficProfileMaster(TrafficProfileManager*, const uint64_t,
                         const Profile*, const uint64_t=0);

    //! Default destructor
    virtual ~TrafficProfileMaster();

    /*!
     * Returns the FIFO type
     *\return either READ or WRITE
     */
    Profile::Type getFifoType() const { return fifo.getType();}

    /*! Returns the FIFO level */
    uint64_t getFifoLevel() const { return fifo.getLevel(); }

    /*!
     * Resets this profile
     */
    virtual void reset();
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_MASTER_HH_ */
