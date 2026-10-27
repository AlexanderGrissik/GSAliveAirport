// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAliveAirportApp.h"
#include "GSLogStream.h"
#include "../UI/GSAliveUIAPI.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <utility>
#include <vector>

namespace NS_GSAliveAirport
{
int GSAliveAirportApp::Run()
{
    GSLogStream::SetSink([this](std::string_view text) { m_ui.AppendLog(text); });

    if (!m_ui.Initialize([this](std::string_view commandLine) {
            if (const auto cmd = ParseCommand(commandLine)) { HandleCommand(*cmd); }
        })) {
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

    //SendSampleAircraft();

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    m_ui.Shutdown();
    return static_cast<int>(message.wParam);
}

std::optional<GSAliveAirportApp::AppCommand> GSAliveAirportApp::ParseCommand(std::string_view line)
{
    std::istringstream input{std::string(line)};
    std::string command;
    input >> command;
    command = GSLogStream::Lower(std::move(command));
    if (command.empty()) return std::nullopt;
    if (command == "log") return AppCommand{AppCommandType::Log, {}};
    if (command == "debug") {
        AppCommand debug{AppCommandType::Debug, {}};
        std::getline(input >> std::ws, debug.argument);
        return debug;
    }
    if (command == "test") return AppCommand{AppCommandType::Test, {}};
    if (command == "injector") {
        AppCommand injector{AppCommandType::Injector, {}};
        std::getline(input >> std::ws, injector.argument);
        return injector;
    }
    return std::nullopt;
}

void GSAliveAirportApp::HandleCommand(const AppCommand &command)
{
    using AT = AppCommandType;
    switch (command.type) {
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
    case AT::Injector:
    {
        std::istringstream input(command.argument);
        std::string injectorValue;
        unsigned int groundServicedPercent = 50;
        input >> injectorValue >> groundServicedPercent;
        if (groundServicedPercent > 100) groundServicedPercent = 100;

        if (injectorValue == "1") {
            m_spawner.SetInjector(GSSpawnerThread::Injector::MSFS24Default, static_cast<std::uint8_t>(groundServicedPercent));
        } else if (injectorValue == "99") {
            m_spawner.SetInjector(GSSpawnerThread::Injector::Any, static_cast<std::uint8_t>(groundServicedPercent));
        } else {
            m_spawner.SetInjector(GSSpawnerThread::Injector::SayIntentions, static_cast<std::uint8_t>(groundServicedPercent));
        }
        break;
    }
    }
}

void GSAliveAirportApp::PostACTrackCommand(int cmdId)
{
    // Results arrive through the log sink; never wait for a worker on the UI thread.
    CmdPtr cmd = std::make_unique<GSCommand>(cmdId);
    m_aircraftTracker.PostCommand(cmd);
}

void GSAliveAirportApp::SendSampleAircraft()
{
    // Sample aircraft data to exercise the Aircrafts page until live tracking is wired.
    auto fill = [](auto &dst, const char *value) { std::snprintf(dst.data(), dst.size(), "%s", value); };

    // Build a self-contained SIMOBJECT_DATA entry whose payload starts at dwData.
    // GSSimConnect::ReadMsgData always copies from the start of dwData, so each
    // loader is given a payload sliced to exactly the range it reads.
    auto withEntry = [](DWORD id, const void *payload, std::size_t payloadSize, auto &&load) {
        std::vector<std::uint8_t> buffer(sizeof(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE) + payloadSize, 0);
        auto *entry = reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(buffer.data());
        const std::size_t offset = reinterpret_cast<const std::uint8_t *>(&entry->dwData) - reinterpret_cast<const std::uint8_t *>(entry);
        entry->dwSize = static_cast<DWORD>(offset + payloadSize);
        entry->dwObjectID = id;
        std::memcpy(&entry->dwData, payload, payloadSize);
        load(*entry);
    };

    auto makeAircraft = [&](std::uint32_t id, GSAircraft::AircraftWireData dyn, GSAircraft::AircraftWireDataConst cons) {
        GSAircraft ac;
        withEntry(id, &dyn, GSAircraft::AIRCRAFT_WIREDATA_INITSIZE, [&ac](const auto &e) { ac.LoadInitState(e); });
        withEntry(id, &dyn.altitudeFeet, GSAircraft::AIRCRAFT_WIREDATA_DYNSIZE, [&ac](const auto &e) { ac.LoadDynamicState(e); });
        withEntry(id, &cons, sizeof(cons), [&ac](const auto &e) { ac.LoadFullState(e); });
        return ac;
    };

    GSAircraft::AircraftWireData d1{};
    fill(d1.title, "Airbus A320-214");
    d1.latitude = 48.8571;
    d1.longitude = 2.3413;
    d1.altitudeFeet = 12.4f;
    d1.groundAltitudeFeet = 41.8f;
    d1.headingDegrees = 271.2f;
    fill(d1.currentAirport, "LFPG");
    fill(d1.assignedParking, "A12");
    fill(d1.trafficState, "PARKED");
    d1.lightBeacon = 1;
    d1.lightNav = 1;
    d1.parkingBrake = 1;
    d1.onGround = 1;

    GSAircraft::AircraftWireDataConst c1{};
    fill(c1.atcId, "EI320");
    fill(c1.atcAirline, "Aer Lingus");
    fill(c1.atcFlightNumber, "EI320");
    c1.wingSpanMeters = 35.8;
    c1.numberOfEngines = 2;
    c1.interactivePoints[0] = GSAircraft::InteractivePointWireData{ 0, 12.1f, 3.0f, -1.4f, 90.0f };
    c1.interactivePoints[1] = GSAircraft::InteractivePointWireData{ 1, 14.2f, 3.1f, -0.8f, 90.0f };
    m_ui.PostMessageUI(GSAliveUIAPI::AddAircraft(makeAircraft(237, d1, c1)));
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(237, GSAliveUIAPI::AircraftService::Inplace));

    GSAircraft::AircraftWireData d2{};
    fill(d2.title, "Boeing 737-8AS");
    d2.latitude = 48.8645;
    d2.longitude = 2.3275;
    d2.altitudeFeet = 28.1f;
    d2.groundAltitudeFeet = 26.9f;
    d2.headingDegrees = 14.5f;
    d2.groundSpeedKnots = 18.5f;
    fill(d2.fromAirport, "EGLL");
    fill(d2.toAirport, "LFPG");
    fill(d2.assignedParking, "B4");
    fill(d2.trafficState, "STATE_SIMPLE_TAXI");
    d2.lightBeacon = 1;
    d2.lightNav = 1;
    d2.lightTaxi = 1;
    d2.onGround = 1;
    d2.pushbackAttached = 1;

    GSAircraft::AircraftWireDataConst c2{};
    fill(c2.atcId, "EI874");
    fill(c2.atcAirline, "Ryanair");
    fill(c2.atcFlightNumber, "FRE874");
    c2.wingSpanMeters = 35.8;
    c2.numberOfEngines = 2;
    c2.pushbackContactXMeters = 21.3f;
    c2.pushbackContactZMeters = -0.4f;
    c2.interactivePoints[0] = GSAircraft::InteractivePointWireData{ 0, 11.8f, 3.0f, -1.2f, 12.0f };
    m_ui.PostMessageUI(GSAliveUIAPI::AddAircraft(makeAircraft(1042, d2, c2)));
    m_ui.PostMessageUI(GSAliveUIAPI::UpdateAircraftServices(1042, GSAliveUIAPI::AircraftService::Spawning));

    GSAircraft::AircraftWireData d3{};
    fill(d3.title, "Airbus A321-253NX");
    d3.latitude = 48.8702;
    d3.longitude = 2.3589;
    d3.altitudeFeet = 1520.0f;
    d3.groundAltitudeFeet = 1479.2f;
    d3.headingDegrees = 245.0f;
    d3.groundSpeedKnots = 142.0f;
    fill(d3.toAirport, "LFPG");
    fill(d3.trafficState, "ARRIVING");
    d3.etaSeconds = 120;
    d3.lightBeacon = 1;
    d3.lightNav = 1;

    GSAircraft::AircraftWireDataConst c3{};
    fill(c3.atcId, "AF1452");
    fill(c3.atcAirline, "Air France");
    fill(c3.atcFlightNumber, "AF1452");
    c3.wingSpanMeters = 35.8;
    c3.numberOfEngines = 2;
    m_ui.PostMessageUI(GSAliveUIAPI::AddAircraft(makeAircraft(4096, d3, c3)));
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
