// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAircraftTrackerThread.h"
#include "GSConsole.h"
#include "GSSpawnerThread.h"
#include "../Animation/GSAnimationThread.h"
#include "../SimObjects/GSMovementThread.h"
#include <atomic>

namespace NS_GSAliveAirport
{
class GSAliveAirportApp final
{
public:
    static constexpr const char* s_Version = "0.4.0";
    GSAliveAirportApp(): m_aircraftTracker(m_spawner), m_spawner(m_animThread, m_mvmntThread) {}
    ~GSAliveAirportApp() { m_aircraftTracker.Stop(); m_spawner.Stop(); }

    GSAliveAirportApp(const GSAliveAirportApp &) = delete;
    GSAliveAirportApp &operator=(const GSAliveAirportApp &) = delete;

    int Run();

    void PostACTrackCommandAndWait(int cmdId);

private:

    void HandleCommand(GSConsole::AppCommand command);

    std::atomic_bool m_quit{false};
    GSAircraftTrackerThread m_aircraftTracker;
    GSAnimationThread m_animThread;
    GSSpawnerThread m_spawner;
    GSMovementThread m_mvmntThread;
    GSConsole m_console;
};
} // namespace NS_GSAliveAirport
