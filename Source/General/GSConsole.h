// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace NS_GSAliveAirport
{
class GSConsole final
{
public:

    enum class AppCommandType
    {
        None, Quit, Help, Tracked, Tracked_1km, Parked, Log, Debug, Test, Unknown,
    };

    struct AppCommand
    {
        AppCommandType type{AppCommandType::None};
        std::string argument;
    };

    explicit GSConsole() {}
    ~GSConsole() = default;

    GSConsole(const GSConsole &) = delete;
    GSConsole &operator=(const GSConsole &) = delete;

    void Pump(const std::function<void(AppCommand)> &handler);
    static AppCommand Parse(std::string_view line);
    static void PrintHelp();
    static void PrintPrompt();

private:

    static std::optional<std::string> ReadArgument(std::istringstream &input);
    static AppCommand GetCommandWithArg(std::istringstream &input, AppCommandType type);

    std::string m_line;
    std::string m_lastCommand;
    std::size_t m_cursor{};
};
} // namespace NS_GSAliveAirport
