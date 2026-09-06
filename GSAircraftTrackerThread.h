#pragma once

#include "Aircraft.h"
#include "GSRequests/GSReqAircraftScan.h"
#include "GSRequests/GSReqCommand.h"
#include "SimConnectThread.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

namespace parking_services
{

class GSAircraftTrackerThread final : public SimConnectThread
{
public:
    static constexpr double ActiveRadiusMeters = 1'000.0;
    static constexpr double DiscoveryRadiusMeters = 5'000.0;

    struct IAircraftTrack
    {
        virtual void OnAircraftAdded(std::shared_ptr<GSAircraft>& aircraft) = 0;
        virtual void OnAircraftModified(std::shared_ptr<GSAircraft>& aircraft) = 0;
        virtual void OnAircraftRemoved(std::shared_ptr<GSAircraft>& aircraft) = 0;
        virtual void OnAircraftUser(std::shared_ptr<GSAircraft>& aircraft) = 0;
    }

    virtual ~AircraftTrackerThread();

    AircraftTrackerThread(const AircraftTrackerThread &) = delete;
    AircraftTrackerThread &operator=(const AircraftTrackerThread &) = delete;

    void Start();
    void Stop();
    void Subscribe(IAircraftTrack* tracker) { m_subscribers.emplace_back(tracker); }

    virtual void OnConnect() override;
    virtual void OnDisconnect() override;
    virtual void OnSimStart() override;
    virtual void OnSimStop() override;
    virtual void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    virtual void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;

  private:

    static void AircraftTrackerLoop(std::stop_token stopToken, AircraftTrackerThread *self);
    void RunLoopTracker(std::stop_token stopToken);

    bool RequestScan();
    void HandleRemoved();
    void HandleExistingAircraft(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry);
    void HandleNewAircraft(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry);

    std::unordered_map<DWORD, std::shared_ptr<GSAircraft>> m_tracked;
    std::vector<IAircraftTrack*> m_subscribers;
    std::unordered_set<DWORD> m_lastScanIDs;
    SendResult m_rc;
    bool m_simStarted = false;
    bool m_scanInProgress = false;
    bool m_lastLoopMsg = false;

    std::jthread m_thread;
};
} // namespace parking_services
