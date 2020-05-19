/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: Oct 2, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_RANDOM_GEN_DESC_HH_
#define __AMBA_TRAFFIC_PROFILE_RANDOM_GEN_DESC_HH_

// Traffic Profile includes
#include "proto/tp_config.pb.h"

#include <random>

using namespace std;

namespace TrafficProfiles {

namespace Random {

//! forward declaration
class Generator;

/*!
 *\brief Random distribution class
 *
 * Encapsulates random number distributions
 * with their configuration APIs
 */
class Distribution {
protected:
    //! Pointer to the generator container object
    Generator* const generator;

    //! Stores the configured base (min) value
    uint64_t baseValue;

    //! Stores the configured range value (min + range = max)
    uint64_t rangeValue;

public:
    /*!
     * Base/range constructor
     *\param gen pointer to the generator container object
     *\param base  min value of the distribution
     *\param range range of values allowed for the distribution
     */
    Distribution(Generator* const gen, const uint64_t base, const uint64_t range):
        generator(gen), baseValue(base), rangeValue(range) {}
    /*!
     * Distribution specific constructor
     *\param gen pointer to the generator container object
     */
    Distribution(Generator* const gen):
        generator(gen), baseValue(0), rangeValue(0) {}

    //! Default destructor
    virtual ~Distribution() {}
    /*!
     * Gets a new value from the distribution
     *\return a randomly extracted unsigned integer value
     */
    virtual uint64_t get() = 0;
};

/*!
 * Random Uniform Distribution
 */
class Uniform : public Distribution {

    //! linear unsigned integer distribution
    uniform_int_distribution<uint64_t>* uniform;

public:
    /*!
     * Base/range constructor
     *\param gen pointer to the generator container object
     *\param base  min value of the distribution
     *\param range range of values allowed for the distribution
     */
    Uniform(Generator* const gen, const uint64_t base, const uint64_t range);
    /*!
     * Distribution specific constructor
     *\param gen pointer to the generator container object
     *\param desc protobuf distribution descriptor
     */
    Uniform(Generator* const gen, const RandomDesc& desc);

    //! Default destructor
    ~Uniform() {delete uniform;}

   /*!
    * Gets a new value from the distribution
    *\return a randomly extracted unsigned integer value
    */
   uint64_t get();
};

/*!
 * Random Normal Distribution
 */
class Normal: public Distribution {

    //! double precision normal distribution
    normal_distribution<double>* normal;
public:
   /*!
    * Base/range constructor
    *\param gen pointer to the generator container object
    *\param base  min value of the distribution
    *\param range range of values allowed for the distribution
    */
    Normal(Generator* const gen, const uint64_t base, const uint64_t range);
   /*!
    * Distribution specific constructor
    *\param gen pointer to the generator container object
    *\param desc protobuf distribution descriptor
    */
    Normal(Generator* const gen, const RandomDesc& desc);

    //! Default destructor
    ~Normal() {delete normal;}

   /*!
    * Gets a new value from the distribution
    *\return a randomly extracted unsigned integer value
    */
    uint64_t get();
};

/*!
 * Random Poisson Distribution
 */
class Poisson: public Distribution {

    //! Poisson distribution
    poisson_distribution<uint64_t>* poisson;
public:
   /*!
    * Base/range constructor
    *\param gen pointer to the generator container object
    *\param base  min value of the distribution
    *\param range range of values allowed for the distribution
    */
    Poisson(Generator* const gen, const uint64_t base, const uint64_t range);
   /*!
    * Distribution specific constructor
    *\param gen pointer to the generator container object
    *\param desc protobuf distribution descriptor
    */
    Poisson(Generator* const gen, const RandomDesc& desc);
    //! Default destructor
    ~Poisson() {delete poisson;}

   /*!
    * Gets a new value from the distribution
    *\return a randomly extracted unsigned integer value
    */
    uint64_t get();
};

/*!
 * Random Weibull Distribution
 */
class Weibull: public Distribution {
    //! Weibull distribution
    weibull_distribution<double>* weibull;

public:

   /*!
    * Base/range constructor
    *\param gen pointer to the generator container object
    *\param base  min value of the distribution
    *\param range range of values allowed for the distribution
    */
    Weibull(Generator* const gen, const uint64_t base, const uint64_t range);
   /*!
    * Distribution specific constructor
    *\param gen pointer to the generator container object
    *\param desc protobuf distribution descriptor
    */
    Weibull(Generator* const gen, const RandomDesc& desc);
    //! Default destructor
    ~Weibull() {delete weibull;}

   /*!
    * Gets a new value from the distribution
    *\return a randomly extracted unsigned integer value
    */
    uint64_t get();
};



/*!
 *\brief Random Numbers generator class
 *
 * Can be configured to generate random numbers according to various distributions.
 * Uses the default generator defined in the random C++11 library
 */
class Generator {

    //! Mersenne Twister 64-bit random generator
    mt19937_64 mersenne;

    //! flag which indicated whether this RandomGenDesc has been initialised
    bool initialized;
    //! Random Number generator distribution type
    RandomDesc::Type type;

    //! Random distribution
    Distribution* distribution;

    //! Random generator seed
    uint64_t seed;

    // friend class declaration
    friend class Distribution;
    friend class Uniform;
    friend class Normal;
    friend class Poisson;
    friend class Weibull;

  public:

    /*! Constructor
     *\param s random number generator seed (default to 1)
     */
    Generator(const uint64_t s = 1);

    //! Default destructor
    virtual ~Generator();

    /*!
     * Initialises a Random Generator Descriptor with data from a configuration object
     *\param from the Google Protocol Buffer configuration object
     */
    void init(const RandomDesc&);

    /*!
     * Initialises a Random Generator Descriptor with a base and a range value
     *\param t random distribution type
     *\param base minimum value
     *\param range max deviation from min value (max=base+range)
     */
    void init(const RandomDesc::Type, const uint64_t, const uint64_t);

    /*!
     * Gets a new value from the configured distribution
     *\return a randomly extracted unsigned integer value
     */
    uint64_t get();

    /*!
     * Returns the configured random generator type
     *\return the rng type
     */
    const RandomDesc::Type& getType() const {return type;}


};



} // end of TraffiProfiles

} // end of Random

#endif /* __AMBA_TRAFFIC_PROFILE_RANDOM_GEN_DESC_HH_ */
