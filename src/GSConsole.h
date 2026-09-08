#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace NS_GSLiveAirportMSFS
{
class GSConsole final
{
public:

    enum class AppCommandType
    {
        None, Quit, Help, Status, Tracked, Aircraft1, Parked, Ground, Roads, Log, Reload, Unknown,
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
} // namespace NS_GSLiveAirportMSFS
