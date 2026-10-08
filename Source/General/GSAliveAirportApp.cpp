// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAliveAirportApp.h"
#include "GSLogStream.h"

#include <sstream>
#include <utility>

namespace NS_GSAliveAirport
{
int GSAliveAirportApp::Run()
{
    GSLogStream::SetSink([this](std::string_view text) { m_ui.AppendLog(text); });

    if (!m_ui.Initialize([this](std::string_view commandLine) { HandleCommand(ParseCommand(commandLine)); })) {
        MessageBoxW(nullptr, L"Unable to create the application window.", L"GSAliveAirport", MB_OK | MB_ICONERROR);
        return 1;
    }

    m_aircraftTracker.Start();
    m_spawner.Start();
    m_animThread.Start();
    m_mvmntThread.Start();

    GSLogStream::Print("=================================================");
    GSLogStream::Print("GSAliveAirport - Alive Airport Ground Services");
    GSLogStream::Print(std::string("For MSFS24. Version: ") + s_Version);
    GSLogStream::Print("=================================================");

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    m_ui.Shutdown();
    return static_cast<int>(message.wParam);
}

GSAliveAirportApp::AppCommand GSAliveAirportApp::ParseCommand(std::string_view line)
{
    std::istringstream input{std::string(line)};
    std::string command;
    input >> command;
    command = GSLogStream::Lower(std::move(command));
    if (command.empty()) return {};
    if (command == "tracked") return {AppCommandType::Tracked};
    if (command == "tracked_1km") return {AppCommandType::Tracked_1km};
    if (command == "parked") return {AppCommandType::Parked};
    if (command == "log") return {AppCommandType::Log};
    if (command == "debug") {
        AppCommand debug{AppCommandType::Debug};
        std::getline(input >> std::ws, debug.argument);
        return debug;
    }
    if (command == "test") return {AppCommandType::Test};
    return {};
}

void GSAliveAirportApp::HandleCommand(AppCommand command)
{
    using AT = AppCommandType;
    switch (command.type) {
    case AT::None:
        break;
    case AT::Tracked:
        PostACTrackCommand(GSDefinitions::CMD_PRINT_AIRCRAFT_ALL);
        break;
    case AT::Tracked_1km:
        PostACTrackCommand(GSDefinitions::CMD_PRINT_AIRCRAFT_1KM);
        break;
    case AT::Parked:
        PostACTrackCommand(GSDefinitions::CMD_PRINT_AIRCRAFT_PARKED);
        break;
    case AT::Log: {
        const bool enabled = !GSLogStream::LoggingEnabled();
        GSLogStream::SetLoggingEnabled(enabled);
        GSLogStream::Print(enabled ? "Logging enabled" : "Logging disabled");
        break;
    }
    case AT::Debug:
        if (!command.argument.empty() && !GSLogStream::SetDebugLoggers(command.argument)) {
            GSLogStream::Print("Unknown debug logger or invalid list. Use 'debug' to list loggers. Selection unchanged.");
        } else {
            GSLogStream::Print(GSLogStream::GetDebugStatus());
        }
        break;
    case AT::Test:
        PostACTrackCommand(GSDefinitions::CMD_SPAWN_TEST_AIRCRAFT);
        break;
    }
}

void GSAliveAirportApp::PostACTrackCommand(int cmdId)
{
    // Results arrive through the log sink; never wait for a worker on the UI thread.
    CmdPtr cmd = std::make_unique<GSCommand>(cmdId);
    m_aircraftTracker.PostCommand(cmd);
}
} // namespace NS_GSAliveAirport

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    const HRESULT RESULT = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(RESULT)) {
        MessageBoxW(nullptr, L"Unable to initialize COM", L"GSAliveAirport", MB_OK | MB_ICONERROR);
        return 1;
    }
    NS_GSAliveAirport::GSAliveAirportApp app;
    int rc = app.Run();
    CoUninitialize();
    return rc;
}
