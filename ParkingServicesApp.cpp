#include "ParkingServicesApp.h"

#include "GSCommon.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

ParkingServicesApp::ParkingServicesApp()
    : m_aircraftTracker(m_simConnect),
      m_animation(m_simConnect, m_aircraftTracker),
      m_groundServices(m_simConnect, m_aircraftTracker, m_animation,
                       GroundServicesConfig::LoadDefault()),
      m_console(m_simConnect)
{
    // Register every status observer before any thread starts, so the SimConnect dispatch
    // thread can never observe a half-populated observer list (registration is lock-free).
    m_simConnect.RegisterStatusObserver(&m_groundServices);
    m_simConnect.RegisterStatusObserver(&m_aircraftTracker);
    m_simConnect.RegisterStatusObserver(&m_console);
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
    m_simConnect.Start();
    m_aircraftTracker.Start();
    m_animation.Start();
    m_groundServices.Start();

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
        m_console.PollGroundDebugSnapshot();
        std::this_thread::sleep_for(10ms);
    }
    return 0;
}

void ParkingServicesApp::HandleCommand(AppCommand command)
{
    std::vector<AircraftSnapshot> aircraftSnapshot;
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
        m_console.PrintStatus({m_aircraftTracker.TrackedCount(), m_aircraftTracker.NearbyCount(),
                               m_console.LastGroundCount(), services.createdObjects,
                               services.pendingCreates, animation.movingObjects,
                               AnimationThread::MaximumMovingObjects});
        break;
    }
    case AppCommandType::Tracked:
        m_aircraftTracker.FillTrackedAircraftSnapshot(aircraftSnapshot);
        ConsoleController::PrintSnapshots(aircraftSnapshot, "tracked aircraft");
        break;
    case AppCommandType::Aircraft1:
        m_aircraftTracker.FillNearbyAircraftSnapshot(
            aircraftSnapshot, AircraftTrackerThread::DiscoveryRadiusMeters);
        ConsoleController::PrintSnapshots(aircraftSnapshot,
                                           "aircraft within 1 km");
        break;
    case AppCommandType::Aircraft5:
        m_aircraftTracker.FillNearbyAircraftSnapshot(aircraftSnapshot);
        ConsoleController::PrintSnapshots(aircraftSnapshot,
                                           "aircraft within 5 km");
        break;
    case AppCommandType::Parked:
        m_aircraftTracker.FillNearbyAircraftSnapshot(aircraftSnapshot);
        ConsoleController::PrintParked(aircraftSnapshot);
        break;
    case AppCommandType::Ground:
        m_console.RequestGroundDebugSnapshot();
        break;
    case AppCommandType::Reposition:
        m_groundServices.RepositionStaticObject(
            command.objectId, command.relativeX, command.relativeY,
            command.relativeZ, command.relativeHeadingDegrees);
        break;
    case AppCommandType::Find:
        m_groundServices.FindClosestRootObject(std::move(command.family));
        break;
    case AppCommandType::Log: {
        const bool enabled = !GSLoggingEnabled();
        GSSetLoggingEnabled(enabled);
        std::cout << "Background logging "
                  << (enabled ? "enabled" : "disabled") << ".\n";
        break;
    }
    case AppCommandType::Reset:
        ResetEverything();
        break;
    case AppCommandType::Reload:
        m_groundServices.ReloadConfiguration();
        break;
    case AppCommandType::Unknown:
        std::cout << "Unknown command or invalid arguments. Type 'help'.\n";
        break;
    }
}

void ParkingServicesApp::ResetEverything()
{
    m_groundServices.Reset();
    m_aircraftTracker.Reset();
    m_console.Reset();
    GSPrint("Reset requested for aircraft tracking and ground services.");
}
} // namespace parking_services

int RunParkingServices()
{
    parking_services::ParkingServicesApp app;
    return app.Run();
}
