/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: Sep 30, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_DESC_HH_
#define __AMBA_TRAFFIC_PROFILE_DESC_HH_

// Traffic Profile includes
#include <vector>
#include <map>
#include <set>
#include "packet_desc.hh"
#include "proto/tp_config.pb.h"
#include "event_manager.hh"
#include "logger.hh"
#include "stats.hh"
#include "types.hh"

using namespace std;

namespace TrafficProfiles {

    class TrafficProfileManager;

    /*!
     *\brief Implements the AMBA Traffic Profile base class
     *
     * Profile Descriptor: tracks the status of an active Profile in terms
     * of FIFO fill level, outstanding transactions
     */
    class TrafficProfileDescriptor : public EventManager{

    public:
        //! Traffic Profile role
        enum Role {
            NONE=0,
            MASTER=1,
            CHECKER=2,
            SLAVE=3,
            DELAY=4
        };

        struct Name {
            static constexpr char Reserved { '$' };
            static const string CloneSuffix;
            static const string Default;
            static uint64_t AnonymousCount;
        };

    protected:
        //! traffic profile configuration
        const Profile* config;

        //! Traffic Profile Role
        Role role;
        //! Traffic Profile Type
        const Profile::Type type;
        //! pointer to parent TPM
        TrafficProfileManager* const tpm;
        //! Traffic Profile unique ID
        const uint64_t id;
        //! Traffic Profile Name
        string name;
        //! Traffic Profile Master Name : used to tag packets and register with TPM
        string masterName;
        //! Traffic Profile Master Id
        uint64_t masterId;
        //! Traffic Profile Stream ID
        uint64_t _streamId{ InvalidId<uint64_t>() };
        //! Traffic Profile Master IOMMU ID
        uint32_t _masterIommuId;

        //! number of current outstanding transactions
        uint64_t ot;
        //! statistics collected at Traffic Profile level
        Stats stats;

        //! true if this profile started
        bool started;

        //! profile start time
        uint64_t startTime;

        //! true if this profile terminated
        bool terminated;

        // Monitors support
        //! set of checkers (ATP monitors) assigned to this profile
        set<uint64_t> checkers;

    public:

        /*!
         *\brief Parses a rate configuration parameter
         *
         * Parses a configuration rate string into an integer
         * representing bytes per second
         * The specifier allowed in the string are:
         * TBps, GBps, MBps, KBps, Bps
         *
         * If the rate is configured in ATP units of time,
         * then this function queries TPM for the appropriate
         * scale and period factors
         *
         *\param s configuration string rate
         *\return integer rate in bytes per time unit,
         *        rate period in time units
         */
        pair<uint64_t, uint64_t> parseRate(const string);

        /*!
        *\brief Parses a time configuration parameter
        *
        * Parses a configuration time string into an integer
        * representing a corresponding number
        * of ATP time units
        *
        * The specifier allowed in the string are:
        * s, ms, us, ns, ps
        *
        *\param t configuration string time
        *\return integer rate in ATP time units
        */
        uint64_t parseTime(const string);

        /*!
         * Constructor
         *\param manager the parent Traffic Profile Manager
         *\param index the unique ID of this Traffic Profile
         *\param p a pointer to the configuration object for this Traffic Profile
         *\param clone_num (Optional) Stream Clone number (0=original)
         */
        TrafficProfileDescriptor(TrafficProfileManager*, const uint64_t,
                                 const Profile*, const uint64_t=0);
        //! Default destructor
        virtual ~TrafficProfileDescriptor();

        /*!
         * Signals the descriptor to send a packet
         *\param locked returns true if the profile descriptor is locked on waits
         *\param p packet to send
         *\param next time a packet will be available
         *\return true if the packet can be sent according to OT and FIFO limits,
         *        false otherwise also returns next time a packet will be
         *        available to be sent
         */
        virtual bool send(bool& locked, Packet*& p, uint64_t& next) = 0;

        /*!
         * Signals the descriptor to receive a packet
         * returns true if the packet was expected.
         *\param next in case of rejection,a profile can return here next time a packet can be received
         *\param packet a pointer to the received ATP packet
         *\param delay measured request to response delay
         *\return true if the packet was accepted by the destination Packet Descriptor, false otherwise
         */
        virtual bool receive(uint64_t& next, const Packet* packet, const double delay) = 0;

        /*!
         *\brief returns if the Traffic Profile is active
         *
         *  If the Traffic Profile descriptor is not waiting
         *  for any events, and hasn't sent toSend data yet,
         *  then it's active. toSend == 0 means this profile
         *  doesn't have a limit on the number of transactions to send
         *  if the profile transitions from being active to inactive,
         *  it fires an event to the TPM
         *\param l returns by reference if the Traffic Profile
         *          is locked on events
         *\return true if the Traffic Profile is active, false otherwise
         */
        virtual bool active(bool& l) = 0;

        /*!
         * Resets the profile to its initial status
         */
        virtual void reset();

        /*!
         * Adds the Traffic Profile Descriptor to a Master
         *\param mId master id
         *\param name master name
         */
        virtual void addToMaster(const uint64_t, const string&);

        /*!
         * Adds the Traffic Profile Descriptor to a Stream
         *\param stream_id Stream Root Traffic Profile ID
         */
        virtual void addToStream(const uint64_t stream_id);

        /*!
         * Returns the termination status of this Traffic Profile
         *\return whether this Traffic Profile is terminated
         */
        virtual inline const bool& isTerminated() const { return terminated;}

        /*!
         * Register checker (ATP Monitor) to this profile
         *\param cid checker profile ID
         */
        virtual inline void registerChecker(const uint64_t cid) {checkers.emplace(cid);}

        /*
         * getter method for the TPM pointer
         *\return a pointer to the TPM
         */
        virtual inline TrafficProfileManager* const & getTpm() { return tpm;}

        /*! getter method for this Traffic Profile id
         *\return the traffic profile id
         */
        virtual inline const uint64_t& getId() const {return id;}

        /*! getter method for this Traffic Profile name
         *\return the traffic profile name
         */
        virtual inline const string& getName() const {return name;}
        /*! getter method for this Traffic Profile master name
         *\return the traffic profile master name
         */
        virtual inline const string& getMasterName() const {return masterName;}

        /*! getter method for this Traffic Profile master ID
         *\return the traffic profile master ID
         */
        virtual inline const uint64_t& getMasterId() const {return masterId;}

        /*!
         * gets the profile role
         *\return profile role type
         */
        virtual inline const Role& getRole() const {return role;}

        /*! getter method for this Traffic Profile statistics object
         *\return a constant reference to this Profile statistics object
         */
        virtual inline const Stats& getStats() const {return stats;}
        /*!
         * Advances this Traffic Profile statistics object time
         *\param t time to set
         */
        virtual inline void setStatsTime(const uint64_t t) {stats.setTime(t);}

        /*!
         * Returns the current outstanding transactions (OT) number
         *\return OT
         */
        virtual inline uint64_t getOt() const {return ot;}

        /*!
         * Gets a const reference to the internal configuration
         * object
         *\return the internal configuration object
         */
        virtual inline const Profile* getConfig() const {return config;}

        /*!
         * Activates the profile
         */
        void activate();
     };
} // end of namespace
#endif /* __AMBA_TRAFFIC_PROFILE_DESC_HH_ */
