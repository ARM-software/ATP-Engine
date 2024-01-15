/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2017 ARM Limited
 * All rights reserved
 *  Author: Matteo Andreozzi
 */

#include <sys/stat.h>
#include "packet_tracer.hh"
#include "logger.hh"
#include "traffic_profile_manager.hh"
#include "utilities.hh"

namespace TrafficProfiles {

const string PacketTracer::traceExt = ".trace";


PacketTracer::PacketTracer(TrafficProfileManager* t): tpm(t), enabled(false),
        timeUnit(Configuration::S),latencyUnit(Configuration::NS) {
    // load packet command names
    for (uint64_t i = 0; i < TrafficProfiles::Command_ARRAYSIZE; ++i) {
        traceName[i] = Command_Name(Command(i));
    }
    // add additional trace names
    traceName[OT] = "OT";
    traceName[LATENCY] = "LATENCY";
}

PacketTracer::~PacketTracer() {
    // close all open descriptors
    for (auto&m : traces) {
        for (auto& os : m.second) {
            os.close();
        }
    }
}

void PacketTracer::setOutDir(const string& dir) {
    outDir = Utilities::buildPath(dir);
}

array<ofstream, PacketTracer::TYPES>& PacketTracer::getTraceFiles(const uint64_t mId) {

    if (traces.find(mId)==traces.end()) {
        // create the output directory if it does not exist yet
        mkdir(outDir.c_str(), S_IRWXU|S_IRWXG);
        // calculate base trace name
        auto masterName = tpm->masterName(mId);
        // create packet types traces
        for (uint64_t t=0; t < TYPES; t++) {
            const string fullPath = Utilities::buildPath(outDir,
                masterName+"."+traceName[t]+traceExt);
            traces[mId][t].open(fullPath, ofstream::out);
            if (traces[mId][t].is_open())
            {
                LOG("PacketTracer::getTraceFiles opened trace",fullPath);
            } else {
                ERROR("PacketTracer::getTraceFiles failed to open trace",fullPath);
            }
        }
        LOG("PacketTracer::getTraceFiles created traces for master", masterName);
    }

    return traces[mId];
}

void PacketTracer::trace(const Packet* const pkt) {
    if (enabled) {

        // get numerical master ID
        const uint64_t mId = tpm->masterId(pkt->master_id());
        // access/create trace files
        auto& masterTraces = getTraceFiles(mId);

        // load the time scale to report times in the configured time unit
        const double timeScale =
                tpm->toFrequency(tpm->getTimeResolution())/tpm->toFrequency(timeUnit);

        auto trace_prefix =
            [pkt, timeScale](std::ofstream &out) -> std::ofstream & {
                out << static_cast<double>(pkt->time()) / timeScale << " "
                    << " 0x" << std::hex << pkt->addr() << std::dec << " ";
                return out;
        };

        // write the time trace point
        trace_prefix(masterTraces[pkt->cmd()]) << pkt->size() << std::endl;

        LOG("PacketTracer::trace tracing master",
                        pkt->master_id(), "packet uid",pkt->uid(),
                        "type", Command_Name(pkt->cmd()), "address",
                        Utilities::toHex(pkt->addr()),"size", pkt->size());

        // write a time/latency trace if the packet was awaited by the TPM
        double delay=tpm->getTime(), requestTime=tpm->getTime();
        uint64_t destId = 0;
        // the packet was waited for
        if (tpm->getDestinationProfile(requestTime, destId, pkt)) {
            // compute latency in ATP time (current ATP time - requestTime)
            delay-=requestTime;

            // get the latency default scale factor
            const double latencyScale = tpm->toFrequency(latencyUnit);

            // compute the latency value using the latency time unit, then truncate to int
            const double latency = (int)((double)(delay)/
                    (tpm->toFrequency(tpm->getTimeResolution())/latencyScale));

            LOG("PacketTracer::trace tracing master", pkt->master_id(), "packet uid", pkt->uid(),
                    "request time", requestTime, "delay (", Configuration::TimeUnit_Name(tpm->getTimeResolution())
                    ,")", delay, "latency (",Configuration::TimeUnit_Name(latencyUnit),")",latency);

            trace_prefix(masterTraces[LATENCY]) << latency << std::endl;

            // acquire the current master OT count and trace it
            trace_prefix(masterTraces[OT]) << tpm->getOt(destId) << std::endl;
        }
    }
}

} /* namespace TrafficProfiles */
