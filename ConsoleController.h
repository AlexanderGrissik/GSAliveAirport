#pragma once

#include "Aircraft.h"
#include "GroundObject.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace parking_services
{
enum class AppCommandType
{
    None, Quit, Help, Status, Tracked, Aircraft1, Aircraft5, Parked, Ground, Reset, Unknown,
};

struct AppCommand
{
    AppCommandType type{AppCommandType::None};
};

struct ConsoleStatus
{
    std::size_t trackedAircraft{};
    std::size_t nearbyAircraft{};
    std::size_t groundObjects{};
    std::size_t createdServices{};
    std::size_t pendingCreates{};
    std::size_t walkingWorkers{};
    std::size_t maximumWalkingWorkers{};
};

class ConsoleController final
{
  public:
    void Pump(const std::function<void(AppCommand)> &handler);
    static AppCommand Parse(std::string_view line);
    static void PrintHelp();
    static void PrintPrompt();
    static void PrintSnapshots(const std::vector<AircraftSnapshot> &aircraft,
                               std::string_view label);
    static void PrintParked(const std::vector<AircraftSnapshot> &aircraft);
    static void PrintGround(const std::vector<GroundSnapshot> &ground);
    void PrintStatus(const ConsoleStatus &status) const;

  private:
    std::string m_line;
};
} // namespace parking_services
