/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 2, 2015
 *      Author: Matteo Andreozzi
 */

#include "packet_desc.hh"
#include "packet_tagger.hh"
#include "logger.hh"
#include "traffic_profile_desc.hh"
#include "utilities.hh"

#include <limits>

namespace TrafficProfiles {

PacketDesc::~PacketDesc() {
      if (tagger!=nullptr) delete tagger;
}

void PacketDesc::reset() {
    LOG("PacketDesc::reset",
            tpId,"reset requested");
    // reset nextAddress
    nextAddress = base;
    // reset stride
    strides = striding ? 1:0;
    strideCount = 0;
    strideStart = nextAddress;
}

void PacketDesc::init(const uint64_t parentId,
                      const PatternConfiguration& from) {
    tpId = parentId;

    bool addressOk = true, sizeOk = true;

    // command is an optional field
    if (from.has_cmd()) {
        setCommand(from.cmd());
    }

    // wait for command is an optional field
    if (from.has_wait_for()) {
        waitFor = from.wait_for();
    }

    if (from.has_size()) {
        sizeType = CONFIGURED;
        size = from.size();
    } else if (from.has_txnsize()) {
        sizeType = CONFIGURED;
        size = from.txnsize();
    }
    else if (from.has_random_size()){
        sizeType = RANDOM;
        randomSize.init(from.random_size());
    }
    else {
        LOG("PacketDesc::init index", tpId,
                "applying default data size of 64 bytes");
        // default behaviour as per ATP specs
        sizeType = CONFIGURED;
        size = 64;
    }

    if (sizeOk) {
        LOG("PacketDesc::init", tpId, "size is",
                (sizeType == CONFIGURED?"CONFIGURED":"RANDOM"));
    }

    if (from.has_address()) {
        base = from.address().base();
        if (from.address().has_increment()) {
            increment   = from.address().increment();
        } else if (sizeType == CONFIGURED) {
            increment = size;
        } else {
            ERROR ("PacketDesc::init", tpId,"no address increment configured "
                    "with RANDOM packet size generation");
        }

        if (from.address().has_range()) {
            range = Utilities::toBytes<double>(from.address().range());
        } else if (from.address().has_yrange()){
            range = Utilities::toBytes<double>(from.address().yrange());
        }
        start = from.address().start();
    }

    if (from.has_random_address()) {
        LOG("PacketDesc::init", tpId,  "address is RANDOM");
        addressType = RANDOM;
        if (from.has_address())
        {   // todo support start?
            randomAddress.init(from.random_address().type(), base, range);
        } else {
            randomAddress.init(from.random_address());
        }
    } else {
        LOG("PacketDesc::init", tpId,  "address is CONFIGURED");
        addressType = CONFIGURED;
    }

    if(!from.has_address() && !from.has_random_address()) {
        WARN("PacketDesc::init index", tpId,  "invalid packet address type configured");
        addressOk=false;
    }

    // set up packet tagger if needed
    if (from.has_lowid() || from.has_highid()){
        tagger = new PacketTagger();
        tagger->lowId = from.lowid();
        tagger->highId = from.highid();
    }

    if (from.has_alignment()) {
        alignAddresses = true;
        alignment = from.alignment();
        if (alignment > 0 && (alignment!=Utilities::nextPowerTwo(alignment))) {
            ERROR("PacketDesc::init",tpId,"configured alignment",
                    alignment,"is not a power of two");
        }
    }


    // init first value
    if (addressType == CONFIGURED) {
        // start from "start" value if available, base otherwise
        nextAddress = (start > 0 ? start : base);
        LOG("PacketDesc::init", tpId, "starting from", start > 0 ? start : base,"next", nextAddress);
    } else {
        // fetch first address from the random address sequence
        nextAddress = randomAddress.get();
    }

    // configure stride generation
    if (from.has_stride()) {
        strideN = from.stride().n();

        if (from.stride().has_increment()) {
            strideInc = from.stride().increment();
        } else if (from.stride().has_stride()) {
            strideInc = from.stride().stride();
        }

        if (from.stride().has_range()) {
            strideRange = Utilities::toBytes<double>(from.stride().range());
        } else if (from.stride().has_xrange()){
            range = Utilities::toBytes<double>(from.stride().xrange());
        }

        // signal start of first stride
        strides = 1;
        // enable striding
        striding = true;

        // set stride start
        strideStart = nextAddress;

        if (strideN>0 && strideRange>0) {
            ERROR("PacketDesc::init",tpId,"configured stride with both number"
                    " of increments",strideN,"and range",from.stride().range(),
                    "please remove one");
        }
    }

    initialized = (addressOk && sizeOk);
}

uint64_t PacketDesc::getAddress() {
    uint64_t currentAddress = nextAddress;

    if (striding && (strideCount < strideN ||
        (currentAddress+strideInc < (strideStart+strideRange)))) {
        // generate value from stride
        strideCount++;
        nextAddress = (strideStart+strideCount*strideInc);
        LOG("PacketDesc::getAddress", tpId, "next", nextAddress,
                "from strideStart", strideStart,
                "stride count", strideCount, "stride inc", strideInc);
    }  else {
        // not striding or stride limit reached
        if (addressType == CONFIGURED) {
            nextAddress  =
                // calculate either the new stride base or the next linear address
                (striding ? base+strides*increment: nextAddress+increment);
        }
        else {
            nextAddress = randomAddress.get();
        }
        if (striding) {
            // reset stride as stride limit reached
            strideCount = 0;
            strideStart = nextAddress;
            strides++;
        }
    }

    // range wrap-around support
    if (range>0 && nextAddress >= (base+range)) {

        LOG("PacketDesc::getAddress", tpId, "address",
                Utilities::toHex(nextAddress),"wrapped on base+range",
                Utilities::toHex(base+range));

        nextAddress = strideStart = base;
        // striding full reset
        strides = striding? 1:0;
        strideCount = 0;
    }

    LOG("PacketDesc::getAddress", tpId, "generating",
        (addressType == CONFIGURED ? "CONFIGURED":"RANDOM"),
        "address", Utilities::toHex(currentAddress),
        (strideCount > 0 ? "stride# " + to_string(strideCount):""),
        (strideN > 0 ? "stride max# " + to_string(strideN):""),
        (strideRange > 0 ? "stride range " + to_string(strideRange):""));

    return currentAddress;
}


uint64_t PacketDesc::getSize() {
    uint64_t ret = 0;
    if (sizeType == CONFIGURED) {
        ret = size;
    }
    else {
        ret = randomSize.get();
    }
    LOG("PacketDesc::getSize", tpId,  "generating",
        (sizeType == CONFIGURED?"CONFIGURED":"RANDOM"),
        "size", ret);
    return ret;
}


bool PacketDesc::send(Packet*& p, const uint64_t time) {
    p = nullptr;
    bool ok = false;
    if (initialized) {
        if (cmd != Command::NONE) {
            // create a new packet
            p = new Packet();
            uint64_t address = getAddress();
            uint64_t size = getSize();
            // byte-align the generated address to the packet size,
            // according to the configured alignment
            if (alignAddresses)
            {
                if (alignment>0) {
                    address &= ~(alignment-1);
                } else {
                    uint64_t toAlign = Utilities::nextPowerTwo(size);
                    // natural alignment
                    // always align to next power of two of the size
                    address &=  ~(toAlign-1);
                }
            }

            p->set_addr(address);
            p->set_size(size);
            p->set_cmd(cmd);
            p->set_time(time);
            // tag packet if needed
            if (tagger!=nullptr) {
                tagger->tag(p);
                LOG("PacketDesc::send [", tpId, "] local tagger assigned id",p->id());
            }
            LOG("PacketDesc::send [", tpId, "] new packet created [command",
                    Command_Name(cmd),
                    "] [size", p->size(), "] [address", Utilities::toHex(address),
                    "]",
                    alignAddresses ? "alignment "+ (alignment?to_string(alignment):
                            "natural"):"");

            ok = true;
        }
        else {
            LOG("PacketDesc::send ID [", tpId, "] is not configured for transmission");
        }
    } else {
        ERROR("PacketDesc::send [", tpId, "] use of uninitialised packet descriptor");
    }
    return ok;
}

bool PacketDesc::receive(const uint64_t time, const Packet* packet) {
    bool ok = false;
    if (initialized) {
        if (waitFor == packet->cmd()) {
            ok = true;
        }
        else {
            ERROR("PacketDesc::receive, waiting for",
                    Command_Name(waitFor),
                    "received unexpected packet type",
                    Command_Name(packet->cmd()),"at time ", time);
        }
    }
    else {
        ERROR("PacketDesc::receive [", tpId, "] use of uninitialised packet descriptor");
    }
    return ok;
}

void PacketDesc::addressReconfigure(const uint64_t b, const uint64_t r) {
    base = b;
    range = r;
    nextAddress = base;
    // reset stride
    strides = strideCount = 0;
    strideStart = nextAddress;

    if (addressType == RANDOM) {
        randomAddress.init(randomAddress.getType(), base, range);
    }

    LOG("PacketDesc::addressReconfigure [", tpId, "] new base set to",
            Utilities::toHex(base),"range to", range, "bytes");
}

uint64_t PacketDesc::autoRange(const uint64_t toSend, const bool force) {
    if (sizeType == RANDOM) {
        ERROR("PacketDesc::autoRange [", tpId, "] autoRange feature "
                "is not supported when RANDOM sizes are configured");
    }

    if (toSend > 0) {
        LOG("PacketDesc::autoRange [", tpId, "] to send", toSend, "force", force);

        uint64_t newRange = 0;
        if (addressType == RANDOM) {
            // in case of random address generation,
            // constrain the new range to simply the linear space
            // for all packet addresses, even if striding is set
            // as random generated addresses won't be strictly sequential
            // also: consider alignment as well for non-power of two sizes
            newRange = toSend *
                    (alignAddresses ?
                            (alignment > 0 ? alignment : Utilities::nextPowerTwo(size))
                            : size);

            LOG("PacketDesc::autoRange [", tpId, "] random configuration: "
                    "auto range", newRange);
        } else if (addressType == CONFIGURED) {
            // check if striding is enabled
            if (striding) {
                // compute packets per stride
                uint64_t stridePackets = strideRange > 0 ?
                        strideRange/strideInc : strideN;

                // compute stride range
                uint64_t strideSpace =
                        strideRange > 0 ? strideRange: strideInc * (strideN-1);

                // compute total number of strides required to send all packets
                uint64_t strides = toSend / stridePackets;

                // compute newRange as number of strides -1 * greater between stride range
                // and increment (covers the case when the new stride is inside the previous one)
                // plus "left over" packets, plus last stride space
                uint64_t leftOver = toSend % stridePackets;

                newRange = (strides - 1) * (strideSpace>increment? strideSpace:increment) // first N-1 strides
                        + strideSpace + // last stride
                        (leftOver? (leftOver * strideInc + // extra packets, not enough for another full stride
                                (increment > strideSpace ? increment - strideSpace:0)):0);


                LOG("PacketDesc::autoRange [", tpId, "] striding configuration: strides",strides,
                        "packets per stride", stridePackets,"stride space", strideSpace, "increment",
                        increment, "leftOver", leftOver, "auto range", newRange);
            } else {
                // no striding, just take into account increment
                newRange = toSend*increment;
                LOG("PacketDesc::autoRange [", tpId, "] configured configuration: increment",
                        increment, "auto range", newRange);
            }
        }
        // if no range is configured or a larger one was set, apply the new range
        if (force || range == 0 || range > newRange ) {
            if (force) {
                WARN("PacketDesc::autoRange [", tpId, "] "
                        "new range forced set to", newRange, "overrides", range);
            } else {

                LOG("PacketDesc::autoRange [", tpId, "] new range set to", newRange);
            }
            range = newRange;

            if (addressType == RANDOM) {
                // reset the random address generator range
                randomAddress.init(randomAddress.getType(), base, range);
            }
        } else {
            WARN("PacketDesc::autoRange [", tpId, "] "
                    "extending pre-existing", range, "to", newRange,"is not allowed. "
                            "You can set the force flag to override this");
        }
    } else if (range == 0){
        ERROR("PacketDesc::autoRange [", tpId, "] autoRange requested on unlimited packets "
                       "with no pre-set range");
    }

    return range;
}

} // end of namespace
