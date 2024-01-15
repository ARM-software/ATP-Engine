/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#include "gem5/atp_device.hh"

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/ATP.hh"
#include "enums/ByteOrder.hh"
#include "gem5/profile_gen.hh"

// See same import statement in profile_gen.cc for rationale
using namespace gem5;

namespace ATP {

Device::Device(const Params &p)
    : AmbaDmaDevice(p, 0x1000), adapter(p.adapter),
      atpId(p.atp_id), sid(p.sid), ssid(p.ssid),
      streamNameRead([this]{ streamNameHandler(); }, name()) { }

AddrRangeList
Device::getAddrRanges() const {
    return { RangeSize(pioAddr, pioSize) };
}

Tick
Device::read(PacketPtr pkt) {
    const Addr addr{ pkt->getAddr() - pioAddr };
    const unsigned size{ pkt->getSize() };

    uint64_t resp{ };
    switch (addr) {
        case STREAM_NAME_BASE:
        case STREAM_NAME_RANGE:
        case READ_BASE:
        case READ_RANGE:
        case WRITE_BASE:
        case WRITE_RANGE:
        case TASK_ID:
        case IN_REQUEST_ID:
        case CONTROL:
            regs.writeOnly(addr);
            break;
        case STREAM_ID:
            if (size != 8) regs.invalidSize(addr, size);
            resp = regs.streamId;
            break;
        case OUT_REQUEST_ID:
            if (size != 4) regs.invalidSize(addr, size);
            resp = regs.outRequestId;
            break;
        case STATUS:
            if (size != 1) regs.invalidSize(addr, size);
            resp = regs.status;
            break;
        default:
            regs.unexpectedAddr(addr);
    }

    DPRINTF(ATP, "ATP::Device::%s: 0x%llx<-0x%llx(%u)\n", __func__, resp,
            addr, size);

    pkt->setUintX(resp, ByteOrder::little);
    pkt->makeResponse();
    return pioDelay;
}

Tick
Device::write(PacketPtr pkt) {
    const Addr addr{ pkt->getAddr() - pioAddr };
    const unsigned size{ pkt->getSize() };

    uint64_t data{ pkt->getUintX(ByteOrder::little) };
    switch (addr) {
        case OUT_REQUEST_ID:
            regs.readOnly(addr);
            break;
        case STREAM_NAME_BASE:
            if (size != 8) regs.invalidSize(addr, size);
            regs.streamNameBase = data;
            break;
        case STREAM_NAME_RANGE:
            if (size != 8) regs.invalidSize(addr, size);
            regs.streamNameRange = data;
            break;
        case READ_BASE:
            if (size != 8) regs.invalidSize(addr, size);
            regs.readBase = data;
            break;
        case READ_RANGE:
            if (size != 8) regs.invalidSize(addr, size);
            regs.readRange = data;
            break;
        case WRITE_BASE:
            if (size != 8) regs.invalidSize(addr, size);
            regs.writeBase = data;
            break;
        case WRITE_RANGE:
            if (size != 8) regs.invalidSize(addr, size);
            regs.writeRange = data;
            break;
        case STREAM_ID:
            if (size != 8) regs.invalidSize(addr, size);
            regs.streamId = data;
            break;
        case TASK_ID:
            if (size != 4) regs.invalidSize(addr, size);
            regs.taskId = data;
            break;
        case IN_REQUEST_ID:
            if (size != 4) regs.invalidSize(addr, size);
            regs.inRequestId = data;
            break;
        case CONTROL:
            if (size != 1) regs.invalidSize(addr, size);
            regs.control = data;
            if (regs.control.test(2) && !regs.control.test(3)) {
                playStreamHandler();
            } else if (!regs.control.test(2) && regs.control.test(3)) {
                intAckHandler();
            } else if (regs.control.test(2) && regs.control.test(3)) {
                panic_if(dmaPending(), "ATP::Device::%s: Unique "
                         "Stream already active, improper locking");
                uniqueStreamHandler();
            }
            break;
        case STATUS:
            if (size != 1) regs.invalidSize(addr, size);
            warn_if(data != 0, "ATP::Device::%s: Received non-zero "
                    "value on STATUS reset, set to zero", __func__);
            regs.status = 0;
            break;
        default:
            regs.unexpectedAddr(addr);
    }

    DPRINTF(ATP, "ATP::Device::%s: 0x%llx->0x%llx(%u)\n", __func__, data,
            addr, size);

    pkt->makeResponse();
    return pioDelay;
}

void
Device::playStreamHandler() {
    fatal_if(streams.find(regs.streamId) == streams.end(), "ATP::Device::%s: "
             "Unknown Stream ID %llu", __func__, regs.streamId);
    DPRINTF(ATP, "ATP::Device::%s: stream %llu, request %u\n", __func__,
            regs.streamId, regs.inRequestId);

    Stream &str{ streams.at(regs.streamId) };
    Request req{ regs.readBase, regs.readRange, regs.writeBase,
                 regs.writeRange, regs.inRequestId, regs.taskId,
                 extractDmaOp() };
    str.active ? str.pendingRequests.push(req) :
                 serveRequest(regs.streamId, req);
    regs.status = 1;
}

Device::DmaOperation
Device::extractDmaOp() const {
    if (!regs.control.test(0) && !regs.control.test(1))
        return DmaOperation::READ;
    else if (regs.control.test(0) && !regs.control.test(1))
        return DmaOperation::WRITE;
    else if (!regs.control.test(0) && regs.control.test(1))
        return DmaOperation::RDWR;
    else
        fatal("ATP::Device::%s: Invalid DMA operation in CONTROL", __func__);
}

void
Device::serveRequest(const uint64_t str_id, const Request &req) {
    Stream &str{ streams.at(str_id) };
    configureDmaOp(str_id, req);
    const uint32_t req_id{ req.id }, task_id{ req.taskId };
    adapter->activateStream(str_id,
        [this, str_id, req_id]() { servedRequestHandler(str_id, req_id); },
        [this, task_id](RequestPtr req) { annotateRequest(req, task_id); });
    str.active = true;
}

void
Device::configureDmaOp(const uint64_t str_id, const Request &req) {
    if (req.dmaOp == DmaOperation::READ || req.dmaOp == DmaOperation::RDWR)
        adapter->configureStream(str_id, req.readBase, req.readRange,
                                 TrafficProfiles::Profile::READ);
    if (req.dmaOp == DmaOperation::WRITE || req.dmaOp == DmaOperation::RDWR)
        adapter->configureStream(str_id, req.writeBase, req.writeRange,
                                 TrafficProfiles::Profile::WRITE);
}

void
Device::servedRequestHandler(const uint64_t str_id, const uint32_t req_id) {
    DPRINTF(ATP, "ATP::Device::%s: stream %llu, request %u\n", __func__,
            str_id, req_id);

    Stream &str{ streams.at(str_id) };
    str.active = false;
    !intAckPending && servedRequests.empty() ? notifyServedRequest(req_id) :
                                               servedRequests.push(req_id);
    if (!str.pendingRequests.empty()) {
        serveRequest(str_id, str.pendingRequests.front());
        str.pendingRequests.pop();
    }
}

void
Device::annotateRequest(RequestPtr req, const uint32_t task_id) {
    req->setStreamId(sid);
    req->setSubstreamId(ssid);
    req->taskId(task_id);
}

void
Device::intAckHandler() {
    DPRINTF(ATP, "ATP::Device::%s\n", __func__);

    interrupt->clear();
    intAckPending = false;
    if (!servedRequests.empty()) {
        notifyServedRequest(servedRequests.front());
        servedRequests.pop();
    }
}

void
Device::notifyServedRequest(const uint32_t req_id) {
    regs.outRequestId = req_id;
    interrupt->raise();
    intAckPending = true;
}

void
Device::uniqueStreamHandler() {
    DPRINTF(ATP, "ATP::Device::%s: base 0x%llx, range %llu\n", __func__,
            regs.streamNameBase, regs.streamNameRange);

    streamNameBuffer.resize(regs.streamNameRange);
    dmaRead(regs.streamNameBase, regs.streamNameRange, &streamNameRead,
            streamNameBuffer.data());
}

void
Device::streamNameHandler() {
    std::string str_name{ streamNameBuffer.begin(), streamNameBuffer.end() };
    regs.streamId = adapter->uniqueStream(atpId, str_name);

    DPRINTF(ATP, "ATP::Device::%s: name %s, id %llu\n", __func__, str_name,
            regs.streamId);

    streams.emplace(regs.streamId, Stream{ });
    regs.status = 1;
    streamNameBuffer.clear();
}

const std::unordered_map<Addr, const char *> Device::Registers::names{
    { STREAM_NAME_BASE,  "StreamNameBase"  },
    { STREAM_NAME_RANGE, "StreamNameRange" },
    { READ_BASE,         "ReadBase"        },
    { READ_RANGE,        "ReadRange"       },
    { WRITE_BASE,        "WriteBase"       },
    { WRITE_RANGE,       "WriteRange"      },
    { STREAM_ID,         "StreamId"        },
    { TASK_ID,           "TaskId"          },
    { IN_REQUEST_ID,     "InRequestId"     },
    { OUT_REQUEST_ID,    "OutRequestId"    },
    { CONTROL,           "Control"         },
    { STATUS,            "Status"          }
};

void
Device::Registers::readOnly(const Addr addr) const {
    warn("ATP::Device::Registers::%s: RO reg (0x%llx) [%s]", __func__, addr,
         names.at(addr));
}

void
Device::Registers::writeOnly(const Addr addr) const {
    warn("ATP::Device::Registers::%s: WO reg (0x%llx) [%s]", __func__, addr,
         names.at(addr));
}

void
Device::Registers::invalidSize(const Addr addr, const unsigned size) const {
    panic("ATP::Device::Registers::%s: Invalid size %u [%s]", __func__, size,
          names.at(addr));
}

void
Device::Registers::unexpectedAddr(const Addr addr) const {
    warn("ATP::Device::Registers::%s: Unexpected address %llx, RAZ/WI access",
         __func__, addr);
}

} // namespace ATP
