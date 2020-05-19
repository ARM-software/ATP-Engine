/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2019 ARM Limited
 * All rights reserved
 *  Created on: 3 Jul 2019
 *      Author: Matteo Andreozzi
 */

#ifndef __AMBA_TRAFFIC_PROFILE_SHELL_HH__
#define __AMBA_TRAFFIC_PROFILE_SHELL_HH__

#include <iostream>
#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>
#include <termios.h>

#include "logger.hh"

using namespace std;

namespace TrafficProfiles {

// forward declaration
class TestAtp;

/*!
 *\brief ATP Engine interactive shell interpreter
 *
 * The ATP Shell is an interactive command line prompt
 * which can execute a set of commands from user input
 */
class Shell {

protected:

    using Routine         = function<void(const string&)>;
    using Command         = pair<Routine, string>;
    using CommandMap      = unordered_map<string, Command>;
    using CommandGroup    = pair<string, CommandMap>;
    using CommandGroupMap = unordered_map<string, CommandGroup>;

    template<typename Impl>
    Command makeCommand(Impl impl, const string &desc);
    template<typename Impl>
    Command makeTpmCommand(Impl impl, const string &desc);

    //! Available Commands
    const CommandMap commands {
            {"hello",   makeCommand(&Shell::hello, "does nothing")},
            {"help",    makeCommand(&Shell::help, "shows this help")},
            {"exit",    makeCommand(&Shell::quit, "exit the shell")},
            {"quit",    makeCommand(&Shell::quit, "quits the shell")},
            {"load",    makeCommand(&Shell::load, "loads an atp file")},
            {"test",    makeTpmCommand(&Shell::testAgainstSlave,
                        "plays loaded atp files")},
            {"flush",   makeTpmCommand(&Shell::flush,
                        "flushes loaded ATP profiles")},
            {"verbose", makeCommand(&Shell::verbose,
                        "enable verbose debug mode")},
            {"slave",   makeCommand(&Shell::setSlave,
                        "sets the ATP slave bandwidth and latency "
                        "parameters")},
            {"reset",   makeTpmCommand(&Shell::reset,
                        "Resets the TPM, causing profiles to be re-loaded")},
            {"pam",     makeTpmCommand(&Shell::profilesAsMasters,
                        "Toggles ATP FIFO to be instantiated as ATP Masters, "
                        "and resets the TPM")}
    };

    //! Available Command Groups
    const CommandGroupMap commandGroups {
            {"ls", make_pair("lists items based on the specified keyword",
                CommandMap {
                    {"profiles", makeTpmCommand(&Shell::lsProfiles,
                                 "lists profiles")},
                    {"masters",  makeTpmCommand(&Shell::lsMasters,
                                 "lists masters")},
                    {"files",    makeTpmCommand(&Shell::lsFiles,
                                 "lists loaded ATP files")},
                    {"streams",  makeTpmCommand(&Shell::lsStreams,
                                 "lists loaded Stream roots")}
                })
            },
            {"stream", make_pair("Runs stream commands",
                CommandMap {
                    {"status",      makeTpmCommand(&Shell::streamStatus,
                                    "Prints the status of a stream")},
                    {"info",        makeTpmCommand(&Shell::streamStatus,
                                    "Prints the status of a stream")},
                    {"activate",    makeTpmCommand(&Shell::streamActivate,
                                    "Activates a stream")},
                    {"reset",       makeTpmCommand(&Shell::streamReset,
                                    "Resets a stream")},
                    {"reconfigure", makeTpmCommand(&Shell::streamReconfigure,
                                    "Reconfigures the profile stream. Usage: "
                                    "reconfigure root base range "
                                    "<type:NONE|READ|WRITE>")},
                    {"unique",      makeTpmCommand(&Shell::uniqueStream,
                                    "Creates a unique instance of a stream "
                                    "optionally associated to a master. "
                                    "Usage: unique root master")}
                })
            }
    };

    /*! State for the current Line of input */
    class Line
    {
        protected:
            /*! Characters withing the Line */
            string _content { };
            /*! Position of the Cursor within the Line */
            size_t _cursorPos { 0 };

        public:
            /*! Refreshes the Line in user input */
            void refresh();
            void refresh(size_t next_cursor_pos);
            /*! Retrieves next Character from user input */
            unsigned char getNextChar() const;
            /*! Moves Cursor backward number of positions specified */
            void moveCursorBackward(size_t npos=1);
            /*! Moves Cursor forward number of positions specified */
            void moveCursorForward(size_t npos=1);
            /*! Moves Cursor to the end of the Line */
            void moveCursorToEnd();
            /*! Moves Cursor to the start of the Line */
            void moveCursorToStart();
            /*! Inserts a new Character to the Line content */
            void insert(const char &c);
            /*! Appends string to the Line */
            void append(const string &s);
            /*! Removes Character before the Cursor from the Line */
            void backspace();
            /*! Ends the input flow for the Line */
            void end();
            /*! Clears the current Line from input */
            void clear();
            /*! Sets the content of the Line */
            Line &operator=(const string &new_content);
            /*! Retrieves the Line content */
            const string &content() const;
            /*! Checks if the Cursor is at the end of the Line content */
            bool isCursorAtEnd() const;
            /*! Checks if the Line ends with the provided Character */
            bool endsWith(const char &c) const;
    };

    /*! Control character identifiers */
    enum ControlChar : unsigned char {
        END_OF_TEXT         = '\x03',
        END_OF_TRANSMISSION = '\x04',
        BACKSPACE           = '\b',
        HORIZONTAL_TAB      = '\t',
        LINE_FEED           = '\n',
        CARRIAGE_RETURN     = '\r',
        ESCAPE              = '\x1b',
        DELETE              = '\x7f'
    };

    //! Singleton shell object pointer
    static Shell* instance;

    //! pointer to the ATP Test Suite
    TestAtp* test;

    //! ATP Slave bandwidth
    string slaveBandwidth;

    //! ATP Slave latency
    string slaveLatency;

    //! list of loaded ATP files
    list<string> atpFiles;

    //! flag to signal verbose mode on
    bool isVerbose;

    //! flag to signal profiles as masters mode is on
    bool isProfilesAsMasters;

    //! flag to signal Rich Mode on
    bool inRichMode { false };

    //! original terminal configuration for restore on exit
    struct termios origTerm;

    //! Command history for the current Shell session
    vector<string> commandHistory { };
    size_t historyIter { 0 };

    //! protected default constructor
    Shell();

    //! Signal / Exit handlers for the Shell
    static void signalHandler(int signum);
    static void exitHandler();

    /*!
     * Enables Rich Mode if supported. Allows features such as command
     * completion and history
     *\return true if Rich Mode becomes enabled, false otherwise
     */
    bool tryEnableRichMode();


    /*!
     * Disables Rich Mode. Restores the original system terminal attributes
     */
    void disableRichMode();

    /*!
     * Retrieves the next line from user input
     *\param ret resulting line to be interpreted
     *\return true if user indicated more input, false if quitted
     */
    static bool getNextLine(string &ret);

    /*!
     * Checks if provided Command is valid
     *\param cmd Command to check
     *\return true if valid, false otherwise
     */
    bool isValidCommand(const string &cmd) const;

    /*!
     * Checks if provided Command identifies a Command Group
     *\param cmd Command to check
     *\return true if it has SubCommands, false otherwise
     */
    bool isCommandGroup(const string &cmd) const;

    /*
     * Check if provided SubCommand is valid within its Command Group
     *\param cmd Command Group of the SubCommand
     *\param sub_cmd SubCommand to check
     *\return true if valid, false otherwise
     */
    bool isValidSubCommand(const string &cmd, const string &sub_cmd) const;

    /*!
     * Executes a Command with the provided Arguments
     *\param cmd Command to execute
     *\param args Arguments for the Command
     */
    void executeCommand(const string &cmd, const string &args);

    /*!
     * Executes a SubCommand belonging to a Command Group with the provided
     * Arguments
     *\param cmd Command Group of the SubCommand
     *\param sub_cmd SubCommand to execute
     *\param args Arguments for the SubCommand
     */
    void executeCommand(const string &cmd, const string &sub_cmd,
                        const string &args);

    using Completions = vector<reference_wrapper<const string>>;

    /*!
     * Completes given input Line based on current content
     *\param line current input
     */
    void completeLine(Line &line) const;
    void completeCommand(Line &line, const string &cmd) const;
    void completeSubCommand(Line &line, const string &cmd,
                            const string &sub_cmd) const;

    /*!
     * Completes a partial Element if possible
     *\param line Line to put completion on
     *\param elem Element (Command or SubCommand) to complete
     *\param compls Initially available Completions
     */
    void complete(Line &line, const string &elem, Completions& compls) const;

    /*!
     * Inserts the new Command into the History
     *\param new_cmd Command to insert into History
     */
    void updateHistory(const string &new_cmd);

    /*! Displays the previous Command in the History */
    void previousCommand(Line &line);

    /*! Displays the next Command in the History */
    void nextCommand(Line &line);

public:

    /*!
     * Singleton Logger object accessor
     *\return the only instance of the logger object
     */
    static Shell *get();

    //! default destructor
    virtual ~Shell();

    /*!
     * Sets the Test suite pointer
     *\param t pointer to the Test Suite
     */
    inline void setTest(TestAtp* const t) {test=t;}

    //! test function
    void hello(const string& world) const;

    /*!
     * prints the command help and exits
     *\param what command - optional
     */
    void help(const string& what) const;

    /*!
     * Exit the program
     **\param null unused
     */
    inline void quit(const string& null) const {(void)null; exit(0);}

    /*!
     * Loads a .atp file
     *\param file atp file name
     */
    void load(const string& file);

    /*!
     * Tests loaded files against
     * ATP internal slave
     *\param null unused
     */
    void testAgainstSlave(const string& null);

    /*!
     * Enables ATP verbose mode
     *\param null unused
     */
    void verbose(const string& null);

    /*!
     * Flushes loaded ATP profiles
     *\param null unused
     */
    void flush(const string& null);

    /*!
     * Resets the TPM, reloads profiles
     *\param null unused
     */
    void reset(const string& null);

    /*!
     * Toggles ATP Profiles as masters
     *\param null unused
     */
    void profilesAsMasters (const string& null);

    /*!
     * Activates a root profile
     *\param root root profile name
     */
    void streamActivate(const string& root);

    /*!
     * Resets a root profile
     *\param root root profile name
     */
    void streamReset(const string& root);

    /*!
     * Reconfigures a traffic profile stream
     *\param line command line
     */
    void streamReconfigure(const string& line);

    /*!
     * Prints the requested stream status
     *\param root stream root profile name
     */
    void streamStatus(const string& root);

    /*!
     * Creates a unique instance of a stream
     *\param root stream root profile name
     */
    void uniqueStream(const string& root);

    /*!
     * Sets the ATP slave bandwidth
     * and latency
     *\param line command line
     */
    void setSlave(const string& line);

    /*!
     * Shell main command loop
     */
    void loop();

    /// ls SubCommands

    //! lists configured profiles
    void lsProfiles(const string& null) const;

    //! lists configured masters
    void lsMasters(const string& null) const;

    //! lists loaded ATP files
    void lsFiles(const string& null) const;

    //! lists loaded Stream roots
    void lsStreams(const string& null) const;
};

} /* namespace TrafficProfiles */

#endif /* __AMBA_TRAFFIC_PROFILE_SHELL_HH__ */
