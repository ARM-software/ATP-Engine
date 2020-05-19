/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 19, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef _AMBA_TRAFFIC_PROFILE_FIFO_HH_
#define _AMBA_TRAFFIC_PROFILE_FIFO_HH_

#include <algorithm>
#include <deque>
#include <list>
#include <set>
#include "event_manager.hh"
#include "proto/tp_config.pb.h"

using namespace std;

namespace TrafficProfiles {

class TrafficProfileDescriptor;

/*!
 *\brief Implements the ATP FIFO model
 *
 * This class implements the ATP FIFO model,
 * keeps track of data either written or read from the represented device
 * buffer, and constrains such data to the size of the buffer, the maximum
 * number of outstanding transactions the modelled device can support,
 * and the data consumption rate the modelled device has
 */
class Fifo: public EventManager {
  private:

    //! Error correction precision for the ATP FIFO (fraction of ATP time unit)
    static const uint64_t errorCorrectionPrecision = 100000000;

    //! Pointer to parent TrafficProfileDescriptor object
    TrafficProfileDescriptor* profile;

    /*! FIFO type, can be either:
    *  - READ (consumes data, requests data from memory) or
    *  - WRITE (produces data, writes data to memory)
    */
    Profile::Type type;
    //! configured startup level
    uint64_t startupLevel;
    //! last time this FIFO was accessed
    uint64_t time;
    //! FIFO current data fill level
    uint64_t level;

    //! partial bytes error adjustment
    double carry;

    /*! FIFO positional time-stamp <data> tracking queue
     * the position in the queue indicates age of <data>
     * as distance from current time
     */
    deque<uint64_t> tracker;
    /*
     * Initial Tracker no-fill level:
     * takes into account the data which will
     * be served with delay zero from the initial
     * FIFO level
     */
    uint64_t initialFillLevel;
    //! configured FIFO max fill level
    uint64_t maxLevel;
    //! configured FIFO fill/depletion rate
    uint64_t rate;
    //! configured FIFO fill/depletion period
    uint64_t period;

    //! outstanding transactions tracker list
    list<uint64_t> ot;

    //! in-flight data counter
    uint64_t inFlightData;

    //! flag to signal this FIFO has been activated once
    bool firstActivation;

    //! stores the first activation time
    uint64_t firstActivationTime;

    //! flag to signal this is an ATP Linked FIFO
    bool linked;

    //! flag to signal this FIFO is active
    bool active;

    //! flag to enable the ATP tracker queue
    bool trackerEnabled;

    /*! updates the FIFO with the configured fill/depletion rate
     *\param underrun flag that signals an underrun occurred
     *\param overrun flag that signals an overrun occurred
     *\param t the current time
     */
    void update(bool&, bool&, const uint64_t);

    /*!
    * Returns whether the FIFO is empty
    *\return true if the FIFO is empty
    */
    inline bool empty() const {return (level==0);}

    /*!
    * Returns whether the FIFO is full
    *\return true if the FIFO is full
    */
    inline bool full() const {return ((level>=maxLevel) && (maxLevel!=0));}

    /*!
    * Broadcasts a FIFO status update event to
    * all subscribed profiles
    */
    void event();

    /*!
     * Returns the next transmission time
     * at which requested data can be served
     *\param data data to be served
     *\return next transmission time
     */
    uint64_t nextTransmissionTime(const uint64_t) const;

    /*!
     *\brief Stage two of the FIFO initialisation
     *
     * Sets up internal structure after the configuration
     * phase done in the init function
     */
    void setup();

  public:
    /*!
     * Getter for the current FIFO level
     *\return the value of the current FIFO level
     */
    inline const uint64_t& getLevel() const {return level;}

    /*
     * Getter for FIFO update byte,period parameters
     */
    inline pair<uint64_t,uint64_t> getRate() const { return make_pair(rate,period);}

    /*!
     * Getter for the current OT count
     *\return the value of the current OT count
     */
    inline uint64_t getOt() const {return ot.size();}

    /*!
     * Switches on the active flag
     */
    inline void activate() {active = true;}

    /*!
     * Configures the FIFO parameters
     *\param d parent TrafficProfileDescriptor
     *\param conf Protobuf FIFO configuration
     *\param t this FIFO type
     *\param e tracker queue enable flag
     */
    void init(TrafficProfileDescriptor*, const Profile::Type t,
            const FifoConfiguration*, const bool);

    /*!
     * Configures the FIFO parameters
     *\param d parent TrafficProfileDescriptor
     *\param t this FIFO type
     *\param r this FIFO fill/depletion rate
     *\param p this FIFO fill/depletion period
     *\param l this FIFO initial fill level
     *\param m this FIFO max fill level
     *\param e tracker queue enable flag
     */
    void init(TrafficProfileDescriptor*, const Profile::Type,
            const uint64_t, const uint64_t, const uint64_t, const uint64_t,
            const bool);

    /*! Attempts to send a request for data - returns true if the FIFO allows it
     *  Also causes the FIFO to call its update function
     *\param underrun flag that signals an underrun occurred
     *\param overrun flag that signals an overrun occurred
     *\param next the next time this request can be satisfied by the FIFO
     *\param request_time the time the request was originated in the FIFO
     *\param t the current time
     *\param data the amount of data the FIFO is asked to request
     *\return true if the requested data can be satisfied according to the current FIFO state
     */
    bool send(bool&, bool&, uint64_t&, uint64_t&, const uint64_t, const uint64_t);

    /*! Records a data response
     *  Also causes the FIFO to call its update function
     *\param underrun flag that signals an underrun occurred
     *\param overrun flag that signals an overrun occurred
     *\param t the current time
     *\param data the amount of data the FIFO is asked to receive
     *\return true if a whole packet was received, false otherwise
     */
    bool receive(bool&, bool&, const uint64_t, const uint64_t);

    /*!
     * Waits for an event
     *\param t waited for event type
     *\param i waited for event id
     *\param retain optional retain the event upon reset
     */
    virtual void waitEvent(const Event::Type, const uint64_t, const bool=false);

    /*!
     * Receives an event
     *\param e event to be received
     *\return true if the event was waited for
     */
    virtual bool receiveEvent(const Event&);

    //! default constructor
    Fifo();
    //! default destructor
    virtual ~Fifo();

    //! resets the FIFO to its initial state
    void reset();

    /*!
     * Returns the FIFO type
     *\return either READ or WRITE
     */
    Profile::Type getType() const { return type;}
};

} /* namespace TrafficProfiles */

#endif /* _AMBA_TRAFFIC_PROFILE_FIFO_HH_ */
