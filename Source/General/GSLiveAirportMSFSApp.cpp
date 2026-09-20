#include "GSLiveAirportMSFSApp.h"
#include "GSLogStream.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace NS_GSLiveAirportMSFS
{
using namespace std::chrono_literals;

int GSLiveAirportMSFSApp::Run()
{
    m_aircraftTracker.Start();
    m_spawner.Start();
    m_animThread.Start();
    m_mvmntThread.Start();

    std::cout << "Ground Services Live Airport for MSFS 2024\n"
              << "Version: 1.0\n"
              << "Type 'help' for commands.\n";

    GSConsole::PrintPrompt();
    while (!m_quit.load()) {
        m_console.Pump([this](GSConsole::AppCommand command) {
            HandleCommand(std::move(command));
            if (!m_quit.load()) GSConsole::PrintPrompt();
        });
        std::this_thread::sleep_for(50ms);
    }
    return 0;
}

void GSLiveAirportMSFSApp::HandleCommand(GSConsole::AppCommand command)
{
    using AT = GSConsole::AppCommandType;
    switch (command.type) {
    case AT::None:
        break;
    case AT::Quit:
        m_quit.store(true);
        break;
    case AT::Help:
        GSConsole::PrintHelp();
        break;
    case AT::Tracked:
        PostACTrackCommandAndWait(GSDefinitions::CMD_PRINT_AIRCRAFT_ALL);
        break;
    case AT::Tracked_1km:
        PostACTrackCommandAndWait(GSDefinitions::CMD_PRINT_AIRCRAFT_1KM);
        break;
    case AT::Parked:
        PostACTrackCommandAndWait(GSDefinitions::CMD_PRINT_AIRCRAFT_PARKED);
        break;
    case AT::Log: {
        const bool enabled = !GSLogStream::LoggingEnabled();
        GSLogStream::SetLoggingEnabled(enabled);
        std::cout << "Logging " << (enabled ? "enabled\n" : "disabled\n");
        break;
    }
    case AT::Test:
        PostACTrackCommandAndWait(GSDefinitions::CMD_SPAWN_TEST_AIRCRAFT);
        break;
    case AT::Unknown:
        std::cout << "Unknown command or invalid arguments. Type 'help'";
        break;
    }
}

void GSLiveAirportMSFSApp::PostACTrackCommandAndWait(int cmdId) 
{
    GSCmdQueue replyQueue; 
    CmdPtr cmd = std::make_unique<GSCommand>(cmdId, replyQueue);
    m_aircraftTracker.PostCommand(cmd);
    replyQueue.Pop();
}
} // namespace NS_GSLiveAirportMSFS

int main()
{
    std::unique_ptr<NS_GSLiveAirportMSFS::GSLiveAirportMSFSApp> app(new NS_GSLiveAirportMSFS::GSLiveAirportMSFSApp());
    return app->Run();
}