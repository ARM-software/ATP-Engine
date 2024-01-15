/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 4 Aug 2016
 *      Author: Matteo Andreozzi
 */

#include "traffic_profile_master.hh"
#include "traffic_profile_manager.hh"
#include "traffic_profile_checker.hh"
#include "types.hh"
#include "utilities.hh"

namespace TrafficProfiles {

TrafficProfileMaster::TrafficProfileMaster(TrafficProfileManager* manager,
        const uint64_t index, const Profile* p, const uint64_t clone_num):
        TrafficProfileDescriptor (manager, index, p, clone_num),
        toSend(0), toStop(0), maxOt(1),
        sent(0), pending(nullptr),
        checkersFifoStarted(false),
        halted (false) {

        // Configure the packet descriptor
        if (p->has_pattern()) {
            LOG("TrafficProfileMaster [", this->name,
                    "] Initialising pattern descriptor");
            packetDesc.init(id, p->pattern(), this->packetTagger);
            // configure packet descriptor command if not done
            // so in the Pattern Section
            if (packetDesc.command() == Command::INVALID) {
                packetDesc.setCommand(type == Profile::READ ?
                        Command::READ_REQ : Command::WRITE_REQ);
            }
        } else {
            ERROR("TrafficProfileMaster [", this->name,
                    "] missing pattern descriptor configuration");
        }

        // initialise total transactions / frame size / frame time
        // total number of transactions overrides any other parameter
        if (p->fifo().has_total_txn()) {
            toSend = p->fifo().total_txn();
        } else if (p->fifo().has_framesize()) {
            if (packetDesc.getSizeType() == PacketDesc::CONFIGURED) {
                toSend = Utilities::toBytes<uint64_t>(p->fifo().framesize())/
                        packetDesc.getPacketSize();
            } else {
                ERROR("TrafficProfileMaster [", this->name,
                    "] FrameSize configuration is incompatible with random packet size");
                toSend = 0;
            }
        } else if (p->fifo().has_frametime()) {
            toStop = parseTime(p->fifo().frametime());
        }

        // set maximum OT or leave default value 1
        if (p->fifo().has_ot_limit()) {
            maxOt = p->fifo().ot_limit();
        } else if (p->fifo().has_txnlimit()) {
            maxOt = p->fifo().txnlimit();
        }

        if (p->has_fifo() && p->has_type()) {
            // Initialise the FIFO
            fifo.init(this, type, &p->fifo(),
                    manager->isTrackerLatencyEnabled());
        } else {
            ERROR("TrafficProfileMaster [", this->name,
                    "] FIFO configuration not found");
        }
        // set role
        role = MASTER;

        // signal profile first activation if not locked by other profiles
        if (getWaitedCount(Event::PROFILE)==0) {
            activate();
        }
        LOG("TrafficProfileMaster::TrafficProfileMaster [", this->name,
                    "] initialised profile type", Profile::Type_Name(type),
                    "to send", toSend, "to stop", toStop, "max OT", maxOt);
}

TrafficProfileMaster::~TrafficProfileMaster() {
    // deallocate any FIFO-generated, unsent request
    if (pending != nullptr) {
        delete pending;
    }
}

void TrafficProfileMaster::reset() {
    TrafficProfileDescriptor::reset();
    LOG("TrafficProfileMaster::reset "
            "[", this->name, "] requested reset");
    // reset the FIFO
    fifo.reset();
    // reset the packet descriptor
    packetDesc.reset();
    // reset sent packets
    sent = 0;
    // delete any pending packet
    if (pending!=nullptr) delete pending;
    halted = false;
    // reset all assigned checkers
    checkersFifoStarted = false;
    for (auto& c:checkers) {
        tpm->getProfile(c)->reset();
    }
    // signal profile first activation if not locked by other profiles
    if (getWaitedCount(Event::PROFILE)==0) {
        emitEvent(Event::ACTIVATION);
        started=true;
    }
}

bool TrafficProfileMaster::receiveEvent(const Event& e) {
    LOG("TrafficProfileMaster::receiveEvent [", this->name, "] Event", e);

    bool ok = false, fifoOk=false;
    // super class handle
    ok = EventManager::receiveEvent(e);
    // check if it's a halt event
    if (ok && Event::category[e.type]==Event::SEND_STATUS) {
        if (e.type == Event::PROFILE_LOCKED) {
            waitEvent(Event::PROFILE_UNLOCKED,e.id);
        } else if (e.type == Event::PROFILE_UNLOCKED) {
            waitEvent(Event::PROFILE_LOCKED,e.id);
        }
        halted = !halted;
        LOG("TrafficProfileMaster::receiveEvent [", this->name,
                "] switching status to",halted?"halted":"not halted");
    }

    if (getWaitedCount(Event::SEND_STATUS)==0) {
        //all send_status events released
        halted = false;
        LOG("TrafficProfileMaster::receiveEvent [", this->name,
                       "] switching status to not halted");
    }

    // signal profile first activation if not locked by other profiles
    if (!started && getWaitedCount(Event::PROFILE)==0) {
        activate();
    }

    // forward to FIFO
    // Fifo Handle
    fifoOk = fifo.receiveEvent(e);

    return ok || fifoOk;
}

bool TrafficProfileMaster::send(bool& locked, Packet*& p, uint64_t& next) {
    // get current time
    const uint64_t& t = tpm->getTime();
    // reset next transmission time
    next = 0;
    locked = false;
    bool ok = false;
    // reset returned packet
    p = nullptr;

    if (active(locked)) {
        bool underrun = false, overrun = false;
        uint64_t request_time = 0;

        // check if there's a packet pending
        if (pending == nullptr) {
            LOG("TrafficProfileMaster::send [", this->name,
                    "] no pending packet found, requesting next to packet descriptor");
            // request packet to descriptor
            if (packetDesc.send(pending, t)) {
                LOG("TrafficProfileMaster::send [", this->name,
                        "] packet generated by packet descriptor");
            }
        } else {
            // update pending packet time
            pending->set_time(t);
        }
        // if there's a pending packet, check if it can be send
        if (pending != nullptr) {
            // check FIFO space
            if (fifo.send(underrun, overrun, next, request_time, t, pending->size())) {
                // tag packet with masterId, streamId and masterIommuId
                pending->set_master_id(masterName);
                // request TPM to tag this packet
                tpm->tag(pending);
                // request Packet Tagger to tag this packet with profile metadata
                // if tagger is available
                if (this->packetTagger != nullptr)
                    this->packetTagger->tagPacket(pending);

                // update number of outstanding transaction if needed
                if (packetDesc.waitingFor() != Command::NONE) {
                    ot++;
                    // wait for this address/command - pass time here.
                    wait(request_time,  pending->uid(),
                            pending->addr(), pending->size());
                } else {
                    // trigger FIFO receive straight away: data does
                    // not require acknowledgement
                    fifo.receive(underrun, overrun, t, pending->size());

                    // updating stats not needed as it's been done already at time t
                    // when send was called

                    // inform the TPM that  responses should be discarded
                    discard(pending->uid());
                }

                //  OK
                LOG("TrafficProfileMaster::send [", this->name,
                        "] packet generated with address",
                        Utilities::toHex(pending->addr()),
                        "current ot", ot);
                ok = true;

                // transmit from pending, and clear buffer
                p = pending;
                pending = nullptr;

            } else {
                LOG("TrafficProfileMaster::send [", this->name,
                        "] next packet available time adjusted to", next,
                        "due to FIFO limitation");
            }
            // check if there are monitors assigned and init their FIFOs
            if (!checkersFifoStarted) {
                for (auto& c:checkers) {
                    LOG("TrafficProfileMaster::send [", this->name,
                            "] activating FIFO for checker profile",
                            tpm->getProfile(c)->getName());
                    // this is guaranteed to be a checker, so we can dynamic cast
                    dynamic_cast<TrafficProfileChecker*>(tpm->getProfile(c))->activateFifo();
                }
                checkersFifoStarted = true;
            }
        } else {
            LOG("TrafficProfileMaster::send [", this->name,
                    "] no available packets to send");
        }

        // if a packet has been sent/recorded
        if (p!=nullptr) {
            // update number of transactions sent
            sent++;
            // update statistics
            stats.send(t, p->size(), ot);
            if ((sent > toSend) && (toSend > 0)) {
                ERROR("TrafficProfileMaster::send [", this->name,
                    "] max send threshold",toSend," breached:",sent);
            }
        }
        else {
            // start statistics if needed
            stats.start(t);
        }
        // updates FIFO stats
        stats.fifoUpdate(fifo.getLevel(),underrun, overrun);

    } else {
        LOG("TrafficProfileMaster::send [", this->name, "] is not active",
                locked ? "it is locked" : "it's terminated");
        // return FIFO period as next time if the profile is locked
        // waiting for events
        if (!locked && !halted && !terminated)
        {
            next = t + fifo.getRate().second;
        }
    }

    if (!terminated) {
        if (ok) {
            emitEvent(Event::PROFILE_UNLOCKED);
        } else {
            emitEvent(Event::PROFILE_LOCKED);
        }
    }
    return ok;
}

bool TrafficProfileMaster::receive(
        uint64_t& next,
        const Packet* packet, const double delay) {

    bool underrun = false, overrun = false, ok = false, whole = false;
    next = 0;
    // get current time
    const uint64_t& t = tpm->getTime();

    if (packetDesc.receive(t, packet)) {
        // update FIFO
        whole = fifo.receive(underrun, overrun, t, packet->size());
        // update statistics
        stats.receive(t, packet->size(), delay);
        // reduce number of outstanding transactions
        if (whole) {
            if (0 == ot) {
            ERROR("TrafficProfileMaster::receive [", this->name,
                    "] address ", Utilities::toHex(packet->addr()),
                    "negative OT detected at time", t, "stats",
                    stats.dump());
            }
            ot--;
            // signal reception
            signal(packet->uid(), packet->addr(), packet->size());
        }
        LOG("TrafficProfileMaster::receive [", this->name, "] address",
                Utilities::toHex(packet->addr()), "received packet at time", t,
                "with latency",delay,"current ot", ot);
        ok=true;
    } else {
        LOG("TrafficProfileMaster::receive [", this->name, "] unexpected packet "
                "received of type", Command_Name(packet->cmd()) ,"address ",
                Utilities::toHex(packet->addr()));
    }

    // updates FIFO stats
    stats.fifoUpdate(fifo.getLevel(), underrun, overrun);

    // check if the profile is still active (triggers termination event)
    bool locked = false;
    if (!active(locked) && !locked) {
        LOG("TrafficProfileMaster::receive [", this->name, "] terminated");
    } else {
        // return current time as next time
        next = t;
    }
    return ok;
}


void TrafficProfileMaster::wait(
        const uint64_t time,
        const uint64_t uid,
        const uint64_t address,
        const uint64_t size) {
    LOG("TrafficProfileMaster::wait [", name, "] UID",
            uid, "address",
            Utilities::toHex(address), "size", size);
    tpm->wait(id, time, uid, TrafficProfileManager::RESPONSE);
}

void TrafficProfileMaster::discard(const uint64_t uid) {
      tpm->wait(InvalidId<uint64_t>(), tpm->getTime(), uid,
                TrafficProfileManager::RESPONSE);
}

void TrafficProfileMaster::signal(
        const uint64_t uid,
        const uint64_t address,
        const uint64_t size) {
    LOG("TrafficProfileMaster::signal [", name, "] UID", uid,
            "address", Utilities::toHex(address), "size", size);

    tpm->signal(id, uid, TrafficProfileManager::RESPONSE);
}

bool TrafficProfileMaster::active(bool& l) {
    bool isActive = true;

    // this profile is active if it isn't waiting for activation events,
    // and if it still has data to send (or if toSend is set to 0)
    // also, a profile is locked if it has sent all of its data but still is waiting
    // for responses (OT>0)

    // if a profile has sent all its data but it's waiting for responses is locked,
    // this condition prevents termination (toSend == 0 means infinite data to send)
    const bool endOfData = (sent == toSend) && (toSend > 0);
    const bool endOfTime = started && ((toStop > 0) && (tpm->getTime() >= startTime+toStop));
    const bool waitingForResponses = (endOfData || endOfTime) && (ot > 0);
    LOG("TrafficProfileMaster::active [", name, "] started:",started,
            "startTime", startTime,"stop time", startTime+toStop,
            "to send",toSend,"sent",sent, "end of Data:",endOfData,
            "end of Time", endOfTime, "waited responses",waitingForResponses);

    if ((endOfData || endOfTime) && !(terminated || waitingForResponses)) {
        // fire deactivation event
        emitEvent(Event::TERMINATION);
        LOG("TrafficProfileMaster::active [", name,
                "] firing deactivation event with id", id);
        terminated = true;
        // clear the halted flag
        halted = false;
    }

    // if not terminated, a profile is locked if:
    // 1) waiting for other profiles to terminate
    // 2) halted - waiting for an unlock event
    // 3) max OT reached and waiting for responses
    // 4) max data sent and waiting for responses

    const bool maxOtReached = ((ot >= maxOt) && (maxOt > 0));
    const uint64_t waitedProfileEvents = getWaitedCount(Event::PROFILE);
    l = !terminated
            && ((waitedProfileEvents > 0) || (halted)
                    || maxOtReached || waitingForResponses);

    // a profile is active if not locked and if has not finished sending its data
    isActive = !(l || endOfData || endOfTime);

    // signal profile first activation
    if (isActive && !started) {
        activate();
    }

    LOG("TrafficProfileMaster::active [", name, "]",
            (terminated ?
                    "terminated" :
                    (isActive ?
                            "is active" : (l ? "is locked" : "is not active"))),
            halted ? "is halted" : "is not halted", "waited PROFILE events",
                    waitedProfileEvents, "OT", ot, "Max", maxOt, "sent",
            sent, "to send", toSend, "toStop", toStop,"current time",tpm->getTime());

    return isActive;
}


} /* namespace TrafficProfiles */
