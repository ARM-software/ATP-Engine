/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 30 Aug 2016
 *      Author: Matteo Andreozzi
 */

#include "event_manager.hh"
#include "traffic_profile_manager.hh"
#include "logger.hh"

namespace TrafficProfiles {

EventManager::EventManager(const uint64_t id,
        TrafficProfileManager* const manager):
        eventId(id), tpm(manager) {
    sent.fill(make_pair(Event::NONE, 0));
    waitedCount.fill(0);
}

EventManager::EventManager():EventManager(0, nullptr) {

}

EventManager::~EventManager() {
}

void
EventManager::reset() {
    LOG("EventManager::reset this id "
                    "[", eventId, "] reset requested");
    waited.clear();
    waitedCount.fill(0);
    sent.fill(make_pair(Event::NONE, 0));

    for (const auto& e: retainedEvents) {
        LOG("EventManager::reset this id "
                "[", eventId, "] restoring event", e);
        waitEvent(e.type, e.id);
    }
}

void
EventManager::waitEvent(const Event::Type type,
        const uint64_t id, const bool retain) {
    if (type!=Event::NONE) {
        Event ev(type, Event::AWAITED, id, tpm->getTime());
        waited[ev.id].insert(ev);
        waitedCount[Event::category[type]]++;
        if (tpm) tpm->subscribe(eventId, ev);
        if (retain) retainedEvents.push_back(ev);
        LOG("EventManager::waitEvent this id "
                "[", eventId, "] waiting for",ev,
                retain?"- event will be retained upon reset":"");
    }
}

void EventManager::emitEvent(const Event::Type type) {

    if (type!=Event::NONE && tpm) {
        // lookup category for the event type requested
        Event::Category cat = Event::category[type];

        // if the last sent event for its category is not
        // the requested one, and if either event concurrency
        // for this category is allowed or the last sent event
        // was not sent at this current time, then send it

        auto& sentCat = sent[cat];

        if ((sentCat.first == Event::NONE) ||
            (sentCat.first != type && (
            (sentCat.second < tpm->getTime()) ||
            Event::allowConcurrency[cat]))) {
            sentCat = make_pair(type,tpm->getTime());
            Event ev (type, Event::TRIGGERED, eventId, tpm->getTime());
            LOG("EventManager::emitEvent this id [", eventId, "] sent event",ev);
            tpm->event(ev);
        } else {
            LOG("EventManager::emitEvent this id [", eventId, "] "
                    "not sent event of type", Event::text[type],
                    "due to last sent", Event::text[sentCat.first],
                    "at time",sentCat.second);
        }
    }
}

bool
EventManager::receiveEvent(const Event& ev) {
    bool ok = false;

    if (ev.action!=Event::TRIGGERED) {
        ERROR("EventManager::receiveEvent this id [",
                eventId
                ,"] action is not TRIGGERED", ev);
    }

    if ((waited.find(ev.id)!=waited.end()) &&
            (waited.at(ev.id).find(ev)!=waited.at(ev.id).end())) {

        waited[ev.id].erase(ev);
        waitedCount[Event::category[ev.type]]--;
        if (waited[ev.id].empty()) {
            waited.erase(ev.id);
        }

        ok=true;
        LOG("EventManager::receiveEvent this id [", eventId, "] event",ev,
                    "received");
    }

    // special handling for TERMINATION events
    if (ev.type==Event::TERMINATION) {
        // release any event this profile might be still waiting
        // for related to the terminated event
        if (waited.count(ev.id) > 0) {
            for (auto& e:waited[ev.id]) {
                waitedCount[Event::category[e.type]]--;
            }
            waited.erase(ev.id);
            LOG("EventManager::receiveEvent this id [", eventId, "] event",ev,
                    "removed all waited events for id", ev.id);
        }

    }

    return ok;
}

} /* namespace TrafficProfiles */
