// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAircraftTrackerThread.h"
#include "GSSpawnerThread.h"
#include "../Animation/GSAnimationThread.h"
#include "../SimObjects/GSMovementThread.h"
#include "../UI/GSAliveUI.h"
#include "../UI/resources.h"

#include <optional>
#include <string>
#include <string_view>

namespace NS_GSAliveAirport
{
class GSAliveAirportApp final
{
public:
    static constexpr const char* s_Version = APP_VERSION;
    GSAliveAirportApp(): m_aircraftTracker(m_spawner), m_spawner(m_animThread, m_mvmntThread, m_ui) {}
    ~GSAliveAirportApp() { m_aircraftTracker.Stop(); m_spawner.Stop(); }

    GSAliveAirportApp(const GSAliveAirportApp &) = delete;
    GSAliveAirportApp &operator=(const GSAliveAirportApp &) = delete;

    int Run();

    void PostACTrackCommand(int cmdId);

private:

    enum class AppCommandType
    {
        Log, Debug, Test, Injector,
    };

    struct AppCommand
    {
        AppCommandType type;
        std::string argument;
    };

    void HandleCommand(const AppCommand &command);
    static std::optional<AppCommand> ParseCommand(std::string_view line);
    void SendSampleAircraft();

    GSAircraftTrackerThread m_aircraftTracker;
    GSAnimationThread m_animThread;
    GSSpawnerThread m_spawner;
    GSMovementThread m_mvmntThread;
    GSAliveUI m_ui;
};
} // namespace NS_GSAliveAirport
