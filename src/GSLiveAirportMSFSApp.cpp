#include "GSLiveAirportMSFSApp.h"
#include "GSLogStream.h"
#include "GSCatalog.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace NS_GSLiveAirportMSFS
{
using namespace std::chrono_literals;

int GSLiveAirportMSFSApp::Run()
{
    GSCatalog::GetInstance().LoadCatalog();

    m_aircraftTracker.Start();
    m_spawner.Start();

    std::cout << "GSLiveAirportMSFS for MSFS 2024\n"
              << "Automatic aircraft tracking: dynamic at 1 km, discover to 5 km.\n"
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
    case AT::Status:
        break;
    case AT::Tracked:
        PostACTrackCommandAndWait(GSDefinitions::CMD_PRINT_AIRCRAFT_ALL);
        break;
    case AT::Aircraft1:
        PostACTrackCommandAndWait(GSDefinitions::CMD_PRINT_AIRCRAFT_1KM);
        break;
    case AT::Parked:
        PostACTrackCommandAndWait(GSDefinitions::CMD_PRINT_AIRCRAFT_PARKED);
        break;
    case AT::Ground:
        break;
    case AT::Roads:
        break;
    case AT::Log: {
        const bool enabled = !GSLogStream::LoggingEnabled();
        GSLogStream::SetLoggingEnabled(enabled);
        std::cout << "Logging " << (enabled ? "enabled" : "disabled");
        break;
    }
    case AT::Reload:
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
    NS_GSLiveAirportMSFS::GSLiveAirportMSFSApp app;
    return app.Run();
}