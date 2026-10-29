// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAircraftTrackerThread.h"
#include "GSSpawnerThread.h"
#include "Animation/GSAnimationThread.h"
#include "SimObjects/GSMovementThread.h"
#include "UI/GSAliveUI.h"
#include "UI/resources.h"

#include <optional>
#include <string>
#include <string_view>

namespace NS_GSAliveAirport
{
class GSAliveAirportApp final
{
public:
    static constexpr const char* s_Version = APP_VERSION;
    GSAliveAirportApp():
        m_animThread(m_userAircraft),
        m_spawner(m_animThread, m_mvmntThread, m_ui, m_userAircraft),
        m_aircraftTracker(m_spawner, m_animThread, m_userAircraft) {}
    ~GSAliveAirportApp() { m_aircraftTracker.Stop(); m_spawner.Stop(); m_animThread.Stop(); m_mvmntThread.Stop(); }

    GSAliveAirportApp(const GSAliveAirportApp &) = delete;
    GSAliveAirportApp &operator=(const GSAliveAirportApp &) = delete;

    int Run();

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

    GSAircraft m_userAircraft;
    GSAliveUI m_ui;
    GSMovementThread m_mvmntThread;
    GSAnimationThread m_animThread;
    GSSpawnerThread m_spawner;
    GSAircraftTrackerThread m_aircraftTracker;
};
} // namespace NS_GSAliveAirport
