#pragma once

#include "GSAircraftTrackerThread.h"
#include "GSConsole.h"
#include "GSSpawnerThread.h"
#include "../Animation/GSAnimationThread.h"
#include "../SimObjects/GSMovementThread.h"
#include <atomic>

namespace NS_GSLiveAirportMSFS
{
class GSLiveAirportMSFSApp final
{
public:
    GSLiveAirportMSFSApp(): m_aircraftTracker(m_spawner), m_spawner(m_animThread, m_mvmntThread) {}
    ~GSLiveAirportMSFSApp() { m_aircraftTracker.Stop(); m_spawner.Stop(); }

    GSLiveAirportMSFSApp(const GSLiveAirportMSFSApp &) = delete;
    GSLiveAirportMSFSApp &operator=(const GSLiveAirportMSFSApp &) = delete;

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
} // namespace NS_GSLiveAirportMSFS
