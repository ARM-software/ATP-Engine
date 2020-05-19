/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2020 ARM Limited
 * All rights reserved
 * Author: Adrian Herrera
 */

#ifndef __TYPES_HH__
#define __TYPES_HH__

#include <cstddef>
#include <limits>

namespace TrafficProfiles {

template<typename T>
constexpr T InvalidId() { return numeric_limits<T>::max(); }

template<typename T>
constexpr bool isValid(const T id) { return id != InvalidId<T>(); }

}

#endif /* __TYPES_HH__ */
