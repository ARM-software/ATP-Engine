/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2017 ARM Limited
 * All rights reserved
 *  Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_PACKET_TRACER_HH__
#define __AMBA_TRAFFIC_PROFILE_PACKET_TRACER_HH__

#include <array>
#include <string>
#include <unordered_map>
#include "proto/tp_config.pb.h"
#include "proto/tp_packet.pb.h"


using namespace std;

namespace TrafficProfiles {

class TrafficProfileManager;

/*!
 *\brief Implements the AMBA Traffic Profile Packets Tracer
 *
 * Traces ATP generated packets to files
 */
class PacketTracer {

protected:
    //! additional traces types - extend the packet commands
    enum TraceType {
        OT = Command_ARRAYSIZE,
        LATENCY,
        TYPES
    };

    //! Trace file extension
    static const string traceExt;

    //! Pointer to the TPM
    TrafficProfileManager* const tpm;

    //! Global enable flag
    bool enabled;

    //! all traces time unit
    Configuration::TimeUnit timeUnit;

    //! latency trace value time unit
    Configuration::TimeUnit latencyUnit;

    //! Trace types enum to name
    array<string, TYPES> traceName;

    //! Trace files output directory
    string outDir;

    //! Open files descriptors, per master id, one per packet command plus additional types
    map<uint64_t, array<ofstream, TYPES> > traces;

    /*!
     * Returns trace file descriptors given a master id
     * if the traces do not exist, it creates them
     *\param mId master id
     *\return an array of ofstream, one per trace type
     */
    array<ofstream, TYPES>& getTraceFiles(const uint64_t);

public:
    /*!
     * Constructor
     *\param t pointer to TPM
     */
    PacketTracer(TrafficProfileManager*);

    //! default destructor
    virtual ~PacketTracer();

    /*!
     * Traces a packet to file
     *\param pkt pointer to the packet to be traced
     */
    void trace(const Packet* const);

    /*!
     * Set the output directory name
     *\param dir directory name
     */
    void setOutDir(const string& dir);

    /*!
     * Sets all traces time unit
     *\param t time unit to set
     */
    inline void setTimeUnit(const Configuration::TimeUnit t) {timeUnit=t;}

    /*!
     * Sets the latency time unit
     *\param l latency time unit to set
     */
    inline void setLatencyUnit(const Configuration::TimeUnit l) {latencyUnit=l;}

    /*!
     * Enables the tracer
     */
    inline void enable() {enabled=true;}
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_PACKET_TRACER_HH__ */
