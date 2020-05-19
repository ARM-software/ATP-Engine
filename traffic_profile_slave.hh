/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 4 Aug 2016
 *      Author: Matteo Andreozzi
 */
#ifndef __AMBA_TRAFFIC_PROFILE_SLAVE_HH__
#define __AMBA_TRAFFIC_PROFILE_SLAVE_HH__

#include "traffic_profile_desc.hh"
#include "fifo.hh"
#include <list>

using namespace std;

namespace TrafficProfiles {

/*!
 *\brief ATP Traffic Profile Slave (Memory)
 *
 * Implements an ATP Slave which receives requests from
 * and sends responses to its registered master Traffic
 * Profiles. When all masters are registered to at least
 * one ATP slave, ATP can be run in standalone mode.
 */
class TrafficProfileSlave : public TrafficProfileDescriptor {
protected:
    //! type for response latency generation method
    enum Type {
        CONFIGURED = 0, RANDOM = 1
    };

    //! Memory bandwidth pair multiplier and bytes per ATP time unit
    const pair<uint64_t, uint64_t> bandwidth;
    //! Maximum outstanding transactions limit
    uint64_t maxOt;
    /*!\brief Width of the slave memory
     * transaction granularity to be taken into account
     * for OT calculation
     */
    uint64_t width;

    //! methodology for generating response latencies
    Type latencyType;

    /*!
     * union which holds either the configured static
     * memory latency
     * or a RandomGenDesc object which is used to generate
     * random latency values
     */
    union {
        struct {
            /*!
             * RandomGenDesc object used to generate
             * random latency values
             */
            Random::Generator latency;
            /*!
             * Base unit to be used to generate
             * random latency values
             */
            uint64_t latencyUnit;
        } random;
        /*!
         * Memory request to response static latency
         */
        uint64_t staticLatency;
    };

    /*!
     *\brief AMBA Traffic Profiles FIFO model
     * In slaves, the ATP FIFO is used as follows:
     * READ Type FIFO, FIFO size = width*MaxOT.
     * MaxOT = same as slave
     * On receive API call, FIFO is queried with send API,
     * data marked as "in-flight" represent requests
     * scheduled for processing
     * On send APU call FIFO, the slave looks up requests
     * which can be served, calls appropriate number of
     * receive API FIFO based on width, unlocks in-flight data
     * The FIFO rate ensures the slave bandwidth is adhered to.
     */
    Fifo fifo;

    /*!
     * Data structure used to store responses to be sent
     */
    list<Packet*> responses;
public:
    /*!
     * Constructor
     *\param manager the parent Traffic Profile Manager
     *\param index the unique ID of this Traffic Profile
     *\param p a pointer to the configuration object for this Traffic Profile
     *\param clone_num (Optional) Stream Clone number (0=original)
     */
    TrafficProfileSlave(TrafficProfileManager*, const uint64_t, const Profile*,
                        const uint64_t=0);

    //! Default destructor
    virtual ~TrafficProfileSlave();

    /*!
     *\brief returns if the Traffic Profile is active
     *
     *  If the Traffic Profile descriptor is not waiting
     *  for any events, and hasn't sent toSend data yet,
     *  then it's active. toSend == 0 means this profile
     *  doesn't have a limit on the number of transactions to send
     *  if the profile transitions from being active to inactive,
     *  it fires an event to the TPM
     *\param l returns by reference if the Traffic Profile
     *          is locked on events
     *\return true if the Traffic Profile is active, false otherwise
     */
    virtual bool active(bool&);

    /*!
     * Queries the slave for Responses
     *\param locked returns true if the slave cannot generate responses
     *\param p Response packet
     *\param next time a Response will be generated
     *\return true if the Response can be generated according to the slave OT
     *        and rate/latency limits, false otherwise.
     *        Also returns next time a Response will be
     *        available to be generated
     */
    virtual bool send(bool&, Packet*&, uint64_t&);

    /*!
     * Signals the slave to receive a Request
     * returns true if the Request can be accepted
     *\param next in case of rejection, return here next time this request
     *       can be received
     *\param packet a pointer to the ATP packet - Request
     *\param delay latency accumulated by the request
     *\return true if the request is accepted, false otherwise
     */
     virtual bool receive(uint64_t&, const Packet*, const double);

     /*!
      * Gets the slave configured bandwidth
      *\return constant reference to the bandwidth pair
      *         (bytes, period in ATP time units)
      */
     inline const pair<uint64_t, uint64_t>& getBandwidth() const {
         return bandwidth;
     }

     /*!
      * Gets the slave configured latency
      *\return constant reference to the latency value in
      *         ATP time units
      */
     inline const uint64_t& getLatency() const {return staticLatency;}

     /*!
      * Gets the slave configured width
      *\return constant reference to the slave configured width in bytes
      */
     inline const uint64_t& getWidth() const {return width;}

     /*!
      * Gets the slave configured max OT
      *\return constant reference to the slave configured max OT
      */
     inline const uint64_t& getMaxOt() const {return maxOt;}

     /*!
      * Gets next response time if available or 0
      *\return next response time if available or 0
      */
     virtual inline uint64_t nextResponseTime() const {return responses.empty()?0:responses.front()->time();}

     /*! getter method for this Traffic Profile master name
      *\return the Slave traffic profile name
      */
     virtual inline const string& getMasterName() const {return name;}

     /*!
      * Resets this profile
      */
     virtual void reset();
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_SLAVE_HH__ */
