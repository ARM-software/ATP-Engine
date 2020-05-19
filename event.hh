/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: 22 Jan 2016
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_EVENT_HH__
#define __AMBA_TRAFFIC_PROFILE_EVENT_HH__

#include <functional>
#include <iostream>

using namespace std;

namespace TrafficProfiles {
/*!
 *\brief AMBA Traffic Profile Event
 *
 * An ATP Event can be triggered by a Profile and
 * is propagated by the ATP Manager to all other
 * Profiles subscribed to it
 */
class Event {
private:
    //! Event configuration string separator
    static const string separator;

public:

    /*! ATP Event Categories by which
     *  Events are grouped
     */
    enum Category {
        NO_CATEGORY = 0,
        PROFILE = 1,
        FIFO_LEVEL = 2,
        SEND_STATUS = 3,
        PACKET = 4,
        CLOCK = 5,
        N_CATEGORIES = 6
    };

    /*!
     * Event Type
     * NONE: an empty event
     * ACTIVATION: a profile is active for the first time
     * TERMINATION : a profile has issued all its transactions
     * FIFO_EMPTY : a profile FIFO has become empty
     * FIFO_FULL  : a profile FIFO has become full
     * FIFO_NOT_EMPTY : a FIFO becomes non empty after being empty
     * FIFO_NOT_FULL : a FIFO becomes non full after being full
     * PROFILE_LOCKED : a profile is unable to send data
     * PROFILE_UNLOCKED: a profile is able to send data after being unable
     * PACKET_REQUEST_RETRY: a rejected request is due to be retried
     * TICK: special clock event, triggers TPM tick
     */
    const enum Type {
        NONE = 0,
        ACTIVATION = 1,
        TERMINATION = 2,
        FIFO_EMPTY  = 3,
        FIFO_FULL   = 4,
        FIFO_NOT_EMPTY = 5,
        FIFO_NOT_FULL = 6,
        PROFILE_LOCKED = 7,
        PROFILE_UNLOCKED = 8,
        PACKET_REQUEST_RETRY = 9,
        TICK = 10,
        N_EVENTS = 11
    } type;

    /*!
     * Action Type
     * AWAITED   = event waited for
     * TRIGGERED = event occurred
     */
    const enum Action {
        AWAITED = 0,
        TRIGGERED = 1
    } action;

    /*!
     * Event type to category map
     */
    static const Category category[];

    /*!
     * Event type to string name map
     */
    static const string text[];

    /*!
     * Event category to allow concurrency flag map
     */
    static const bool allowConcurrency[];

    /*!
     *  Unique ID
     */
    const uint64_t id;

    /*!
     * Event time
     */
    const uint64_t time;

public:

    /*!
     * Parses a string into Event Type and Profile name
     *\param t returned Event Type
     *\param profile returned Profile name
     *\param in input string
     *\return true if both type and id are successfully
     *        parsed, false otherwise
     */
    static bool parse(Type&, string&, const string&);

    /*!
     * Event Constructor
     *\param ty the type of the Event
     *\param a the Event action
     *\param eId this event ID
     *\param t Event time
     */
    Event(const Type, const Action, const uint64_t, const uint64_t);

    //! Default destructor
    virtual ~Event();

    /*!
     * Equality operator
     *\param ev1 the first Event to be compared
     *\param ev2 the second Event to be compared
     *\return true if the events compare equal
     */
    inline friend bool operator==(const Event& ev1, const Event& ev2) {
        return (ev1.type == ev2.type && ev1.id == ev2.id);
    }

    /*!
     * Inequality operator
     *\param ev1 the first Event to be compared
     *\param ev2 the second Event to be compared
     *\return true if the events compare equal
     */
    inline friend bool operator!=(const Event& ev1, const Event& ev2) {
       return (ev1.type != ev2.type || ev1.id != ev2.id);
    }

    /*!
     * Ostream operator
     *\param os the ostream where to output the event
     *\param ev the event to be output to ostream
     *\return reference to the modified ostream
     */
    friend ostream& operator<<(ostream& os, const Event& ev);
};

} /* namespace TrafficProfiles */

//! the C++ standard template library namespace
namespace std {
/*!\brief ATP Event Hash function
 *
 * Computes the hash value of an ATP event
 */
template<>
struct hash<TrafficProfiles::Event> {
  size_t operator()(const TrafficProfiles::Event& object) const {
    return hash<uint64_t>()((object.type + 1) * object.id);
  }
};
}

#endif /* __AMBA_TRAFFIC_PROFILE_EVENT_HH__ */
