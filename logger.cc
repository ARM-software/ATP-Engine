/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 8, 2015
 *      Author: Matteo Andreozzi
 */
#include "logger.hh"
#include <stdexcept>

namespace TrafficProfiles {

Logger* Logger::instance = nullptr;

const string Logger::RED = "\x1b[31m";
const string Logger::GREEN =  "\x1b[32m";
const string Logger::YELLOW = "\x1b[33m";
const string Logger::BLUE = "\x1b[34m";
const string Logger::MAGENTA = "\x1b[35m";
const string Logger::CYAN = "\x1b[36m";
const string Logger::COLOR_RESET = "\x1b[0m";


Logger* Logger::get() {
    if (instance == nullptr) {
#ifdef LOG_FILE
        // the global Logger entity, configured to log to file
        instance = new FileLogger(LOG_FILE, LOG_LEVEL);
#else
        // the default global Logger entity, configured to log to standard output
        instance = new Logger(&cout, LOG_LEVEL);
#endif
    }
    return instance;
}

FileLogger::FileLogger(const string& fileName, const Level lvl):
        Logger(&cout, LOG_LEVEL) {
    level = lvl;
    open(fileName);
}

FileLogger::~FileLogger() {
    if(out!=nullptr) {
        close();
        delete out;
    }
}

void FileLogger::open(const string& fileName)
{
    out = new ofstream(fileName.c_str(), ios_base::binary| ios_base::out);
    if( !dynamic_cast<ofstream*>(out)->is_open() )
    {
        throw(runtime_error("Logger: Unable to open file"));
    }
}

void FileLogger::close()
{
    if(out && dynamic_cast<ofstream*>(out)->is_open())
    {
        dynamic_cast<ofstream*>(out)->close();
    }
}


} // end of namespace
