/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 19, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef _AMBA_TRAFFIC_PROFILE_STATS_HH_
#define _AMBA_TRAFFIC_PROFILE_STATS_HH_

#include <cstdint>
#include <string>
#include <cmath>
#include <limits>

#include "proto/tp_stats.pb.h"

using namespace std;

namespace TrafficProfiles {
/*!
 *\brief Statistics collection class
 *
 * This class provides methods and storage to collect and store statistics
 * at any level in the ATP hierarchy. It can also export recorded values
 * as Google Protocol Buffer objects
 */
class Stats {
    //! started flag : signals that the start time has been initialised
    bool started;

public:
    //! Start time since when stats are computed
    uint64_t startTime;
    //! Time scale factor: used to export time in seconds
    uint64_t timeScale;
    //! last time statistics were recorded
    uint64_t time;
    //! total packet sent counter
    uint64_t sent;
    //! total packet received counter
    uint64_t received;
    //! how many data have been generated
    uint64_t dataSent;
    //! how many data have been received
    uint64_t dataReceived;
    //! previous latency - used to compute jitter
    double prevLatency;
    //! cumulative response jitter
    double jitter;
    //! cumulative response latency
    double latency;
    //! FIFO buffer underruns
    uint64_t underruns;
    //! FIFO buffer overruns
    uint64_t overruns;

    //! Cumulative OT (CurTxn)
    uint64_t ot;
    //! Number of OT measurements
    uint64_t otN;

    //! Cumulative FIFO level (CurLvl)
    uint64_t fifoLevel;

    //! Number of FIFO level measurements
    uint64_t fifoLevelN;

    //! Default Constructor
    Stats();
    //! Default destructor
    virtual ~Stats();
    /*!
     * records a send event
     *\param t the time the event occurred
     *\param data the amount of data sent
     *\param o outstanding transactions
     */
    void send (const uint64_t, const uint64_t, const uint64_t=0);
    /*!
     * records a receive event
     *\param t the time the event occurred
     *\param data the amount of data received
     *\param l the latency measured for this data reception
     */
    void receive (const uint64_t, const uint64_t, const double);

    /*!
     * records a FIFO update event
     *\param l the FIFO level
     *\param u whether the FIFO did underrun
     *\param o whether the FIFO did overrun
     */
    void fifoUpdate(const uint64_t, const bool, const bool);

    //! resets all statistics counters
    inline void reset() {started=false, startTime=numeric_limits<uint64_t>::max(), time=0,
        sent=0, received=0, dataSent=0,
        dataReceived=0, prevLatency=.0, jitter=.0, latency=.0,
        underruns=0, overruns=0, ot=0, otN=0, fifoLevel=0,
        fifoLevelN=0;}

    /*!
     * Starts the statistics from the given time,
     * if not already started
     *\param t start time
     */
    inline void start(const uint64_t t) { if (!started) {started=true; startTime=t;}}

    /*!
     * Advances the time to a specific value
     * going backwards in time is prevented
     *\param t time to set
     */
    void setTime(const uint64_t);

    /*!
     * Dumps statistics
     *\return a formatted string with all statistics
     */
    const string dump() const;
    /*!
     * Method to compute and get the rate for sent data
     *\return the data rate
     */
    inline double sendRate () const
    { return (time? (double)dataSent/((double)time - (double)startTime)*timeScale:0); }
    /*!
     * Method to compute and get the rate for received data
     *\return the data rate
     */
    inline double receiveRate () const
    { return (time? (double)dataReceived/((double)time - (double)startTime)*timeScale:0);}
    /*
     * Method to compute and get the average response latency
     *\return the average response latency
     */
    inline double avgLatency() const
    { return latency/(double)(timeScale * received); }

    /*
     * Method to compute and get the request to response jitter
     *\return the computed response jitter
     */
    inline double avgJitter() const
    { return jitter/(double)(timeScale); }

    /*
     * Method to compute and get the average OT
     *\return the average OT count
     */
    inline uint64_t avgOt() const { return otN ? ceil((double)ot/(double)otN) : 0;}

    /*
     * Method to compute and get the average FIFO level
     *\return the average FIFO level
     */
    inline uint64_t avgFifoLevel() const {return fifoLevelN ? (fifoLevel/fifoLevelN) : 0;}

    /*!
     * Method to compute and return the stats finish time
     *\return the stats time in seconds
     */
    inline double getTime() const {return (double)time/(double)timeScale;}

    /*!
     * Method to compute and return the stats start time
     *\return the stats time in seconds
     */
    inline double getStartTime() const {return (double)startTime/(double)timeScale;}

    /*!
     * Exports statistics as Google Protocol Buffer object
     *\return a Google Protocol Buffer StatObject
     */
    const StatObject* xport() const;

    /*!
     * Sum operator: merges two Stats objects
     *\param s the Stats object to add
     *\return the sum by value
     */
    Stats operator+(const Stats&);

    /*!
    * Add operator: adds Stats objects
    * fields to this one
    *\param s the Stats object to add
    *\return this object by reference
    */
    Stats& operator+=(const Stats&);
};

} /* namespace TrafficProfiles */

#endif /* _AMBA_TRAFFIC_PROFILE_STATS_HH_ */
