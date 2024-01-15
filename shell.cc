/*
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2019 ARM Limited
 * All rights reserved
 *  Created on: 3 Jul 2019
 *      Author: Matteo Andreozzi
 */

#include <cctype>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <unistd.h>

#include "shell.hh"
#include "logger.hh"
#include "test_atp.hh"
#include "traffic_profile_desc.hh"
#include "types.hh"
#include "utilities.hh"

namespace TrafficProfiles {

Shell* Shell::instance = nullptr;

Shell* Shell::get() {
    if (instance == nullptr) {
        instance = new Shell();
    }
    return instance;
}

Shell::Shell(): test(nullptr),
        slaveBandwidth("32GB/s"), slaveLatency("80ns"),
        isVerbose(false), isProfilesAsMasters(false) {
    commandHistory.reserve(512);
}

Shell::~Shell() {
    if (instance) delete instance;
}

void Shell::loop() {

    // default shell log level to warnings
    Logger::get()->setLevel(Logger::WARNING_LEVEL);

    // brag
    PROMPT(  "\n "
            "******* ATP ENGINE SHELL ****\n",
            "** by Matteo Andreozzi ****\n",
            "***************************\n",
            " type help for a list of \n",
            " useful commands         \n\n");

    // Enable Rich Mode if supported
    function<bool(string &)> get_next_line { tryEnableRichMode() ?
        Shell::getNextLine :
        [](string &line) {
            PROMPT(""); return static_cast<bool>(getline(cin, line));
        }
    };

    // disable exit on errors
    Logger::get()->setExitOnErrors(false);

    string line, cmd, sub_cmd, args;
    while (get_next_line(line)) {
        if (line.empty()) continue;

        cmd = Utilities::extractHead(line);
        if (!isValidCommand(cmd)) {
            PROMPT("Unsupported command:",cmd,"\n");
            continue;
        }

        args = Utilities::extractTail(line);
        if (isCommandGroup(cmd)) {
            sub_cmd = Utilities::extractHead(args);
            if (!isValidSubCommand(cmd, sub_cmd)) {
                PROMPT("Unsupported sub-command for command group:",cmd,
                       sub_cmd,"\n");
                continue;
            }
            args = Utilities::extractTail(args);
            executeCommand(cmd, sub_cmd, args);
        } else {
            executeCommand(cmd, args);
        }
    }
    exit(0);
}

void
Shell::signalHandler(int signum)
{
    LOG("Shell::signalHandler: signal (",signum,") received");
    cout << endl;
    exit(signum);
}

void
Shell::exitHandler()
{
    // Safely disable Rich Mode
    Shell *shell { Shell::get() };
    if (shell->inRichMode) shell->disableRichMode();
}

bool
Shell::tryEnableRichMode()
{
    if (!isatty(STDIN_FILENO)) {
        ERROR("Shell::tryEnableRichMode: STDIN is not a terminal");
        return false;
    }

    // Store original terminal configuration
    if (tcgetattr(STDIN_FILENO, &origTerm) == -1) {
        ERROR("Shell::tryEnableRichMode: Could not retrieve original "
              "terminal attributes");
        return false;
    }

    struct termios shell_term = origTerm;
    // See termios man page
    shell_term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                            IGNCR | ICRNL | IXON);
    shell_term.c_cflag |= CS8;
    shell_term.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    shell_term.c_cc[VMIN] = 1;
    shell_term.c_cc[VTIME] = 0;

    // Set shell terminal (raw) configuration
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &shell_term) == -1) {
        ERROR("Shell::tryEnableRichMode: Could not configure shell "
              "terminal attributes");
        return false;
    }

    // Register signal / exit handlers
    signal(SIGINT, Shell::signalHandler);
    signal(SIGTSTP, Shell::signalHandler);
    atexit(Shell::exitHandler);

    inRichMode = true;
    return true;
}

void
Shell::disableRichMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTerm) == -1)
        ERROR("Shell::disableRichMode: Could not restore original "
              "terminal attributes");
    else
        inRichMode = false;
}

bool
Shell::getNextLine(string &ret)
{
    Line line { };
    bool more_chars { true };
    unsigned char next_char { };
    Shell *shell { Shell::get() };
    line.refresh();
    do {
        cout.flush();
        next_char = line.getNextChar();
        if (iscntrl(next_char)) {
            switch (next_char) {
                case END_OF_TEXT:
                case END_OF_TRANSMISSION:
                    ret.clear();
                    line.end();
                    return false;
                case BACKSPACE:
                case DELETE:
                    line.backspace();
                    break;
                case HORIZONTAL_TAB:
                    if (line.isCursorAtEnd())
                        shell->completeLine(line);
                    break;
                case LINE_FEED:
                case CARRIAGE_RETURN:
                    more_chars = false;
                    line.end();
                    break;
                case ESCAPE:
                    // Identify ANSI espace sequences
                    if (line.getNextChar() == '[') {
                        switch(line.getNextChar()) {
                            case 'A': shell->previousCommand(line); break;
                            case 'B': shell->nextCommand(line); break;
                            case 'C': line.moveCursorForward(); break;
                            case 'D': line.moveCursorBackward(); break;
                        }
                    }
            }
        } else {
            line.insert(next_char);
        }
    } while (more_chars);

    ret = line.content();
    return true;
}

bool
Shell::isValidCommand(const string &cmd) const
{
    return commands.find(cmd) != commands.end() || isCommandGroup(cmd);
}

bool
Shell::isCommandGroup(const string &cmd) const
{
    return commandGroups.find(cmd) != commandGroups.end();
}

bool
Shell::isValidSubCommand(const string &cmd, const string &sub_cmd) const
{
    const CommandMap &sub_cmds { commandGroups.at(cmd).second };
    return sub_cmds.find(sub_cmd) != sub_cmds.end();
}

void
Shell::executeCommand(const string &cmd, const string &args)
{
    commands.at(cmd).first(args);
    updateHistory(cmd + " " + args);
}

void
Shell::executeCommand(const string &cmd, const string &sub_cmd,
                      const string &args)
{
    const CommandMap &sub_cmds { commandGroups.at(cmd).second };
    sub_cmds.at(sub_cmd).first(args);
    updateHistory(cmd + " " + sub_cmd + " " + args);
}

void
Shell::completeLine(Line &line) const
{
    string cmd { Utilities::extractHead(line.content()) };

    // Line is empty, display help
    if (cmd.empty()) {
        line.end();
        help("help");
        line.refresh();
        return;
    }

    string args { Utilities::extractTail(line.content()) };
    if (!isValidCommand(cmd)) {
        // Guard against malformed Commands with Arguments
        if (!line.endsWith(' ') && args.empty())
            completeCommand(line, cmd);
    } else if (isCommandGroup(cmd)) {
        string sub_cmd { Utilities::extractHead(args) };
        if (!isValidSubCommand(cmd, sub_cmd)) {
            // Guard against malformed SubCommands with Arguments
            args = Utilities::extractTail(args);
            if (args.empty() && (!line.endsWith(' ') || sub_cmd.empty()))
                completeSubCommand(line, cmd, sub_cmd);
        }
    }
}

void
Shell::completeCommand(Line &line, const string &cmd) const
{
    Completions compls { };
    compls.reserve(commands.size() + commandGroups.size());
    for (const auto &comp : commands)
        compls.push_back(comp.first);
    for (const auto &comp : commandGroups)
        compls.push_back(comp.first);
    complete(line, cmd, compls);
}

void
Shell::completeSubCommand(Line &line, const string &cmd,
                          const string &sub_cmd) const
{
    Completions compls { };
    const CommandMap &sub_cmds { commandGroups.at(cmd).second };
    compls.reserve(sub_cmds.size());
    for (const auto &comp : sub_cmds)
        compls.push_back(comp.first);
    complete(line, sub_cmd, compls);
}

void
Shell::complete(Line &line, const string &elem, Completions& compls) const
{
    // Discard non-fitting Completions for the current element
    size_t pos { 0 };
    while (pos < elem.size() && !compls.empty()) {
        auto comp_it = compls.begin();
        while (comp_it != compls.end()) {
            const string &comp = (*comp_it).get();
            if (pos >= comp.size() || elem[pos] != comp[pos])
                comp_it = compls.erase(comp_it);
            else
                ++comp_it;
        }
        ++pos;
    }

    switch (compls.size()) {
        // No Completions available, skip
        case 0:
            break;
        // Exact match, complete the line and display
        case 1:
        {
            const string &comp { compls.front().get() };
            line.append(comp.substr(elem.size()));
            break;
        }
        // Multiple completions, display them
        default:
        {
            stringstream ss_tmp { };
            for (const auto &comp : compls)
                ss_tmp << comp.get() << " ";
            ss_tmp << endl;
            line.end();
            PROMPT(ss_tmp.str());
            line.refresh();
        }
    }
}

void
Shell::updateHistory(const string &new_cmd)
{
    commandHistory.push_back(Utilities::trimOuterCopy(new_cmd));
    historyIter = commandHistory.size();
}

void
Shell::previousCommand(Line &line)
{
    if (!commandHistory.empty() && historyIter) {
        --historyIter;
        line = commandHistory.at(historyIter);
    }
}

void
Shell::nextCommand(Line &line)
{
    if (!commandHistory.empty() && historyIter < (commandHistory.size() - 1)) {
        ++historyIter;
        line = commandHistory.at(historyIter);
    }
}

void
Shell::Line::refresh()
{
    cout << '\r';
    PROMPT("");
    cout << _content;
    _cursorPos = _content.size();
}

void
Shell::Line::refresh(size_t next_cursor_pos)
{
    refresh();
    moveCursorBackward(_content.size() - next_cursor_pos);
}

unsigned char
Shell::Line::getNextChar() const
{
    return static_cast<unsigned char>(cin.get());
}

void
Shell::Line::moveCursorBackward(size_t npos)
{
    if (!_content.empty() && _cursorPos >= npos) {
        for (size_t i { 0 }; i < npos; i++) cout << "\x1b[1D";
        _cursorPos -= npos;
    }
}

void
Shell::Line::moveCursorForward(size_t npos)
{
    if ((_cursorPos + npos) <= _content.size()) {
        for (size_t i { 0 }; i < npos; i++) cout << "\x1b[1C";
        _cursorPos += npos;
    }
}

void
Shell::Line::moveCursorToEnd()
{
    moveCursorForward(_content.size() - _cursorPos);
}

void
Shell::Line::moveCursorToStart()
{
    moveCursorBackward(_cursorPos);
}

void
Shell::Line::insert(const char &c)
{
    const size_t next_cursor_pos = _cursorPos + 1;
    _content.insert(_cursorPos, 1, c);
    refresh(next_cursor_pos);
}

void
Shell::Line::append(const string &s)
{
    if (isCursorAtEnd()) {
        _content += s;
        refresh();
    }
}

void
Shell::Line::backspace()
{
    if (!_content.empty() && _cursorPos) {
        const size_t next_cursor_pos = _cursorPos - 1;
        clear();
        _content.erase(next_cursor_pos, 1);
        refresh(next_cursor_pos);
    }
}

void
Shell::Line::end()
{
    cout << endl;
    _cursorPos = 0;
}

void
Shell::Line::clear()
{
    moveCursorToStart();
    cout << string(_content.size(), ' ');
}

Shell::Line &
Shell::Line::operator=(const string &new_content)
{
    clear();
    _content = new_content;
    refresh();
    return *this;
}

const string &
Shell::Line::content() const
{
    return _content;
}

bool
Shell::Line::isCursorAtEnd() const
{
    return _cursorPos == _content.size();
}

bool
Shell::Line::endsWith(const char &c) const
{
    return !_content.empty() && _content.back() == c;
}

/// shell commands ///

void Shell::help(const string& what) const {
    if (what != "help" && isValidCommand(what)) {
        if (isCommandGroup(what)) {
            PROMPT(commandGroups.at(what).first,"\n");
            const CommandMap &sub_cmds { commandGroups.at(what).second };
            for (const auto& lc : sub_cmds)
                PROMPT(lc.first,":",lc.second.second,"\n");
        } else {
            PROMPT(commands.at(what).second,"\n");
        }
    } else {
        for (const auto& h: commands) {
            PROMPT(h.first,":",h.second.second,"\n");
        }
        for (const auto &cmd_grp : commandGroups)
            PROMPT(cmd_grp.first,":",cmd_grp.second.first,"\n");
    }
}

void Shell::load(const string& file) {
    if (test) {
        if (test->buildManager_fromFile(file)){
            atpFiles.push_back(file);
            PROMPT("Loaded file",file,"\n");
        }
    }
}
void Shell::testAgainstSlave(const string& null) {
    (void)null;
    test->testAgainstInternalSlave(slaveBandwidth, slaveLatency);
}

void Shell::reset(const string& null) {
    (void)null;
    test->getTpm()->reset();
    PROMPT("TPM has been reset\n");
}

void Shell::verbose(const string& null) {
    (void) null;
    // toggle mode first
    isVerbose = !isVerbose;

    if (isVerbose) {
        Logger::get()->setLevel(Logger::DEBUG_LEVEL);
    } else {
        Logger::get()->setLevel(Logger::ERROR_LEVEL);
    }
    PROMPT("Verbose mode",isVerbose?"on":"off","\n");
}

void Shell::profilesAsMasters(const string& null) {
    (void) null;
    isProfilesAsMasters = !isProfilesAsMasters;

    if (isProfilesAsMasters) {
        test->getTpm()->enableProfilesAsMasters();
    } else {
        test->getTpm()->disableProfilesAsMasters();
    }
    PROMPT("Profiles as masters mode",
            isProfilesAsMasters?"on.":"off.","Resetting TPM\n");
    test->getTpm()->reset();
}

void Shell::streamActivate(const string& root) {
    uint64_t rootId = test->getTpm()->profileId(root);
    auto* profile = test->getTpm()->getProfile(rootId);
    if (profile) {
        profile->activate();
        PROMPT("Activated stream",root,"\n");
    }
}

void Shell::streamReset(const string& root) {
    uint64_t rootId = test->getTpm()->profileId(root);
    test->getTpm()->streamReset(rootId);
    PROMPT("Reset stream",root,"\n");
}

void Shell::setSlave(const string& line) {
    if (line != "slave") {
        stringstream ss(line);
        string tempBandwidth="", tempLatency="";
        ss >> tempBandwidth >> tempLatency;
        if (Utilities::timeToHz<double>(tempLatency) &&
                Utilities::toRate<double>(tempBandwidth).first > 0) {
            slaveBandwidth = tempBandwidth;
            slaveLatency = tempLatency;
        } else {
            PROMPT("Unrecognised slave configuration",line,"\n");
        }
    } else {
        PROMPT("Current slave configuration",
                slaveBandwidth, slaveLatency,"\n");
    }
}

void Shell::streamReconfigure(const string& line) {
    string root="", base_string,
            range_string, type_string="NONE";
    uint64_t base=0;
    double range=0;

    // parse the command line
    stringstream ss(line);
    ss >> root >> base_string >> range_string >> type_string;

    // convert base and range
    base = stoull(base_string,0,0);
    range = Utilities::toBytes<double>(range_string);
    // parse the optional type
    Profile::Type type = Profile::NONE;
    Profile_Type_Parse(type_string, &type);

    const uint64_t rootId = test->getTpm()->profileId(root);

    if (rootId < numeric_limits<uint64_t>::max()) {
        auto* profile = test->getTpm()->getProfile(rootId);

        if (profile) {
            test->getTpm()->addressStreamReconfigure(rootId,base,range,type);
            PROMPT("Stream",root,"reconfigured with base",base_string,
                    "range",range_string,
                    (type!=Profile::NONE?"filtering non-"+
                            type_string+" profiles\n":"\n"));
        }
    }
}

void Shell::streamStatus(const string& root) {
    const uint64_t rootId = test->getTpm()->profileId(root);

    if (rootId < numeric_limits<uint64_t>::max()) {
        const auto& stream = test->getTpm()->getStreamCache();
        if (stream.find(rootId)!=stream.end()) {
            for (const auto& node: stream.at(rootId)) {
                PROMPT(test->getTpm()->getProfile(node.first)->getName(),
                        node.first==rootId?
                                "root":(node.second?"leaf":"intermediate"),"\n");
            }
            const bool terminated = test->getTpm()->streamTerminated(rootId);
            PROMPT("The stream is",terminated?"terminated\n":"not terminated\n");
        }
    }
}

void Shell::uniqueStream(const string& line) {
    istringstream argsSet(line);
    vector<string> args { istream_iterator<string> { argsSet },
                          istream_iterator<string> { } };
    const uint64_t rootId { test->getTpm()->profileId(args.at(0)) };
    if (isValid(rootId)) {
        uint64_t cloneRootId { InvalidId<uint64_t>() };
        if (args.size() > 1) {
            const uint64_t masterId { test->getTpm()->masterId(args.at(1)) };
            if (isValid(masterId))
                cloneRootId = test->getTpm()->uniqueStream(rootId, masterId);
        } else {
            cloneRootId = test->getTpm()->uniqueStream(rootId);
        }
        if (isValid(cloneRootId)) {
            if (rootId == cloneRootId)
                PROMPT("Using default stream instance with ID",rootId,"\n");
            else
                PROMPT("Cloned stream instance ID",cloneRootId,"\n");
        }
    }
}

void Shell::flush(const string& null) {
    (void) null;
    test->getTpm()->flush();
    atpFiles.clear();
}

void Shell::lsProfiles(const string& null) const {
    (void) null;
    for (const auto&p: test->getTpm()->getProfileMap()) {
        PROMPT(p.first,"\n");
    }
}

void Shell::lsMasters(const string& null) const {
    (void) null;
    for (const auto&m: test->getTpm()->getMasters()) {
        PROMPT(m,"\n");
    }
}

void Shell::lsStreams(const string& null) const {
    (void) null;
    for (const auto&s: test->getTpm()->getStreamCache()) {
        PROMPT(test->getTpm()->getProfile(s.first)->getName(),"\n");
    }
}

void Shell::lsFiles(const string& null) const {
    (void) null;
    for (const auto& f: atpFiles) {
        PROMPT(f,"\n");
    }
}

void Shell::hello(const string& world) const {
    PROMPT("The world is",world,"\n");
    PROMPT("    (        )\n");
    PROMPT("    O        O\n");
    PROMPT("    ()      ()\n");
    PROMPT("     Oo.nn.oO\n");
    PROMPT("      _mmmm_\n");
    PROMPT("    \\/_mmmm_\\/\n");
    PROMPT("    \\/_mmmm_\\/\n");
    PROMPT("    \\/_mmmm_\\/\n");
    PROMPT("    \\/ mmmm \\/\n");
    PROMPT("        nn\n");
    PROMPT("        ()\n");
    PROMPT("        ()\n");
    PROMPT("         ()    /\n");
    PROMPT("     mat  ()__()\n");
    PROMPT("           '--'\n");
}

template<typename Impl>
Shell::Command
Shell::makeCommand(Impl impl, const string &desc) {
    return make_pair([this, impl](const string& input) {
        (this->*impl)(input);
    }, desc);
}

template<typename Impl>
Shell::Command
Shell::makeTpmCommand(Impl impl, const string &desc) {
    return make_pair([this, impl](const string& input) {
        if (test && test->getTpm()) (this->*impl)(input);
    }, desc);
}

} /* namespace TrafficProfiles */
