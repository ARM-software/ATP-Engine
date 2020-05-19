/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: Oct 1, 2016
 *      Author: Matteo Andreozzi
 *
 */

#include <queue>
#include "kronos.hh"
#include "traffic_profile_manager.hh"
#include "logger.hh"

namespace TrafficProfiles {

Kronos::Kronos(TrafficProfileManager* const t) :
        tpm(t), bucketWidth(0), epoch(0), bucket(0),
        counter(0), initialized(false) {
}

void Kronos::init() {

    bucketWidth = tpm->getKronosConfiguration().first;

    calendar.resize(tpm->getKronosConfiguration().second / bucketWidth);

    initialized = true;

    LOG("Kronos initialised with bucket size",
            bucketWidth, "calendar buckets",
            calendar.size());
}

Kronos::~Kronos() {
}

void Kronos::schedule(const Event ev) {

    if (initialized) {
        // compute epoch and bucket
        const uint64_t quantum = ev.time / bucketWidth;
        const uint64_t bucket = quantum % calendar.size();

        // add to the calendar queue
        auto& l = calendar[bucket];
        if (!l.empty()) {
            auto pos = begin(l);
            // search for position where to insert element;
            while (pos->time < ev.time && pos != end(l))
                pos++;
            l.insert(pos, ev);
        } else {
            // insertion in empty list
            l.push_back(ev);
        }

        // update counter
        ++counter;

        LOG("Kronos::schedule event", ev, "epoch",
                quantum / calendar.size(),
                "bucket", bucket,
                "bucket size", calendar[bucket].size(),
                "total events",
                counter);
    } else {
        ERROR("Kronos::schedule Kronos uninitialized");
    }
}

void Kronos::get(list<Event>& q) {
    if (initialized) {
        // access current time
        const uint64_t& time = tpm->getTime();
        const uint64_t quantum = time / bucketWidth;
        // Lookup epoch and bucket for new TPM time
        const uint64_t target_epoch = quantum / calendar.size();
        const uint64_t target_bucket = quantum % calendar.size();
        // detect epoch change
        LOG("Kronos::get time", time, "current epoch",
                epoch, "current bucket",
                bucket, "target epoch",
                target_epoch, "target bucket",
                target_bucket);

        // access corresponding calendar bucket an
        // retrieve all events matching current epoch
        // the signed integer conversion handles wrap around
        int64_t distance = target_bucket - bucket;
        const uint64_t stop = (bucket + (distance > 0 ? distance :
                (calendar.size()-distance)));

        LOG("Kronos::get searching from", bucket,
                "to", stop%calendar.size(), "(", stop,
                ") distance:", distance);

        for (uint64_t i = bucket; i <= stop; ++i) {
            const uint64_t b = i % calendar.size();
            auto& events = calendar[b];
            while (!events.empty() && events.front().time <= time) {
                LOG("Kronos::get match found:", events.front(),
                        "in bucket",b);
                q.push_back(events.front());
                events.pop_front();
                // update the event counter
                if (counter > 0) {
                    --counter;
                } else {
                    ERROR("Kronos::schedule event counter in inconsistent state");
                }
            }
        }
        LOG("Kronos::get found", q.size(), "events triggered at time", time,
                "still active events", counter);
        // update current epoch and bucket
        uint64_t new_epoch = target_epoch;
        uint64_t new_bucket = target_bucket;

        // skip empty buckets
        while ((counter>0) && calendar.at(new_bucket).empty()) {
            if (++new_bucket >= calendar.size()) {
                new_bucket=0;
                new_epoch++;
            }
        }

        epoch = new_epoch;
        bucket = new_bucket;
        LOG("Kronos::get setting epoch to",epoch,"bucket to",bucket);
    } else {
        ERROR("Kronos::get Kronos uninitialized");
    }
}

uint64_t Kronos::next() const {
    uint64_t ret = 0;
    if (initialized) {
        bool found = false;
        if (counter > 0) {
            // next epoch events
            list<uint64_t> nextEpochEvents;

            // search for next event starting from current day/time
            for (uint64_t i = bucket; i < (bucket + calendar.size()); ++i) {
                auto& b = calendar[(i % calendar.size())];

                if (!b.empty()) {
                    const uint64_t front_epoch =
                            (b.front().time/bucketWidth)/calendar.size();
                    // check if current epoch or calendar wrapped around
                    LOG("Kronos::next found at bucket", i%calendar.size(),
                            "front epoch", front_epoch,"current",epoch);

                    if (front_epoch == epoch) {
                        // next event found
                        ret = b.front().time;
                        found=true;
                        LOG("Kronos::next - next event found:", b.front());
                        break;
                    } else {
                        nextEpochEvents.push_back(b.front().time);
                    }
                }
            }
            if (!found && !nextEpochEvents.empty()) {
                nextEpochEvents.sort();
                ret = nextEpochEvents.front();
            }

            LOG("Kronos::next - next event is at time", ret,"total events",counter);
        } else {
            LOG("Kronos::next - no events scheduled");
        }
    } else {
        ERROR("Kronos::next Kronos uninitialized");
    }
    return ret;
}

} /* namespace TrafficProfiles */
