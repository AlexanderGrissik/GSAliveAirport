#pragma once

#include "AircraftTrackerThread.h"
#include "AnimationThread.h"
#include "ConsoleController.h"
#include "GroundServicesThread.h"
#include "GroundServicesConfig.h"
#include "SimConnectThread.h"

#include <atomic>
#include <mutex>

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
    void ResetEverything();
    void LogLine(std::string message) const;

    std::atomic_bool m_quit{false};
    mutable std::mutex m_outputMutex;
    SimConnectThread m_simConnect;
    AircraftTrackerThread m_aircraftTracker;
    AnimationThread m_animation;
    GroundServicesThread m_groundServices;
    ConsoleController m_console;
};
} // namespace parking_services

int RunParkingServices();
