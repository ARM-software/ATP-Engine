/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2015 ARM Limited
 * All rights reserved
 *  Created on: Oct 8, 2015
 *      Author: Matteo Andreozzi
 */

#ifndef _AMBA_TRAFFIC_PROFILE_LOGGER_HH_
#define _AMBA_TRAFFIC_PROFILE_LOGGER_HH_

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>

using namespace std;

namespace TrafficProfiles {
/*!
 *\brief ATP Logger class
 *
 * ATP Logger base class to write logging messages to ostreams
 */
class Logger {
public:
    /*! Verbosity log level, specifies which kind of
     * debug messages should be processed
     */
    enum Level {
        DEBUG_LEVEL   = 0,
        WARNING_LEVEL = 1,
        ERROR_LEVEL   = 2,
        PRINT_LEVEL = 3,
        PROMPT_LEVEL = 4
    };

protected:
    // Colours support
    //! Red colour ANSI code
    static const string RED;
    //! Green colour ANSI code
    static const string GREEN;
    //! Yellow colour ANSI code
    static const string YELLOW;
    //! Blue colour ANSI code
    static const string BLUE;
    //! Magenta colour ANSI code
    static const string MAGENTA;
    //! Cyan colour ANSI code
    static const string CYAN;
    //! Colour reset ANSI code
    static const string COLOR_RESET;

    //! Singleton logger object pointer
    static Logger* instance;

    //! Configured verbosity level
    Level  level;
    //! Output stream to use for logging the messages
    ostream* out;
    //! Flag to enable colours
    bool colours;
    //! Flag to enable exit on errors
    bool exitOnErrors;

    /*!
     *  variadic template logger - recursive body
     *  prints the head argument to the configured ostream
     *
     *\param lvl the logging level this message has
     *\param head the first argument of the variadic function
     *\param tail all the remaining arguments of the variadic function
     */
    template <class T, class ...Tail>
    void _log(const Level lvl, T head, Tail&&... tail);

    /*!
     *  variadic template logger - recursion termination function
     *  prints end of line to the configured ostream
     *\param lvl the logging level this message has
     */
    inline virtual void _log(const Level lvl){
        if (get()->out){
            if (getColours() && (lvl!=DEBUG_LEVEL)) *(get()->out) << COLOR_RESET;
            if (lvl!=PROMPT_LEVEL) (*(get()->out)) << endl;
        }
    }

    /*!
     * Constructor
     *\param o address of the ostream to be used
     *\param lvl the verbosity level to be configured
     */
    Logger(ostream* o, const Level lvl):level(lvl), out(o),
            colours(false), exitOnErrors(true) {}

    /*!
     * Default Constructor
     */
    Logger() = delete;

public:

    /*!
     * Singleton Logger object accessor
     *\return the only instance of the logger object
     */
    static Logger *get();

    /*!
     * Logging level setter method
     *\param lvl the logging level which has to be set in the Logger
     */
    inline void setLevel(const Level lvl){ level = lvl;}

    /*!
     * Logging level getter method
     *\return the Logger configured logging level
     */
    inline Level getLevel() const { return level;}

    /*!
     * Sets the colours enable flag
     *\param f flag value
     */
    inline void setColours(const bool f) {colours=f;}

    /*!
     * Gets the colours enable flag
     * Automatically disables it in case of redirection
     *\return the colour flag status
     */
    inline bool getColours() const {return colours && isatty(fileno(stdout));}


    /*!
     * Configure the output stream
     *\param os address of output stream object
     */
    inline void setOstream(ostream* os) { out=os;}

    /*!
     * Sets the exitOnErrors flag and returns its status
     *\param e flag value
     */
    inline void setExitOnErrors(const bool e) {exitOnErrors = e;}

    /*!
     * Gets the value of the exitOnErrors flag
     *\returns exitOnErrors flag value
     */
    inline bool getExitOnErrors() const {return exitOnErrors;}

    /*!
     *  Variadic template logger - helper function
     *  start a recursive chain to print all the variadic
     *  arguments provided to the configured ostream, if the
     *  provided level is equal or greater than this Logger
     *  configured verbosity level
     *\param lvl the logging level this message has
     *\param args all the arguments of the variadic function
     */
    template <class ...Tail>
    void log(const Level lvl, Tail&&... args);


    //! Default destructor
    virtual ~Logger(){}
};

/*!
 *\brief ATP Logger class
 * ATP Logger class specialisations to write log messages to file
 */
class FileLogger : public Logger
{
protected:
    /*!
     * opens a file and sets it as default ostream
     *\param fileName the name of the file to be opened
     */
    void open(const string&);
    //! Closes the configured log file
    void close();
    /*!
     * Constructor
     *\param fileName the file name to which log messages shall be written to
     *\param lvl the logging verbosity level
     */
    FileLogger(const string&, const Level);

    /*!
     * Default Constructor
     */
    FileLogger() = delete;
public:

    //! Default Destructor
    virtual ~FileLogger();
};

// variadic logger, helper function implementation
template <class ...Tail>
void Logger::log(const Level lvl, Tail&&... args) {
    if ((get()->out) && (lvl >= level)) {
        switch(lvl) {
        case ERROR_LEVEL:
            if (getColours()) *(get()->out) << RED;
            *(get()->out) << "ERROR:";
            break;
        case WARNING_LEVEL:
            if (getColours()) *(get()->out) << YELLOW;
            *(get()->out) << "WARNING:";
            break;
        case PRINT_LEVEL:
            if (getColours()) *(get()->out) << CYAN;
            break;
        case PROMPT_LEVEL:
            if (getColours()) *(get()->out) << MAGENTA;
            *(get()->out) << "#engine>";
            break;
        case DEBUG_LEVEL: //intentional fall through
        default: break;
        }
        // start recursive body
        _log(lvl,forward<Tail>(args)...);
    }
}

// variadic logger recursive body implementation
template <class T, class ...Tail>
void Logger::_log(const Level lvl,T head, Tail&&... tail){
    *(get()->out) << " " << head ;
    _log(lvl, forward<Tail>(tail)...);
}


// global logger macros - change LOG_LEVEL to hard code verbosity
#define LOG_LEVEL Logger::ERROR_LEVEL
#define ERROR(...)  do { if (Logger::get()->getLevel()<=Logger::ERROR_LEVEL) \
        Logger::get()->log(Logger::ERROR_LEVEL,__VA_ARGS__); \
        if (Logger::get()->getExitOnErrors()) exit(1); } while (false)
#define WARN(...)   do { if (Logger::get()->getLevel()<=Logger::WARNING_LEVEL) \
        Logger::get()->log(Logger::WARNING_LEVEL,__VA_ARGS__); } while (false)
#define LOG(...)    do { if (Logger::get()->getLevel()<=Logger::DEBUG_LEVEL) \
        Logger::get()->log(Logger::DEBUG_LEVEL,__VA_ARGS__); } while (false)
#define PRINT(...)  do { if (Logger::get()->getLevel()<=Logger::PRINT_LEVEL) \
        Logger::get()->log(Logger::PRINT_LEVEL,__VA_ARGS__); } while (false)
#define PROMPT(...)  do { if (Logger::get()->getLevel()<=Logger::PROMPT_LEVEL) \
        Logger::get()->log(Logger::PROMPT_LEVEL,__VA_ARGS__); } while (false)
} // end of namespace

#endif /* _AMBA_TRAFFIC_PROFILE_LOGGER_HH_ */
