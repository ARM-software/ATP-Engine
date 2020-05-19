/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 * Created on: 4 Aug 2016
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_DELAY_HH__
#define __AMBA_TRAFFIC_PROFILE_DELAY_HH__

#include "traffic_profile_desc.hh"

namespace TrafficProfiles {

/*!
 *\brief Traffic Profile Delay implementation
 *
 * A Traffic Profile Delay implements a delay block,
 * it can be utilised to force a pause between two
 * traffic profiles. Its behaviour is to stay active
 * for the configured amount of time, sending no packets
 * and accepting no responses. At the of the configured
 * delay, it would terminate.
 */
class TrafficProfileDelay: public TrafficProfileDescriptor {

protected:
    //! configured delay in ATP time units
    const uint64_t delay;

    //! field to record activation time
    uint64_t startTime;
    //! field to record current time
    uint64_t time;

public:
    /*!
     * Constructor
     *\param manager the parent Traffic Profile Manager
     *\param index the unique ID of this Traffic Profile
     *\param p a pointer to the configuration object for this Traffic Profile
     *\param clone_num (Optional) Stream Clone number (0=original)
     */
    TrafficProfileDelay(TrafficProfileManager*, const uint64_t, const Profile*,
                        const uint64_t=0);

    //! Default destructor
    virtual ~TrafficProfileDelay();

    /*!
     * Signals this Traffic Profile that an event has
     * occurred
     * it removes the event from the waiting list
     * in case of waited for event
     *\param e the event
     */
    virtual bool receiveEvent(const Event&);

    /*!
     *
     *\brief returns if the Traffic Profile is active
     *
     *  if the Traffic Profile descriptor is not waiting
     *  for any events, and hasn't sent toSend data yet,
     *  then it's active. toSend == 0 means this profile
     *  doesn't have a limit on the number of transactions to send
     *  if the profile transitions from being active to inactive,
     *  it fires an event to the TPM
     *\param l returns by reference if the Traffic Profile is locked on events
     *\return true if the Traffic Profile is active, false otherwise
     */
    virtual bool active(bool&);

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
     * Resets this profile
     */
    virtual void reset();
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_DELAY_HH__ */
