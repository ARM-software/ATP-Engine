/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2019-2020 ARM Limited
 * All rights reserved
 * Authors: Adrian Herrera
 */

#ifndef __ATP_DEVICE_HH__
#define __ATP_DEVICE_HH__

#include <bitset>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/addr_range.hh"
#include "base/types.hh"
#include "dev/arm/amba_device.hh"
#include "mem/packet.hh"
#include "params/ATPDevice.hh"
#include "sim/eventq.hh"

class ProfileGen;

namespace ATP {

//! AMBA Adaptive Traffic Profiles (ATP) device
/*!
 * Generic ATP device. Provides MMIO programmability and ATP APIs
 * access from system software.
 */
class Device : public gem5::AmbaDmaDevice {
  public:
    PARAMS(gem5::ATPDevice);
    Device(const Params &p);

  protected:
    //! MMIO registers offsets
    enum Offset : gem5::Addr {
        //! Stream name address base
        STREAM_NAME_BASE  = 0x00,
        //! Stream name address range
        STREAM_NAME_RANGE = 0x08,
        //! Read DMA buffer address base
        READ_BASE         = 0x10,
        //! Read DMA buffer address range
        READ_RANGE        = 0x18,
        //! Write DMA buffer address base
        WRITE_BASE        = 0x20,
        //! Write DMA buffer address range
        WRITE_RANGE       = 0x28,
        //! Stream ID for Play Stream API
        STREAM_ID         = 0x30,
        //! Task ID, uniquely identifies dataflow
        TASK_ID           = 0x38,
        //! Request ID of incoming request
        IN_REQUEST_ID     = 0x3c,
        //! Request ID of served request
        OUT_REQUEST_ID    = 0x40,
        //! Control register
        /*!
         * CONTROL[1:0]: DMA operation type
         *     0b00: READ / 0b01: WRITE / 0b11: RDWR
         * CONTROL[3:2]: Action
         *     0b01: Play Stream
         *     0b10: Acknowledge interrupt
         *     0b11: Unique Stream
         */
        CONTROL           = 0x44,
        //! Status register
        /*! Polled for served requests */
        STATUS            = 0x45
    };

    //! MMIO registers
    struct Registers {
        uint64_t streamNameBase, streamNameRange, readBase, readRange,
                 writeBase, writeRange, streamId;
        uint32_t taskId, inRequestId, outRequestId;
        std::bitset<8> control;
        uint8_t status;

        static const std::unordered_map<gem5::Addr, const char *> names;

        void readOnly(const gem5::Addr addr) const;
        void writeOnly(const gem5::Addr addr) const;
        void invalidSize(const gem5::Addr addr, const unsigned size) const;
        void unexpectedAddr(const gem5::Addr addr) const;
    } regs;

    //! Play Stream API request
    enum DmaOperation : unsigned char { READ, WRITE, RDWR };
    struct Request {
        const uint64_t readBase, readRange, writeBase, writeRange;
        const uint32_t id, taskId;
        const DmaOperation dmaOp;
    };

    //! Stream descriptor
    struct Stream {
        bool active{ false };
        std::queue<Request> pendingRequests;
    };

    //! Reference to the ATP Adapter
    ProfileGen *adapter;
    //! ATP Device ID, same as in .atp files
    const std::string atpId;
    //! IOMMU stream ID for request annotation
    const uint32_t sid;
    //! IOMMU substream ID for request annotation
    const uint32_t ssid;

    //! Stream name read DMA operation completion event
    gem5::EventFunctionWrapper streamNameRead;
    //! Stream name storage buffer
    std::vector<uint8_t> streamNameBuffer;

    //! Stream -> Active status and pending requests
    std::unordered_map<uint64_t, Stream> streams;

    //! Served requests to notify
    std::queue<uint32_t> servedRequests;
    //! True if interrupt acknowledgement is pending
    bool intAckPending{ false };

    //! Provides device address ranges
    gem5::AddrRangeList getAddrRanges() const override;
    //! MMIO read handler
    gem5::Tick read(gem5::PacketPtr pkt) override;
    //! MMIO write handler
    gem5::Tick write(gem5::PacketPtr pkt) override;

    //! Play Stream API handler
    /*! Builds a new request. If no pending requests, serves it */
    void playStreamHandler(void);
    //! Extracts DMA operation from CONTROL
    DmaOperation extractDmaOp(void) const;
    //! Triggers DMA configuration and activates the stream through adapter
    void serveRequest(const uint64_t str_id, const Request &req);
    //! Configures stream DMA operation paramaters through adapter
    void configureDmaOp(const uint64_t str_id, const Request &req);
    //! Callback for when stream completes and associated request is served
    /*! If possible, notifies the served request and serves the next pending */
    void servedRequestHandler(const uint64_t str_id, const uint32_t req_id);
    //! Callback for when a request is issued from the adapter
    /*! Annotates IOMMU IDs and request task ID */
    void annotateRequest(gem5::RequestPtr req, const uint32_t task_id);

    //! Interrupt acknowledgment handler
    /*! Clears interrupt and if possible notifies next served request */
    void intAckHandler(void);
    //! Publishes served request in OUT_REQUEST_ID and sends interrupt
    void notifyServedRequest(const uint32_t req_id);

    //! Unique Stream API handler
    /*! Initialises DMA operation to read the stream name */
    void uniqueStreamHandler(void);
    //! Stream name read DMA operation completion handler
    /*!
     * Calls uniqueStream, publishes stream id and notifies system software
     * through STATUS
     */
    void streamNameHandler(void);
};

} // namespace ATP

#endif /* __ATP_DEVICE_HH__ */
