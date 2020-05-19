/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: Oct 1, 2016
 *      Author: Matteo Andreozzi
 */
#ifndef __AMBA_TRAFFIC_PROFILE_KRONOS_HH__
#define __AMBA_TRAFFIC_PROFILE_KRONOS_HH__

#include <vector>
#include <list>
#include <deque>
#include "proto/tp_packet.pb.h"
#include "event.hh"

using namespace std;

namespace TrafficProfiles {

class TrafficProfileManager;

/*!
 *\brief Kronos is the simulation engine for ATP
 *
 * Kronos schedules and triggers events in order to allow ATP
 * run in standalone mode or adaptor-driven mixed memory
 * mode, when requests go both to ATP and adaptor serviced memory
 * slaves.
 *
 * Kronos intercepts calls to send and receive API in the
 * TrafficProfileManager, of which it is an object. It matches
 * packets against ATP routing policies, then routes all packets
 * matching with internal ATP slaves to their destinations.
 *
 * If a packet is rejected by the slave, it gets scheduled to be
 * re-sent at the appropriate time. This happens by adding an
 * appropriate entry into its calendar queue. When a packet is
 * otherwise accepted by a slave, its corresponding response
 * is scheduled also in the form of a Kronos Event.
 *
 * Kronos Events are processed from the calendar queue
 * whenever a send/receive event is triggered in the ATP TPM.
 *
 *  "In the Orphic cosmogony, the unaging Chronos produced
 *  Aether and Chaos, and made a silvery egg in the divine Aether.
 *  It produced the hermaphroditic god Phanes and Hydros who gave
 *  birth to the first generation of gods and is the ultimate
 *  creator of the cosmos."
 */
class Kronos {

protected:

    //! Pointer to container ATP Manager
    TrafficProfileManager* const tpm;

    /*!
     *\brief Width of the calendar bucket
     * Duration of a time slice expressed
     * in ATP time units
     */
    uint64_t bucketWidth;

    //! the Kronos calendar queue
    vector<list<Event> > calendar;

    //! current epoch
    uint64_t epoch;

    //! current bucket
    uint64_t bucket;

    /*!
     *\brief Events counter
     * Keeps track of the number of
     * currently active events
     */
    uint64_t counter;

    //! initialization flag
    bool initialized;

public:

    /*!
     * Kronos constructor
     *\param t pointer to parent TPM object
     */
    Kronos(TrafficProfileManager* const);

    /*!
     * Initializes Kronos with parameters
     * from the TPM
     */
    void init();

    /*!
     * Kronos destructor
     */
    virtual ~Kronos();

    /*!
     *\brief Event schedule function
     *
     * Inserts an ATP Event
     * into the calendar queue
     *
     *\param ev ATP Event
     */
    void schedule(const Event);

    /*!
     * Returns Events which happen
     * at the current time
     *\param q returned by reference list of Events
     */
    void get(list<Event>&);

    /*!
     *\brief Returns next event time
     * Checks TPM time and returns the next
     * scheduled event time.
     */
    uint64_t next() const;

    /*!
     * Returns the number of active events
     *\return the value of the event counter
     */
    inline const uint64_t& getCounter() const {
        return counter;
    }

    /*!
     * Returns whether Kronos has been initialized
     *\return value of the initialized flag
     */
    inline const bool& isInitialized() const {
        return initialized;
    }
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_KRONOS_HH__ */
