#pragma once

#include "AircraftTrackerThread.h"
#include "AnimationThread.h"
#include "ConsoleController.h"
#include "GroundServicesThread.h"
#include "GroundServicesConfig.h"
#include "SimConnectThread.h"

#include <atomic>

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

    std::atomic_bool m_quit{false};
    SimConnectThread m_simConnect;
    AircraftTrackerThread m_aircraftTracker;
    AnimationThread m_animation;
    GroundServicesThread m_groundServices;
    ConsoleController m_console;
};
} // namespace parking_services

int RunParkingServices();
