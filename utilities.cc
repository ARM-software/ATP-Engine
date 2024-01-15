/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2016 ARM Limited
 * All rights reserved
 *  Created on: 9 Sep 2016
 *      Author: matand03
 */

#include "utilities.hh"

namespace Utilities {


bool isNumber(const string& s)
{
    return !s.empty() && find_if(s.begin(),
        s.end(), [](char c) { return !(isdigit(c) || (c=='.') || (c=='-')); }) == s.end();
}

void trimLeft(string& s){
    s.erase(s.begin(), find_if(s.begin(), s.end(),
        [](char c) { return !isspace(static_cast<unsigned char>(c)); }));
}

string trimLeftCopy(string s){
    trimLeft(s);
    return s;
}

void trimRight(string& s){
    s.erase(find_if(s.rbegin(), s.rend(),
        [](char c) { return !isspace(static_cast<unsigned char>(c)); }).base(),
        s.end());
}

string trimRightCopy(string s){
    trimRight(s);
    return s;
}

void trimOuter(string& s){
    trimLeft(s);
    trimRight(s);
};

string trimOuterCopy(string s){
    trimOuter(s);
    return s;
};

const string trim(const string& s){
    string ret = s;
    // trim string
    ret.erase(remove_if(ret.begin(), ret.end(),
            [](char x){return std::isspace(x);}),
            ret.end());

    return ret;
}

string extractHead(const string& s){
    string ret { trimOuterCopy(s) };
    return ret.substr(0, ret.find_first_of(' '));
}

string extractTail(const string& s){
    string ret { trimOuterCopy(s) };
    const auto sep_pos = ret.find_first_of(' ');
    return (sep_pos == string::npos) ? string { } :
           ret.substr(ret.find_first_not_of(' ', sep_pos + 1));
}

uint64_t nextPowerTwo(const uint64_t n){
    uint64_t temp = n;
    uint64_t i = 0;
    while (temp >>= 1)
    {
      i++;
    }
    return (1<<i);
}

const string toLower(const string s) {
    string ret = s;
    transform(s.begin(), s.end(), ret.begin(), ::tolower);
    return ret;
}

// multiplier = 0 is returned for pure numbers
// this is necessary for the byte rate conversion function - do not change
uint64_t parseUnits(size_t& pos, const string& s, const map<string, uint64_t>& units) {
    bool found=false;
    uint64_t ret = 0;
    size_t pos_ret = string::npos;
    for (auto& u:units) {
       // look for matches from position pos
       auto temp = s.find(u.first, pos);
       if (temp != string::npos) {
           ret = (ret == 0 ? u.second:ret*u.second);
           // store position of first unit specifier character
           if (!found || (temp<pos_ret)) {
               pos_ret=temp;
               found=true;
           }
       }
   }
   // return position of first match by reference
   pos=pos_ret;

   return ret;
}

} // end of namespace Utilities
