#pragma once

#include "GSAircraft.h"
#include "GSSimConnect.h"

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
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace NS_GSLiveAirportMSFS
{

class GSAircraftTrackerThread final : public GSSimConnect
{
public:
    static constexpr double ActiveRadiusMeters = 1'000.0;
    static constexpr DWORD DiscoveryRadiusMeters = 5000;

    enum {
        CMD_PRINT_AIRCRAFT_ALL = 0,
        CMD_PRINT_AIRCRAFT_1KM,
        CMD_PRINT_AIRCRAFT_PARKED
    };;

    /*struct IAircraftTrack
    {
        virtual void OnAircraftAdded(std::shared_ptr<GSAircraft>& aircraft) = 0;
        virtual void OnAircraftModified(std::shared_ptr<GSAircraft>& aircraft) = 0;
        virtual void OnAircraftRemoved(std::shared_ptr<GSAircraft>& aircraft) = 0;
        virtual void OnAircraftUser(std::shared_ptr<GSAircraft>& aircraft) = 0;
    };*/

    ~GSAircraftTrackerThread() override {}
    GSAircraftTrackerThread() = default;
    GSAircraftTrackerThread(const GSAircraftTrackerThread &) = delete;
    GSAircraftTrackerThread &operator=(const GSAircraftTrackerThread &) = delete;

    void Start();
    void Stop();
    //void Subscribe(IAircraftTrack* tracker) { m_subscribers.emplace_back(tracker); }

    void OnConnect() override;
    void OnDisconnect() override;
    void OnSimStart() override;
    void OnSimStop() override;
    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;
    bool OnCommand(GSCommand& cmd) override;

  private:

    static void AircraftTrackerLoop(std::stop_token stopToken, GSAircraftTrackerThread *self);
    void RunLoopTracker(std::stop_token stopToken);

    bool RequestScan();
    void HandleRemoved();
    void HandleExistingAircraft(std::shared_ptr<GSAircraft> &aircraft, std::shared_ptr<GSAircraft> &existing);
    void HandleNewAircraft(std::shared_ptr<GSAircraft> &aircraft);
    void PrintAircrafts(bool parkedOnly, double distKM) const;

    std::unordered_map<DWORD, std::shared_ptr<GSAircraft>> m_tracked;
    std::unordered_set<DWORD> m_lastScanIDs;
    SendResult m_rc;
    bool m_simStarted = false;
    bool m_scanInProgress = false;
    bool m_lastLoopMsg = false;

    std::jthread m_thread;
};
} // namespace NS_GSLiveAirportMSFS
