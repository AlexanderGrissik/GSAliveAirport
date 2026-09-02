#include "ParkingServicesApp.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

ParkingServicesApp::ParkingServicesApp()
    : m_groundServicesConfig(GroundServicesConfig::LoadDefault()),
      m_simConnect([this](std::string message) { LogLine(std::move(message)); }),
      m_aircraftTracker(m_simConnect,
                        [this](std::string message) { LogLine(std::move(message)); }),
      m_animation(m_simConnect, m_aircraftTracker,
                  [this](std::string message) { LogLine(std::move(message)); }),
      m_groundServices(m_simConnect, m_aircraftTracker, m_animation,
                       m_groundServicesConfig,
                       [this](std::string message) { LogLine(std::move(message)); })
{
    for (const std::string &message : m_groundServicesConfig.StartupMessages()) {
        LogLine(message);
    }
}

ParkingServicesApp::~ParkingServicesApp()
{
    m_groundServices.Stop();
    m_animation.Stop();
    m_aircraftTracker.Stop();
    m_simConnect.Stop();
}

int ParkingServicesApp::Run()
{
    std::cout << "ParkingServices for MSFS 2024\n"
              << "Automatic aircraft tracking: discover at 1 km, retain to 5 km.\n"
              << "Type 'help' for commands.\n";
    ConsoleController::PrintPrompt();
    while (!m_quit.load()) {
        m_console.Pump([this](AppCommand command) {
            const bool waitsForGroundResult = command.type == AppCommandType::Ground;
            HandleCommand(std::move(command));
            if (!m_quit.load() && !waitsForGroundResult) ConsoleController::PrintPrompt();
        });
        PollGroundDebugSnapshot();
        std::this_thread::sleep_for(10ms);
    }
    return 0;
}

void ParkingServicesApp::HandleCommand(AppCommand command)
{
    switch (command.type) {
    case AppCommandType::None:
        break;
    case AppCommandType::Quit:
        m_quit.store(true);
        break;
    case AppCommandType::Help:
        ConsoleController::PrintHelp();
        break;
    case AppCommandType::Status: {
        const auto services = m_groundServices.Status();
        const auto animation = m_animation.Status();
        m_console.PrintStatus({m_simConnect.IsConnected(), m_aircraftTracker.TrackedCount(),
                               m_aircraftTracker.NearbyCount(), m_lastGroundObjects.size(),
                               services.createdObjects, services.pendingCreates,
                               animation.walkingWorkers,
                               AnimationThread::MaximumWalkingWorkers,
                               animation.probeActive, animation.probeObjectId,
                               animation.probeSamples});
        break;
    }
    case AppCommandType::Tracked:
        m_aircraftTracker.FillTrackedAircraftSnapshot(m_aircraftSnapshotBuffer);
        ConsoleController::PrintSnapshots(m_aircraftSnapshotBuffer, "tracked aircraft");
        break;
    case AppCommandType::Aircraft1:
        m_aircraftTracker.FillNearbyAircraftSnapshot(
            m_aircraftSnapshotBuffer, AircraftTrackerThread::DiscoveryRadiusMeters);
        ConsoleController::PrintSnapshots(m_aircraftSnapshotBuffer,
                                           "aircraft within 1 km");
        break;
    case AppCommandType::Aircraft5:
        m_aircraftTracker.FillNearbyAircraftSnapshot(m_aircraftSnapshotBuffer);
        ConsoleController::PrintSnapshots(m_aircraftSnapshotBuffer,
                                           "aircraft within 5 km");
        break;
    case AppCommandType::Parked:
        m_aircraftTracker.FillNearbyAircraftSnapshot(m_aircraftSnapshotBuffer);
        ConsoleController::PrintParked(m_aircraftSnapshotBuffer);
        break;
    case AppCommandType::Ground:
        RequestGroundDebugSnapshot();
        break;
    case AppCommandType::StartProbe:
        if (command.objectId) m_animation.StartProbe(*command.objectId);
        break;
    case AppCommandType::StopProbe:
        m_animation.StopProbe();
        break;
    case AppCommandType::Reset:
        ResetEverything();
        break;
    case AppCommandType::Catalog:
        m_simConnect.RequestCatalog();
        break;
    case AppCommandType::Unknown:
        std::cout << "Unknown command or invalid ObjectID. Type 'help'.\n";
        break;
    }
}

void ParkingServicesApp::RequestGroundDebugSnapshot()
{
    if (m_groundRequest) {
        std::cout << "A ground-object debug request is already in progress.\n";
        return;
    }
    m_groundRequest.emplace(m_simConnect.RequestGroundObjects());
    std::cout << "Requested the current 5 km ground-object debug snapshot.\n";
}

void ParkingServicesApp::PollGroundDebugSnapshot()
{
    if (!m_groundRequest ||
        m_groundRequest->wait_for(0ms) != std::future_status::ready) return;
    GroundScanResult result = m_groundRequest->get();
    m_groundRequest.reset();
    if (!result.succeeded) {
        std::cout << "Ground-object debug request failed or timed out.\n";
        ConsoleController::PrintPrompt();
        return;
    }
    m_lastGroundObjects = std::move(result.objects);
    ConsoleController::PrintGround(m_lastGroundObjects);
    ConsoleController::PrintPrompt();
}

void ParkingServicesApp::ResetEverything()
{
    m_groundServices.Reset();
    m_animation.Reset();
    m_aircraftTracker.Reset();
    m_lastGroundObjects.clear();
    LogLine("Reset requested for aircraft tracking, ground services, and animation.");
}

void ParkingServicesApp::LogLine(std::string message) const
{
    std::scoped_lock lock(m_outputMutex);
    std::cout << "\n[probe] " << message << '\n';
}
} // namespace parking_services

int RunParkingServices()
{
    parking_services::ParkingServicesApp app;
    return app.Run();
}
