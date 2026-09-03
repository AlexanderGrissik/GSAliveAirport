#pragma once

#include "AircraftTrackerThread.h"
#include "AnimationThread.h"
#include "ConsoleController.h"
#include "GroundServicesThread.h"
#include "GroundServicesConfig.h"
#include "GSRequests/GSReqCommand.h"
#include "GSRequests/GSReqGroundScan.h"
#include "ISimConnectStatus.h"
#include "SimConnectThread.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace parking_services
{
class ParkingServicesApp final : public ISimConnectStatus
{
  public:
    ParkingServicesApp();
    ~ParkingServicesApp();

    ParkingServicesApp(const ParkingServicesApp &) = delete;
    ParkingServicesApp &operator=(const ParkingServicesApp &) = delete;

    int Run();

    void OnSimConnected() override;
    void OnSimDisconnected() override {}
    void OnSimStarted() override {}
    void OnSimStopped() override {}
    void OnObjRemoved(std::uint32_t) override {}

  private:
    void HandleCommand(AppCommand command);
    void RequestGroundDebugSnapshot();
    void PollGroundDebugSnapshot();
    GSReqCommand &NewSetupRequest();
    void CollectFinishedSetupRequests();
    void ResetEverything();
    void LogLine(std::string message) const;

    std::atomic_bool m_quit{false};
    mutable std::mutex m_outputMutex;
    SimConnectThread m_simConnect;
    AircraftTrackerThread m_aircraftTracker;
    AnimationThread m_animation;
    GroundServicesThread m_groundServices;
    ConsoleController m_console;
    std::unique_ptr<GSReqGroundScan> m_groundRequest;
    std::mutex m_setupRequestMutex;
    std::vector<std::unique_ptr<GSReqCommand>> m_setupRequests;
    std::vector<GroundSnapshot> m_lastGroundObjects;
    std::vector<AircraftSnapshot> m_aircraftSnapshotBuffer;
};
} // namespace parking_services

int RunParkingServices();
