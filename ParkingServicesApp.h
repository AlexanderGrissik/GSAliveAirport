#pragma once

#include "AircraftTrackerThread.h"
#include "AnimationThread.h"
#include "ConsoleController.h"
#include "GroundServicesThread.h"
#include "GroundServicesConfig.h"
#include "SimConnectThread.h"

#include <atomic>
#include <future>
#include <mutex>
#include <optional>
#include <vector>

namespace parking_services
{
class ParkingServicesApp final
{
  public:
    ParkingServicesApp();
    ~ParkingServicesApp();

    ParkingServicesApp(const ParkingServicesApp &) = delete;
    ParkingServicesApp &operator=(const ParkingServicesApp &) = delete;

    int Run();

  private:
    void HandleCommand(AppCommand command);
    void RequestGroundDebugSnapshot();
    void PollGroundDebugSnapshot();
    void ResetEverything();
    void LogLine(std::string message) const;

    std::atomic_bool m_quit{false};
    mutable std::mutex m_outputMutex;
    GroundServicesConfig m_groundServicesConfig;
    SimConnectThread m_simConnect;
    AircraftTrackerThread m_aircraftTracker;
    AnimationThread m_animation;
    GroundServicesThread m_groundServices;
    ConsoleController m_console;
    std::optional<std::future<GroundScanResult>> m_groundRequest;
    std::vector<GroundSnapshot> m_lastGroundObjects;
    std::vector<AircraftSnapshot> m_aircraftSnapshotBuffer;
};
} // namespace parking_services

int RunParkingServices();
