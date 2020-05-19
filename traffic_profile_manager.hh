/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Sep 30, 2015
 *      Author: Matteo Andreozzi
 *
 */

#ifndef __AMBA_TRAFFIC_PROFILE_MANAGER_HH__
#define __AMBA_TRAFFIC_PROFILE_MANAGER_HH__

// Traffic Profile includes
#include <iostream>
#include <fstream>
#include <deque>
#include <queue>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include "packet_desc.hh"
#include "packet_tagger.hh"
#include "packet_tracer.hh"
#include "proto/tp_config.pb.h"
#include "event.hh"
#include "logger.hh"
#include "random_generator.hh"
#include "stats.hh"
#include "types.hh"
#include "kronos.hh"

using namespace std;
//!\brief All ATP code is enclosed in this namespace
namespace TrafficProfiles {
// forward declaration
class TrafficProfileDescriptor;

/*!
 *\brief AMBA Traffic Profile Manager
 *
 * TrafficProfileManager is a std C++/ Google Protobuf platform independent implementation
 * of the AMBA traffic profile generator specifications in (Bruce Mattewson - ARM-ECM-0313315)
 * and AMBA Traffic Profile implementation design (Matteo Andreozzi - n.TBD)
 *
 */
class TrafficProfileManager {
public:
    //! ATP Packet Type
    enum PacketType {
        NO_TYPE = 0,
        REQUEST = 1,
        RESPONSE = 2
    };

protected:

    // declare test class as friend
    friend class TestAtp;

    //! default time resolution used
    static const Configuration::TimeUnit defaultTimeResolution;
    //! default frequency - 1 GhZ
    static const uint64_t defaultFrequency;

    /*!
     * Returns an ATP packet type
     * (REQUEST or RESPONSE)
     *\param cmd ATP packet command
     *\return packet type
     */
    static PacketType packetType (const Command);

    /*! Initialisation flag - true means the profile has been correctly
        loaded from the specification file
     */
    bool initialized;

    /*!
     * Debug Mode: create a Master per each profile,
     * bypasses configured master names
     */
    bool profilesAsMasters;

    /*!
     * ATP Tracker Latency: enables the use of the ATP
     * Tracker Latency for all ATP Masters
     */
    bool trackerLatency;

    //! Kronos enable flag
    bool kronosEnabled;

    /*!
     * Kronos event scheduler calendar buckets width
     * in ATP units of time
     */
    uint64_t kronosBucketsWidth;

    /*!
     * Kronos event scheduler calendar length
     * in ATP units of time
     */
    uint64_t kronosCalendarLength;

    /*!
     * Kronos configuration valid flag
     */
    bool kronosConfigurationValid;

    //! current time, as provided by the adaptor calling TPM methods
    uint64_t time;

    //! Aggregated statistics at TPM level (includes measurements from all Profiles)
    Stats stats;

    //! TrafficProfileManager global time resolution
    Configuration::TimeUnit timeResolution;

    //! Per-profile ID time conversion factors
    map<uint64_t, pair<uint64_t,uint64_t>> timeScaleFactor;

    /*!
     * When loading an ATP file, keeps track of forward-declared
     * profiles and triggers an ERROR if any are left incomplete
     */
    uint64_t forwardDeclaredProfiles;

    /*!
     *  Traffic profile manager Configuration data structures,
     *  which are populated from the Google Protocol Buffer
     *  data files and include all Traffic Profiles used to generate packets
     */
    list<Configuration> config;

    //! Traffic Profiles descriptor pointers - keep track of the Traffic Profiles evolution
    vector <TrafficProfileDescriptor*> profiles;

    /*!\brief Traffic Profiles checkers map
     *
     * Keeps track of which Traffic Profile are monitored by checkers
     * it is organised as profiled ID -> checkers (pointers to) for that profile
     */
    multimap <uint64_t, TrafficProfileDescriptor*> checkedByMap;
    /*!
     *\brief Keeps track of active checkers
     * Stores checkers profile IDs
     */
    set<uint64_t> checkers;

    /*! Map of configured Master names to Master IDs
     */
    unordered_map<string, uint64_t> masterMap;


    /*! Configured Master IDs to master names
     * mapping vector
     */
    vector<string> masters;

    //! map of Master Id -> profile IDs of that master
    map<uint64_t, set<uint64_t>> masterProfiles;

    /*!
     * Per-master map of non terminated profiles count
     * gets updated at every TERMINATE event reception
     */
    map<uint64_t, uint64_t> nonTerminatedProfiles;

    /*!
     *\brief Profiles active list
     *
     * Profiles added here are queried for sending packets
     * Profiles can add themselves to the active list
     * sending an ACTIVATION event.
     */
    list<uint64_t> activeList;

    /*!
     *\brief Global Packet tagger module
     *
     * Sets generated packet fields according to
     * global configuration
     */
    PacketTagger tagger;

    /*!
     *\brief Global Packet tracer module
     *
     * Traces packets to file
     */
    PacketTracer tracer;

    //! hash map profile name -> profile id
    unordered_map<string, uint64_t> profileMap;

    //! stream cache - map of profile root id -> profiles linked by
    // TERMINATION event, flag which indicates leaves
    map<uint64_t, list<pair<uint64_t, bool>>> streamCache;
    //! stream leaves cache - map of profile root id -> leaves of cache
    map<uint64_t, list<uint64_t>> streamLeavesCache;

    //! maps stream root id to number of cloned instances of streams,
    // list of clone roots
    map<uint64_t, pair<uint64_t,list<uint64_t>>> clonedStreams;
    //! Clone Root Profile ID -> Origin Root Profile ID
    unordered_map<uint64_t, uint64_t> streamCloneToOrigin;

    //! flag to signal that the streamCache is valid
    bool streamCacheValid;

    //! hash map to record responses waited for by profiles when UID routing is used: UID -> (profile, time)
    map<uint64_t, pair<uint64_t,uint64_t>> waitedResponseUidMap;

    //! hash map to record requests waited for by profiles when UID routing is used: UID -> (profile, time)
    map<uint64_t, pair<uint64_t,uint64_t>> waitedRequestUidMap;

    //! hash map event waited for Event id -> Event -> profiles waiting for it
    map<uint64_t, unordered_map<Event, set <uint64_t>>> waitEventMap;

    //! Kronos
    Kronos kronos;

    /*!
     *\brief Packets buffer
     * Stores packets rescheduled for transmission,
     * based on their UID
     *
     */
    map<uint64_t, Packet*> buffer;

    //! Slaves set - lists all slaves profile IDs
    set<uint64_t> slaves;

    /*!
     *\brief Master to slave map
     * Associates one master ID to one or more slave IDs
     */
    map<uint64_t, uint64_t> masterSlaveMap;

    /*!
     *\brief Address range to slave map
     *Associates an address range to an ATP slave
     *all ranges are non-overlapping.
     *Ranges are sorted on lower bounds in descending order,
     *this allows to lookup a value for a possible inclusive range using the
     *map lower_bound method
     */
    map<uint64_t, pair<uint64_t,uint64_t>, greater<uint64_t>> slaveAddressRanges;

    //! next profile transmission times priority queue type
    typedef priority_queue<uint64_t, vector<uint64_t>, greater<uint64_t>> NextTimesPq;

    //! next profile transmission times priority queue, sorted in ascending order
    NextTimesPq nextTimes;

    /*! Creates TrafficProfileDescriptors from a configuration object
     *\param toLoad the protocol buffer configuration object
     */
    void loadConfiguration(const Configuration&);
    /*!
     * Configures TimeUnit and cyclesToTime conversion factor for a given
     * configuration object
     *\param c the protocol buffer configuration object
     *\return the profiles time scale factor (ratio expressed as pair)
     */
    pair<uint64_t, uint64_t> loadTimeConfiguration(const Configuration&);

    /*!
     * Configures the Packets Tagger with global options
     *\param c the protocol buffer configuration object
     */
    void loadTaggerConfiguration(const Configuration&);

    /*!
     * Configures the Packets Traces with global options
     *\param c the protocol buffer configuration object
     */
    void loadTracerConfiguration(const Configuration&);

    /*!
     * Loads a Google Protocol Buffer Traffic Profile Configuration object
     *\param from the Profile object from which configuration should be loaded from
     *\param ts the time scale factors to be applied to this profile configured rate
     *\param overwrite if set, and a profile with the same name is found, replaces it
     *\param clone_num (Optional) Stream Clone number (0=original)
     *\param master_id (Optional) Associated Master ID
     */
    void configureProfile(const Profile&,
                          const pair<uint64_t, uint64_t> = { 1, 1 },
                          bool overwrite=false, const uint64_t=0,
                          const uint64_t=InvalidId<uint64_t>());

    /*!
     * Creates and returns a TrafficProfileDescriptor object according to the passed
     * Google Protocol Buffer configuration
     *
     *\param id traffic profile id
     *\param from the Profile object from which configuration should be loaded from
     *\param clone_num (Optional) Stream Clone number (0=original)
     *\param master_id (Optional) Associated Master ID
     */
    void createProfile(const uint64_t, const Profile&,
                       const uint64_t=0, const uint64_t=InvalidId<uint64_t>());

    /*!
     *\brief Update checkers
     *
     * Updates ATP checkers assigned to an ATP Profile
     * by passing them a packet request or response
     *
     *\param profile the ATP profile ID
     *\param packet a pointer to the ATP Packet sent/received
     *\param delay request to response delay (for responses only)
     */
    void updateCheckers(const uint64_t, Packet*, const double=0);

    /*!
     * Method to check if a TrafficProfileDescriptor role is a checker
     *\param pId traffic profile id to check
     *\return true is the profile is a checker
     */
    inline bool isChecker(const uint64_t pId) const {
        return (checkers.count(pId)>0);
    }

    /*!
     * Method to check if a TrafficProfileDescriptor role is a slave
     *\param pId traffic profile id to check
     *\return true is the profile is a slave
     */
    inline bool isSlave(const uint64_t pId) const {
        return (slaves.count(pId)>0);
    }

    /*!
     * Method to check if a Master is internal to ATP
     * NOTE: this operation involves string hashing,
     * it is expensive
     *\param m master name to check
     *\return true if the master is an internal master
     */
    bool isInternalMaster(const string) const;

    /*!
     * Handles TPM Events
     *\param ev event to handle
     *\return true if the event was expected and handled
     */
    bool handle(const Event&);

    /*!
     *\brief Routes a packet between two profiles
     *
     * Routes an packet by calling send on source profile and
     * receive on destination profile.
     * If the packet is not received, an event is scheduled
     * in Kronos for when that profile will allow the packet
     * to be received.
     * The rejected packet is left in the TPM packets buffer
     *\param pkt - optionally pass the packet to be routed, the routed packet
     *          is also returned by reference
     *\param src source profile ID or nullptr if unknown
     *\param dst destination profile ID or nullptr if unknown
     *\return true if the route operation has succeeded
     */
    bool route(Packet*&, const uint64_t*, const uint64_t*);

    /*!
     * Returns a packets generated by the specified traffic profile
     *\param pkt returned by reference
     *\param locked if the profile is locked on waiting for responses
     *\param pId profile id
     *\return true if a packet has been produced by the profile
     */
    bool send(Packet*&, bool&, const uint64_t);

    /*!
     *\brief Handles a Kronos Tick event
     * On a Kronos Tick, TPM :
     * 1) asks Kronos for events
     * 2) routes non-packet events to profiles
     * 3) routes packets corresponding to triggered events to masters or slaves
     * 4) schedules slaves responses or delayed requests to Kronos
     */
    void tick();

    /*!
     * Automatically configures Kronos using slave parameters
     */
    void autoKronosConfiguration();


    /*!
     * Starts Kronos if configured
     */
    void initKronos();

    /*!
     *\brief Profile stream lookup function
     *
     * Builds a list of profiles navigating a
     * profile stream, linked by termination
     * events.
     * Starts from the provided profile root
     *\param root root profile id
     *\return stream list reference
     */
    const list<pair<uint64_t, bool>>& getStream (const uint64_t);

    /*!
     * Causes an update of the stream cache
     * by parsing the activeList of profiles
     */
    void streamCacheUpdate();

    /*!
     *\brief Clones a profile stream and returns a copy of it
     *
     *\param root root profile id
     *\param master_id (Optional) Associated Master ID
     *\return the cloned root profile id
     */
    uint64_t cloneStream(const uint64_t, const uint64_t=InvalidId<uint64_t>());

public:
    //! Default Constructor
    TrafficProfileManager();
    //! Default Destructor
    virtual ~TrafficProfileManager();

    /*! Returns the appropriate scaling factor given a TimeUnit
     *\param t the Time Unit
     *\return the scaling factor
     */
    static uint64_t toFrequency(const Configuration::TimeUnit);

    /*!
     * Returns the ATP Engine time resolution
     *\return the time resolution as time unit
     */
    inline Configuration::TimeUnit getTimeResolution() const { return timeResolution;}

    /*!
     * Returns the time conversion factor for the given profile
     *\param id traffic profile ID
     *\return the time conversion factors
     */
    pair<uint64_t, uint64_t> getTimeScaleFactors(const uint64_t id) const;

    /*!
     * Returns a pointer to given traffic profile index
     *\param index the traffic profile index
     *\return a pointer to the corresponding traffic profile
     */
    TrafficProfileDescriptor* getProfile(const uint64_t) const;

    /*!
     * Returns a constant reference to the traffic profiles map
     *\return a constant reference to the traffic profiles name map
     */
    inline const unordered_map<string, uint64_t>& getProfileMap() const { return profileMap; }

    /*!
     * Returns a constant reference to the Stream Cache map
     *\return a constant reference to the Stream Cache map
     */
    inline const map<uint64_t, list<pair<uint64_t,bool>>>& getStreamCache() const { return streamCache; }

    /* Returns whether a master has terminated executing all its configured
     * Traffic Profiles
     *\param m the master name
     *\return true if the queried for master is terminated
     */
    bool isTerminated(const string&);

    /*! Configures the Traffic Profile by loading
     * the protocol buffer definition file and
     * parsing it
     *\param fileName the configuration file name
     *\return true if the operation was successful, false otherwise
     */
    bool load(const string&);

    /*!
     * Looks up a profile id from name
     * if the profile does not exist,
     * it does create it
     *\param name the profile name
     *\return the profile ID
     */
    uint64_t getOrGeneratePid(const string&);

    /*!
     * Looks up a profile id from name
     *\param name the profile name
     *\return the profile ID
     */
    uint64_t profileId(const string&);

    /*!
     * Looks up a master id from name
     * if the master does not exist,
     * it does create it
     *\param name the master name
     *\return the master ID
     */
    uint64_t getOrGenerateMid(const string&);

    /*!
     * Looks up a master id from name
     *\param name the master name
     *\return the master ID
     */
    uint64_t masterId(const string&) const;

    /*!
     * Returns a master name given its id
     *\param mId master Id
     *\return the master name
     */
    const string& masterName(const uint64_t) const;

    /*!
     * Signals the TPM that a Profile is waiting for an UID
     *\param profile the profile ID
     *\param t the time this request was generated
     *\param uid a packet unique ID
     *\param type type of the packet being waited for
     */
    void wait(const uint64_t, const uint64_t, const uint64_t,
            const PacketType);

    /*!
     * Signals the TPM that a Profile no longer waits for an UID
     *\param profile the profile ID
     *\param uid a packet unique ID
     *\param type type of the packet being waited for
     */
    void signal(const uint64_t, const uint64_t,
            const PacketType);

    /*!
     * Allows a Traffic Profile to subscribe to a specific event
     *\param profile the profile ID
     *\param ev the event which the profile is subscribed to
     */
    void subscribe(const uint64_t, const Event&);

    /*!
     * Signals the TPM that an event has occurred
     * for all profiles of a specific master
     *\param m master name
     *\param e event (id field is ignored)
     */
    void event(const string&, const Event&);

    /*!
     * Signals the TPM that an event has occurred. It gets
     * re-routed to all profiles that subscribed to this event
     * \param ev the event
     */
    void event(const Event&);

    /*!
     * Request the TPM to tag a packet
     *\param p the packet to tag
     */
    inline void tag(Packet* p) { tagger.tag(p); }

    /*!
     * Returns packets generated by the active traffic profiles
     *\param locked if the TPM is locked on waiting for responses
     *\param nextTransmission the next time a packet will be available
     *\param packetTime the current time unit
     *\return a list of ATP packets, ordered per master
     */
    multimap<string, Packet*> send(bool&, uint64_t&, const uint64_t);

    /*!
     * Receive packets -> they get delivered to traffic profiles
     * return true if the packet was expected, false otherwise
     *\param packetTime the time the packet was received
     *\param packet a pointer to the ATP Packet received
     *\return true if the destination profile has accepted the packet, false otherwise
     */
    bool receive(const uint64_t, Packet*);

    /*!
     * Loads a Google Protocol Buffer Configuration object (overwrites any previously loaded data)
     *\param from the Configuration object from which configuration should be loaded from
     */
    void configure(const Configuration&);

    /*!
     * Prints the content of the Manager to given string
     *\param output the string which has to be written
     *\return true if the data could be successfully dumped, false otherwise
     */
    bool print(string&);

    /*!
     * Flush function: cleans all TPM status,
     * flushes the stored configuration and
     * re-initialises it to its initial state.
     */
    void flush();

    /*!
     * Reset function: re-populates the activeList with the current
     * Google Protocol Buffer manager object
     * effectively restarting the TPM.
     * Any status information is discarded.
     */
    void reset();

    /*!
     * Signals that a profiles has undergone reset
     *\param pId profile id
     */
    void signalReset(const uint64_t);

    /*!
     * getter method to access the TPM statistics
     *\return a constant reference to the stats data structure
     */
    inline const Stats& getStats() const { return stats; }
    /*!
     * getter method to access the TPM list of configured Masters
     *\return a set of configured Master names
     */
    const unordered_set<string> getMasters() const;

    /*!
     * getter method to access the TPM list of paired
     * masters with internal slaves
     *\return a map with master to internal slaves pairings
     */
    const unordered_map<string, string> getMasterSlaves() const;

    /*!
     * getter method to access an ATP master statistics
     *\param m the master name
     *\return a constant stats data structure
     */
    const Stats getMasterStats(const string&);

    /*!
     * getter method to access an ATP profile statistics
     *\param p the profile name
     *\return a constant stats data structure
     */
    const Stats getProfileStats(const string&);

    /*!
     * method to query whether the TPM is waiting for responses
     *\return true if the TPM is waiting for responses, false otherwise
     */
    inline bool waiting() const { return !waitedResponseUidMap.empty();}

    /*!
     * API to enable UID routing
     */
    inline void enableUidRouting() { WARN("TrafficProfileManager::enableUidRouting is now enabled by default");}

    /*!
     * API to enable special mode where each profile is
     * created as a single master
     * (please use for debug purposes only)
     */
    inline void enableProfilesAsMasters() { profilesAsMasters=true;}

    /*!
     * API to disable special mode where each profile is
     * created as a single master
     * (please use for debug purposes only)
     */
    inline void disableProfilesAsMasters() { profilesAsMasters=false;}

    /*!
     * API to enable the ATP Tracker latency for all ATP Masters
     */
    inline void enableTrackerLatency() {trackerLatency=true;}

    /*!
     * method to check UID routing status
     *\return value of the UID routing enable flag
     */
    inline bool isUidRouting() const {
        WARN("TrafficProfileManager::isUidRouting is now enabled by default");
        return true;
    }

    /*!
     * method to check special profiles as masters
     * mode status
     *\return value of profilesAsMasters enable flag
     */
    inline const bool& isProfilesAsMasters() const { return profilesAsMasters;}

    /*!
     * method to check ATP tracker latency status
     *\return value of trackerLatency enable flag
     */
    inline const bool& isTrackerLatencyEnabled() const { return trackerLatency;}

    /*!
     * gets the current ATP time
     *\return the ATP time
     */
    inline const uint64_t& getTime() const {return time;}

    /*!
     * sets the current ATP time
     *\param t time to set - cannot be in the past
     */
    inline void setTime(const uint64_t t) {
        if (t>=time){
            time = t;
        }
    }

    /*!
     * Sets Kronos configuration parameters
     *\param b bucket width in time
     *\param c calendar length in time
     */
    void setKronosConfiguration(const string&, const string&);

    /*!
     * Gets Kronos configuration parameters
     *\return pair bucket width, calendar size
     */
    inline const pair<uint64_t, uint64_t> getKronosConfiguration() const {
        return make_pair(kronosBucketsWidth, kronosCalendarLength);
    }

    /*!
     * Registers a master to a slave ID
     *\param master master name
     *\param sId slave ID
     */
    void registerMasterToSlave(const string&, const uint64_t);

    /*!
     * Sets an address range for an internal ATP slave
     *\param low lower address range bound (inclusive)
     *\param high higher address range bound (inclusive)
     *\param slaveId ID of the ATP slave profile
     */
    void registerSlaveAddressRange(const uint64_t, const uint64_t, const uint64_t);

    /*!
     *\brief Checks if a packet destination is an internal slave
     * Checks if a packet is destined to an internal slave by
     * cross matching master to slave assignments
     * and slave address ranges.
     * If a slave has no master assigned, its assigned address
     * range will catch any unassigned packet.
     * If a slave has a master assigned,
     * it will catch any packets originated from that master.
     * If a slave has both a master and an address range assigned,
     * it will catch packets in the assigned range originated
     * from the assigned master.
     *\param dest destination slave id
     *\param pkt packet to be checked
     *\return true if an internal slave destination is matched
     */
    bool toInternalSlave(uint64_t&, const Packet*) const;

    /*!
     * Returns the profile destination ID for an incoming packet
     * (packet receive destination). It will check the internal
     * routing table and the waited address table
     *\param rTime request time returned by reference
     *\param dest destination profile returned by reference
     *\param pkt the packet to be routed
     *\return true if a destination profile was found
     */
    bool getDestinationProfile(double&, uint64_t&, const Packet*) const;

    /*!
     * Returns the current number of outstanding transactions
     * for a given profile
     *\param pId profile ID
     *\return current profile OT
     */
    uint64_t getOt(const uint64_t) const;

    /*!
     * API to directly enable the Tracer and set its output directory
     *\param out the tracer output directory
     */
    void enableTracer(const string&);

    /*!
     * Runs the main event loop, which can call the kronosCallback at
     * each iteration if defined
     */
    void loop();

    /*!
     *\brief Profile stream reconfiguration
     *
     * Builds an adjacency list of profiles of the type
     * specified and recursively reconfigures their address ranges
     * based on the base and range passed so that they fit the new
     * address space. Starts from the provided profile
     *\param root root profile id
     *\param base new address base
     *\param range address range (in bytes)
     *\param type (optional) specifies to apply the reconfiguration to
     * either READ or WRITE profiles only
     *\return the number of utilised bytes of the assigned range
     *  or zero if the reconfiguration failed
     */
    uint64_t addressStreamReconfigure(
            const uint64_t, const uint64_t,
            const uint64_t, const Profile::Type=Profile::NONE);

    /*!
     *\brief Resets a profiles stream
     *
     * Looks up a list of profiles linked by TERMINATION event
     * and issues them the reset command
     *\param root stream root profile id
     */
    void streamReset(const uint64_t);

    /*!
     *\brief Checks if a profiles stream is terminated
     *
     * Traverses a profiles stream hierarchy and returns
     * true only if all profiles of the stream have
     * terminated
     *\param root stream root profile id
     *\return true if the stream has terminated,
     *  false otherwise
     */
    bool streamTerminated(const uint64_t);

    /*!
     *\brief Gets a unique reference to a stream
     *
     * If the requested stream is already in use, it clones it
     *\param root root profile id
     *\param master_id (Optional) Associated Master ID
     *\return the unique (root or cloned) root profile id
     */
    uint64_t uniqueStream(const uint64_t,
                          const uint64_t=InvalidId<uint64_t>());
};

} // end of namespace
#endif /* __AMBA_TRAFFIC_PROFILE_MANAGER_HH__ */
