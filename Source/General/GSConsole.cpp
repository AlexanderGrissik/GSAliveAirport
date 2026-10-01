// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSConsole.h"

#include "GSLogStream.h"

#include <conio.h>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace NS_GSLiveAirportMSFS
{

void GSConsole::Pump(const std::function<void(AppCommand)> &handler)
{
    while (_kbhit()) {
        const int key = _getch();
        if (key == 0 || key == 224) {
            const int extendedKey = _getch();
            if (extendedKey == 72 && !m_lastCommand.empty()) {
                for (std::size_t index = 0; index < m_cursor; ++index) {
                    std::cout << '\b';
                }
                std::cout << std::string(m_line.size(), ' ');
                for (std::size_t index = 0; index < m_line.size(); ++index) {
                    std::cout << '\b';
                }
                m_line = m_lastCommand;
                m_cursor = m_line.size();
                std::cout << m_line << std::flush;
            } else if (extendedKey == 75 && m_cursor != 0) {
                --m_cursor;
                std::cout << '\b' << std::flush;
            } else if (extendedKey == 77 && m_cursor < m_line.size()) {
                std::cout << m_line[m_cursor++] << std::flush;
            }
            continue;
        }
        if (key == '\r') {
            std::cout << m_line.substr(m_cursor) << '\n';
            const std::string line = std::exchange(m_line, {});
            m_cursor = 0;
            if (!line.empty()) m_lastCommand = line;
            handler(Parse(line));
        } else if (key == '\b') {
            if (m_cursor != 0) {
                m_line.erase(--m_cursor, 1);
                const std::string_view suffix(m_line.data() + m_cursor,
                                              m_line.size() - m_cursor);
                std::cout << '\b' << suffix << ' ';
                for (std::size_t index = 0; index <= suffix.size(); ++index) {
                    std::cout << '\b';
                }
                std::cout << std::flush;
            }
        } else if (key >= 32 && key <= 126) {
            const char character = static_cast<char>(key);
            m_line.insert(m_cursor, 1, character);
            const std::string_view suffix(m_line.data() + m_cursor,
                                          m_line.size() - m_cursor);
            ++m_cursor;
            std::cout << suffix;
            for (std::size_t index = 1; index < suffix.size(); ++index) {
                std::cout << '\b';
            }
            std::cout << std::flush;
        }
    }
}

GSConsole::AppCommand GSConsole::Parse(std::string_view line)
{
    std::istringstream input{std::string(line)};
    std::string command;
    input >> command;
    command = GSLogStream::Lower(std::move(command));
    if (command.empty()) return {};
    if (command == "quit") return {AppCommandType::Quit};
    if (command == "help") return {AppCommandType::Help};
    if (command == "tracked") return {AppCommandType::Tracked};
    if (command == "tracked_1km") return {AppCommandType::Tracked_1km};
    if (command == "parked") return {AppCommandType::Parked};
    if (command == "log") return {AppCommandType::Log};
    if (command == "debug") {
        AppCommand debug{AppCommandType::Debug};
        std::getline(input >> std::ws, debug.argument);
        return debug;
    }
    if (command == "test") return {AppCommandType::Test};
    return {AppCommandType::Unknown};
}

void GSConsole::PrintHelp()
{
    GSLogStream::Print(
              "Commands:\n"
              "  tracked             Print all detected aircrafts\n"
              "  tracked_1km         Print detected aircrafts currently within 1 km\n"
              "  parked              Print detected parked aircrafts\n"
              "  log                 Toggle background event/periodic logging\n"
              "  debug               Show available debug loggers and their state\n"
              "  debug XXX,YYY       Enable only the named loggers (case-insensitive)\n"
              "  debug all|off       Enable or disable all debug loggers\n"
              "  test                Spawn all size of parked aircrafts once near the user\n"
              "  quit                Close the application\n"
              "  help                This help\n");
}

void GSConsole::PrintPrompt()
{
    GSLogStream::Print("> ");
}

std::optional<std::string> GSConsole::ReadArgument(std::istringstream &input)
{
    std::string token;
    if (input >> token && !token.empty()) return token;
    return std::nullopt;
}

GSConsole::AppCommand GSConsole::GetCommandWithArg(std::istringstream &input, AppCommandType type)
{
    auto arg = ReadArgument(input);
    if (!arg) return {AppCommandType::Unknown};
    std::ranges::transform(*arg, arg->begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    AppCommand cmd;
    cmd.type = type;
    cmd.argument = std::move(*arg);
    return cmd;
}

} // namespace NS_GSLiveAirportMSFS
