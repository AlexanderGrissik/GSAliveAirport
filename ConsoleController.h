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
    None, Quit, Help, Status, Tracked, Aircraft1, Aircraft5, Parked, Ground,
    StartProbe, StopProbe, Reset, Catalog, Unknown,
};

struct AppCommand
{
    AppCommandType type{AppCommandType::None};
    std::optional<std::uint32_t> objectId;
};

struct ConsoleStatus
{
    bool connected{};
    std::size_t trackedAircraft{};
    std::size_t nearbyAircraft{};
    std::size_t groundObjects{};
    std::size_t createdServices{};
    std::size_t pendingCreates{};
    std::size_t walkingWorkers{};
    std::size_t maximumWalkingWorkers{};
    bool animationProbeActive{};
    std::uint32_t animationProbeObjectId{};
    std::size_t animationProbeSamples{};
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
