#pragma once

#include "GSAircraftTrackerThread.h"
#include "GSConsole.h"
#include "GSSimConnect.h"

#include <atomic>

namespace NS_GSLiveAirportMSFS
{
class GSLiveAirportMSFSApp final
{
public:
    GSLiveAirportMSFSApp() = default;
    ~GSLiveAirportMSFSApp() { m_aircraftTracker.Stop(); }

    GSLiveAirportMSFSApp(const GSLiveAirportMSFSApp &) = delete;
    GSLiveAirportMSFSApp &operator=(const GSLiveAirportMSFSApp &) = delete;

    int Run();

private:

    void HandleCommand(GSConsole::AppCommand command);

    std::atomic_bool m_quit{false};
    GSAircraftTrackerThread m_aircraftTracker;
    GSConsole m_console;
};
} // namespace NS_GSLiveAirportMSFS