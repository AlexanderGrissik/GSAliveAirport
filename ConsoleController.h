#pragma once

#include "Aircraft.h"
#include "ISimConnectHandler.h"
#include "ISimConnectStatus.h"
#include "GSRequests/GSReqCommand.h"
#include "GSRequests/GSReqGroundScan.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace parking_services
{
enum class AppCommandType
{
    None, Quit, Help, Status, Tracked, Aircraft1, Aircraft5, Parked, Ground,
    Reposition, Find, Log, Reset, Reload, Unknown,
};

struct AppCommand
{
    AppCommandType type{AppCommandType::None};
    AircraftId objectId{};
    double relativeX{};
    double relativeY{};
    double relativeZ{};
    double relativeHeadingDegrees{};
    std::string family;
};

struct ConsoleStatus
{
    std::size_t trackedAircraft{};
    std::size_t nearbyAircraft{};
    std::size_t groundObjects{};
    std::size_t createdServices{};
    std::size_t pendingCreates{};
    std::size_t movingObjects{};
    std::size_t maximumMovingObjects{};
};

class ConsoleController final : public ISimConnectStatus
{
  public:
    explicit ConsoleController(ISimConnectHandler &simConnect);
    ~ConsoleController() = default;

    ConsoleController(const ConsoleController &) = delete;
    ConsoleController &operator=(const ConsoleController &) = delete;

    void Pump(const std::function<void(AppCommand)> &handler);
    static AppCommand Parse(std::string_view line);
    static void PrintHelp();
    static void PrintPrompt();
    static void PrintSnapshots(const std::vector<AircraftSnapshot> &aircraft,
                               std::string_view label);
    static void PrintParked(const std::vector<AircraftSnapshot> &aircraft);
    static void PrintGround(const std::vector<GroundSnapshot> &ground);
    void PrintStatus(const ConsoleStatus &status) const;

    void RequestGroundDebugSnapshot();
    void PollGroundDebugSnapshot();
    [[nodiscard]] std::size_t LastGroundCount() const;
    void Reset();

    void OnSimConnected() override;
    void OnSimDisconnected() override {}
    void OnSimStarted() override {}
    void OnSimStopped() override {}
    void OnObjRemoved(std::uint32_t) override {}

  private:
    GSReqCommand &NewSetupRequest();
    void CollectFinishedSetupRequests();

    ISimConnectHandler &m_simConnect;
    std::string m_line;
    std::string m_lastCommand;
    std::size_t m_cursor{};
    std::unique_ptr<GSReqGroundScan> m_groundRequest;
    std::mutex m_setupRequestMutex;
    std::vector<std::unique_ptr<GSReqCommand>> m_setupRequests;
    std::vector<GroundSnapshot> m_lastGroundObjects;
};
} // namespace parking_services
