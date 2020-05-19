/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 2, 2015
 *      Author: Matteo Andreozzi
 */

#include "random_generator.hh"

#include "logger.hh"

namespace TrafficProfiles {

namespace Random {

Generator::Generator(const uint64_t s):
        initialized(false),
        type(RandomDesc::UNIFORM),
        distribution(nullptr),
        seed(s) {
    mersenne.seed(seed);
}

Generator::~Generator() {
    delete distribution;
}

void Generator::init(const RandomDesc::Type t,
        const uint64_t base, const uint64_t range) {
    // clean previously allocated generators
    delete distribution;
    // set new type
    type = t;

    switch(type)
    {
        case RandomDesc::UNIFORM: {
            distribution = new Uniform(this, base, range);
            break;
        }
        case RandomDesc::NORMAL: {
            distribution = new Normal(this, base, range);
            break;
        }
        case RandomDesc::POISSON: {
            distribution = new Poisson(this, base, range);
            break;
        }
        case RandomDesc::WEIBULL: {
            distribution = new Weibull(this, base, range);
            break;
        }
        default:
            ERROR("Generator::init unknown random generator type", RandomDesc::Type_Name(type));
            break;
    }
    LOG("Generator::init",RandomDesc::Type_Name(type),"generator initialised with base",
            base, "range", range);

    initialized = true;
}

void Generator::init(const RandomDesc& from) {

    // clean previously allocated generators
    delete distribution;

    type = from.type();

    switch(type)
    {
        case RandomDesc::UNIFORM: {
            distribution = new Uniform(this, from);
            break;
        }
        case RandomDesc::NORMAL: {
            distribution = new Normal(this, from);
            break;
        }
        case RandomDesc::POISSON: {
            distribution = new Poisson(this, from);
            break;
        }
        case RandomDesc::WEIBULL: {
            distribution = new Weibull(this, from);
            break;
        }
        default:
            ERROR("Generator::init unknown random generator type", RandomDesc::Type_Name(type));
            break;
    }
    LOG("Generator::init",RandomDesc::Type_Name(type),"generator initialised from descriptor");

    initialized = true;
}

uint64_t Generator::get() {
    uint64_t ret = 0;
    if (initialized) {
        ret = distribution->get();
        LOG("Generator::get generated",RandomDesc::Type_Name(type),"value", ret);
    } else {
        ERROR("RandomGenerator::get uninitialised");
    }
    return ret;
}

Uniform::Uniform(Generator* const gen, const uint64_t base, const uint64_t range):
        Distribution(gen, base, range) {
    uint64_t min = base, max = base + range;
    uniform = new uniform_int_distribution<uint64_t>(min,max);
}

Uniform::Uniform(Generator* const gen, const RandomDesc& from):
        Distribution(gen) {
    uint64_t min = from.uniform_desc().min(), max = from.uniform_desc().max();
    uniform = new uniform_int_distribution<uint64_t>(min,max);
}

uint64_t Uniform::get() {
    return (*uniform)(generator->mersenne);
}

Normal::Normal(Generator* const gen, const uint64_t base, const uint64_t range):
        Distribution(gen, base, range) {
    double dev  = range/2, mean = base + dev;
    normal = new normal_distribution<double>(mean,dev);
}

Normal::Normal(Generator* const gen, const RandomDesc& from):
        Distribution(gen) {
    double mean = from.normal_desc().mean(), dev = from.normal_desc().std_dev();
    normal = new normal_distribution<double>(mean,dev);
}

uint64_t Normal::get() {
    return (*normal)(generator->mersenne);
}
Poisson::Poisson(Generator* const gen, const uint64_t base, const uint64_t range):
        Distribution(gen, base, range) {
    double mean = base + range/2;
    poisson = new poisson_distribution<uint64_t>(mean);
}

Poisson::Poisson(Generator* const gen, const RandomDesc& from):
        Distribution(gen) {
    double mean = from.poisson_desc().mean();
    poisson = new poisson_distribution<uint64_t>(mean);
}

uint64_t Poisson::get() {
    return (*poisson)(generator->mersenne);
}

Weibull::Weibull(Generator* const gen, const uint64_t base, const uint64_t range):
        Distribution(gen, base, range) {
    // todo should we work scale and shape out from GAMMA ?
    double scale = base + range/2, shape = 0;
    weibull = new weibull_distribution<double>(shape, scale);
}

Weibull::Weibull(Generator* const gen, const RandomDesc& from):
         Distribution(gen) {
    double shape = from.weibull_desc().shape(), scale = from.weibull_desc().scale();
    weibull = new weibull_distribution<double>(shape,scale);
}

uint64_t Weibull::get() {
    return (*weibull)(generator->mersenne);
}

}

}
