/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: July 1, 2016
 *      Author: Matteo Andreozzi
 */


#ifndef __AMBA_TRAFFIC_PROFILE_UTILITIES_HH__
#define __AMBA_TRAFFIC_PROFILE_UTILITIES_HH__

#include "logger.hh"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <map>
#include <set>
#include <type_traits>

using namespace std;
using namespace TrafficProfiles;

namespace Utilities {

/*!
 *\brief Frequency units lookup map
 *
 * Matches a text time unit specifier with the
 * corresponding multiplier in Hz
 */
const map<string, uint64_t> frequencyUnits = {
        {"p",1e12},
        {"n",1e9},
        {"u",1e6},
        {"m",1e3},
        {"s", 1}
};


/*!
 *\brief Byte units lookup map
 *
 * Matches a text byte unit specifier with
 * the corresponding integer value in bytes
 */
const map<string, uint64_t> byteUnits = {
    {"TB",1e12},
    {"GB",1e9},
    {"MB",1e6},
    {"kB",1e3},
    {"B",1},
    {"TiB",(1UL << 40)},
    {"GiB",(1UL << 30)},
    {"MiB",(1UL << 20)},
    {"KiB",(1UL << 10)},
    {"Tb",(1e12)/8},
    {"Gb",(1e9)/8},
    {"Mb",(1e6)/8},
    {"kb",(1e3)/8},
    {"Tib",(1UL << 37)},
    {"Gib",(1UL << 27)},
    {"Mib",(1UL << 17)},
    {"Kib",(1UL << 7)}
};

/*!
 * number to string map comparator
 */
struct stringMapComp {
  bool operator() (const double& lhs,
          const double& rhs) const
  {return lhs>rhs;}
};

/*!
 *\brief Time strings lookup map
 *
 * Allows conversion form a time numeric
 * value in seconds to one with SI/IEC unit
 * specifier
 */
const map<double, string, stringMapComp> timeStrings = {
        {1e-12, "ps"},
        {1e-9,  "ns"},
        {1e-6,  "us"},
        {1e-3,  "ms"},
        {1,     "s"}
};

/*!
 *\brief Byte byte units lookup map
 *
 * Allows conversion from a byte numeric
 * value to one with SI/IEC unit specifier
 */

const map<double, string, stringMapComp> byteStrings = {
        {1e12,"TB"},
        {1e9, "GB"},
        {1e6, "MB"},
        {1e3, "kB"},
        {1, "B"}
};

/*!
 *\brief Bandwidth (data rate) separators
 *
 * Specifies all allowed separators to be used
 * between Data and Time units
 * The order in which are specified is the order
 * in which they'll be checked
 */
const string rateSeparators[] = {
        "/",
        "p",
        "@"
};

/*!
 * Converts a string to lower case
 *\return the converted lower case string
 */
const string toLower(const string s);

/*!
 * Great Common Denominator function
 *\param a first number
 *\param b second number
 *\return the great common denominator
 *        between two numbers
 */
template<typename T>
T GCD(const T a, const T b)
{
    return b == 0 ? a : GCD<T>(b, fmod(a,b));
}

/*!
 * Reduces a fraction
 *\param num numerator
 *\param den denominator
 *\return pair of simplified factors
 */
template<typename T>
pair<T,T> reduce (const T num, const T den) {
    T gcd = GCD(num,den);

    return make_pair(num/gcd, den/gcd);
}

/*!
 * Checks if a string represents a number
 *\param s string to be checked
 *\return true if the string is a number
 */
bool isNumber(const string& s);

//! Left-only string trimming
void trimLeft(string& s);
string trimLeftCopy(string s);

//! Right-only string trimming
void trimRight(string& s);
string trimRightCopy(string s);

//! Outer-only string trimming
void trimOuter(string& s);
string trimOuterCopy(string s);

/*!
 * Trims all spaces from a string and returns a trimmed copy of it
 *\param s string to be trimmed
 *\return the trimmed string
 */
const string trim(const string& s);

/*!
 * Extracts the heading element from a stringified list
 *\param s stringified list
 *\return the heading element
 */
string extractHead(const string& s);

/*!
 * Extracts the trailing elements from a stringified list
 *\param s stringified list
 *\return the trailing elements (stringified)
 */
string extractTail(const string& s);

/*!
 * Converts a string representing a positive floating
 * point number to an unsigned integer number and
 * a scale factor
 *\param s string to be converted
 *\return pair number, scale
 */
template<typename T>
pair<T,T> toUnsignedWithScale(const string& s)
{
    T num = 0, scale = 1;
    string decimals="";
    size_t dot_pos= s.find(".");
    // decimal point found!
    if (dot_pos!=string::npos){
        try {
            decimals = s.substr(dot_pos+1);
        }
        catch (out_of_range& o){
            // decimal point not found or
            // at the end of the number - ignore
        }
    }
    // load integer part of number
    num = stoll(s.substr(0, dot_pos));
    // convert to num and scale
    for(char& c : decimals) {
        num = num*10 + (c-'0');
        scale*=10;
    }
    return make_pair(num, scale);
}

/*!
 * Parses a string for unit specifiers
 * and return their combined multiplier value
 * and position (by reference)
 *\param pos position where to start the search from,
 *      returns by reference the first occurrence found
 *\param s const reference to the string to be parsed
 *\param units const reference to the units map
 *\return multiplier value
 */
uint64_t parseUnits(size_t& pos, const string& s, const map<string, uint64_t>& units);

/*!
 * Converts a string specifying a time to
 * a type T number representing the corresponding
 * frequency in Hz - *** USE FLOATING POINT TYPES! ***
 *\param t string to be converted
 *\return frequency value in Hz - 0 if a pure number is provided
 */
template <typename T>
T timeToHz(const string t) {

    T multiplier = 0, time=0,scale=1;
    size_t unit_pos = 0;

    // apply unit multiplier if needed
    multiplier = parseUnits(unit_pos,t,frequencyUnits);

    if ((unit_pos == string::npos) && !isNumber(t)) {
        // raise error if a non-pure number was passed with an unknown unit specifier
        ERROR("Utilities::timeToHz unsupported time unit specifier detected in", t);
    }
    if (unit_pos > 0) {
        try {
            // trim unit specifier, convert to unsigned integer number
            tie(time, scale) = toUnsignedWithScale<T>(trim(t.substr(0, unit_pos)));
        } catch (invalid_argument& i) {
            ERROR("Utilities::timeToHz invalid parameter", i.what(),
                    "field",t.substr(0, unit_pos),"unit pos",unit_pos);
        }
        catch (out_of_range& o){
            ERROR("Utilities::timeToHz out of range parameter", o.what());
        }
    } else {
        // only the time unit was found in passed string, assume unitary value
        time = 1;
    }
    return time>0 ? (1/time)*multiplier*scale : 0;
}

/*!
 * Converts a string specifying a byte plus unit value
 * to a type T number representing Bytes
 * SI and IEC conventions are followed
 * Bytes are assumed by default
 *\param s input string
 *\return number of bytes
 */
template<typename T>
T toBytes(const string s) {
    T multiplier = 1, bytes=0, scale=1;
    size_t unit_pos = 0;
    // look for byte units, save position and store multiplier
    multiplier = parseUnits(unit_pos, s, byteUnits);
    if ((unit_pos == string::npos) && !isNumber(s)) {
        // raise error if a non-pure number was passed with an unknown unit specifier
        ERROR("Utilities::toBytes unsupported byte unit specifier detected in", s);
    } else if (unit_pos == string::npos) {
        // a pure number was passed
        multiplier = 1;
    }

    try {
        // trim unit specifier, convert to unsigned integer number
        tie(bytes, scale) = toUnsignedWithScale<T>(trim(s.substr(0, unit_pos)));
    }
    catch (invalid_argument& i) {
        ERROR("Utilities::toBytes invalid parameter",
                i.what(), "field",s.substr(0, unit_pos),"unit pos",unit_pos);
    }
    catch (out_of_range& o){
        ERROR("Utilities::toBytes out of range parameter", o.what());
    }

    return bytes*multiplier/scale;
}

/*!
 * Converts a number of bytes into a
 * string representing a rate and utilising the
 * SI/IEC byte units
 */
template<typename T>
string toByteString(const T rate) {
    string ret="0 B";
    for (const auto& u: byteStrings) {
        if (rate >= u.first) {
            ret = to_string((double)rate / u.first)+" "+u.second;
            break;
        }
    }
    return ret;
}

/*!
 * Converts a number of seconds into a
 * string representing time and utilising the
 * SI/IEC byte units
 */
template<typename T>
string toTimeString(const T seconds) {
    string ret="0 s";
    for (const auto& u: timeStrings) {
        if (seconds >= u.first) {
            ret = to_string((double)seconds / u.first)+" "+u.second;
            break;
        }
    }
    return ret;
}

/*!
 * Converts a string specifying a rate to
 * a type T number representing Bytes per second
 * SI and IEC conventions are followed
 *\param s input string
 *\return a pair of bytes per second and multiplier
 *         multiplier = 0 means a pure number was passed
 */
template<typename T>
pair<T,T> toRate(const string s) {
    // if multiplier is 0, this means a pure number has been processed
    T multiplier = 0, rate = 0, scale =1;
    size_t pos = 0, sep_pos = string::npos;

    // 1) look for separator - if found, parse rate and time specifiers
    for (auto& sep:rateSeparators) {
        sep_pos=s.find(sep);
        // separator found
        if (sep_pos!=string::npos) {
            // increase sep_pos by separator length so that
            // we point at the first char of the units field
            sep_pos+=sep.length();
            break;
        }
    }

    if (sep_pos!=string::npos) {

        // 2) look for byte units, store position
        multiplier = parseUnits(pos,s,byteUnits);
        if (pos==string::npos){
            ERROR("Utilities::toBps no rate unit specifier detected in", s);
        }
        // 3) apply unit multiplier if needed
        multiplier *= parseUnits(sep_pos,s,frequencyUnits);

        if (sep_pos==string::npos) {
            ERROR("Utilities::toBps no time unit specifier detected in", s);
        }
    } else if (isNumber(s)) {
       // set parser position to npos
       // whole field should be treated as number
       pos = string::npos;
    } else {
        ERROR("Utilities::toBps unsupported rate unit specifier detected in", s);
    }

    try {
        // trim unit specifier, convert to unsigned integer number
        tie(rate, scale) = toUnsignedWithScale<T>(trim(s.substr(0, pos)));
    }
    catch (invalid_argument& i) {
        ERROR("Utilities::toBps invalid parameter", i.what());
    }
    catch (out_of_range& o) {
        ERROR("Utilities::toBps out of range parameter", o.what());
    }

    // reduce scale and multiplier
    auto temp = reduce (scale, multiplier);
    tie(scale, multiplier) = temp;

    if (scale > 1) {
        ERROR("Utilities::toBps unable to reduce "
                "fractional data rate, please change",s,
                "(scale",scale,"multiplier",multiplier,")");
    }
    return make_pair(rate, multiplier);
}

/*!
 * Converts a number to its hexadecimal
 * string representation
 *\param n the number to convert
 *\return a string with the hexadecimal representation
 *      of the number
 */
template<typename T>
string toHex(const T n) {
    stringstream stream;
    stream << "0x"<< std::hex << n;
    return stream.str();
}

/*!
 * Returns the next power of two
 * to the passed integer number
 *\param n the argument number
 *\return the closest power of two
 */
uint64_t nextPowerTwo(const uint64_t n);

/*! C++17 std::conjuction for C++11 */
template <bool... B>
struct conjunction { };
template <bool First, bool... Rest>
struct conjunction<First, Rest...>
    : integral_constant<bool, First && conjunction<Rest...>::value>{ };
template <bool B>
struct conjunction<B> : integral_constant<bool, B> { };

/*! Constructs a filepath out of several subpaths */
template<typename... Ts, typename Enable = typename enable_if<
    conjunction<is_convertible<Ts, string>::value...>::value>::type>
inline string
buildPath(Ts const&... subpaths)
{
    size_t subpos { 0 };
    string path { };
    for (const string &sp : { subpaths... }) {
        if (!sp.empty() && sp != " ") {
            subpos = !path.empty() && sp.front() == '/' && sp.size() > 1;
            path.append(sp, subpos, sp.size() - subpos);
            if (path.back() != '/') path += '/';
        }
    }
    // Remove trailing backslash
    if (path.size() > 1) path.pop_back();
    return path;
}

} // end of namespace Utilities
#endif /* __AMBA_TRAFFIC_PROFILE_UTILITIES_HH__ */
