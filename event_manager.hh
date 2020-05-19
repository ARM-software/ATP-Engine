/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 30 Aug 2016
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_EVENT_MANAGER_HH__
#define __AMBA_TRAFFIC_PROFILE_EVENT_MANAGER_HH__

#include <array>
#include <list>
#include <map>
#include <set>
#include <unordered_set>
#include "event.hh"

using namespace std;

namespace TrafficProfiles {

class TrafficProfileManager;

/*!
 *\brief ATP Event Manager
 *
 * An ATP Event Manager object supports generation and
 * reception of ATP Events.
 */
class EventManager {

protected:

    /*!
     * ID of the generated events
     */
    uint64_t eventId;

    /*!
     * Pointer to the TPM
     */
    TrafficProfileManager* tpm;

    /*!
     * Last sent event and time per Category
     */
    array<pair<Event::Type, uint64_t>, Event::N_CATEGORIES> sent;

    /*!
     * Map: event ID, Events waited for with
     * that ID
     */
    map<uint64_t, unordered_set<Event>> waited;

    /*!
     * Number of events waited for per category
     */
    array<uint64_t, Event::N_CATEGORIES> waitedCount;

    /*!
     *\brief list of events to be retained upon reset
     *
     * This list is populated with events on a wait invoked
     * with the retain flag set
     * Upon reset, this list is parsed and the events contained
     * in it are waited for.
     */
    list<Event> retainedEvents;

public:
    /*!
     * Default constructor
     */
    EventManager();

    /*!
     * Constructor
     *\param id ID of the generated events
     *\param manager pointer to the TPM
     */
    EventManager(const uint64_t, TrafficProfileManager* const);

    //! Default Destructor
    virtual ~EventManager();

    /*!
     *\brief reset support
     *
     * restores the EventManager
     * to its initial state, and
     * reinstates retained events
     */
    virtual void reset();

    /*!
     * Set the ID with which events will be emitted
     *\param id event id to be set
     */
    inline void setEventId(const uint64_t id) {
        eventId = id;
    }
    ;

    /*!
     * Set the TPM to which events will be emitted
     * and registered
     *\param manager pointer to the TPM
     */
    inline void setTpm(TrafficProfileManager * const manager) {
        tpm = manager;
    }
    ;

    /*!
     * Waits for an event
     *\param type waited for event type
     *\param id waited for event id
     *\param retain flag - optional - indicates that
     * the wait shall be restored upon a reset
     */
    virtual void waitEvent(const Event::Type,
            const uint64_t, const bool=false);

    /*!
     * Receives an event
     *\param ev event to be received
     *\return true if the event was waited for
     */
    virtual bool receiveEvent(const Event&);

    /*!
     * Returns a constant reference to the waited events set
     *\return constant reference to waited events set
     */
    virtual const map<uint64_t, unordered_set<Event>>&
        getWaited() const {return waited;}

    /*!
     * Builds and sends an event
     *\param type of the event to be sent
     */
    void emitEvent(const Event::Type);

    /*!
     * Returns if the EventManager is waiting
     * for events
     *\return true if waiting for any events
     */
    inline bool waiting() const {
        return !waited.empty();
    }

    /*!
     * Returns how many events per category are waited for
     *\param c Event Category
     *\return the number of events waited for of the specified
     *        category
     */
    inline uint64_t getWaitedCount(const Event::Category c) {
        return waitedCount[c];
    }
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_EVENT_MANAGER_HH__ */
