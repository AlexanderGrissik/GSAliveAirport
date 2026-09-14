#pragma once

#include "simobj/GSAircraft.h"
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

    ~GSAircraftTrackerThread() override {}
    GSAircraftTrackerThread(GSSimConnect& singleObserver): m_singleObserver(singleObserver) {}
    GSAircraftTrackerThread(const GSAircraftTrackerThread &) = delete;
    GSAircraftTrackerThread &operator=(const GSAircraftTrackerThread &) = delete;

    void Start();
    void Stop();
    
    void OnConnect() override;
    void OnDisconnect() override;
    void OnSimStart() override {}
    void OnSimStop() override;
    void OnCommand(GSCommand& cmd) override;

    void SetInProgress(bool v) { m_scanInProgress = v; }
    const GSAircraft* GetUserAircraft() const { return m_userAircraft.get(); }

  private:

    static void AircraftTrackerLoop(std::stop_token stopToken, GSAircraftTrackerThread *self);
    void RunLoopTracker(std::stop_token stopToken);

    void RequestScan();
    void HandleRemoved();
    bool HandleExistingAircraft(std::shared_ptr<GSAircraft> &aircraft, std::shared_ptr<GSAircraft> &existing);
    bool HandleNewAircraft(std::shared_ptr<GSAircraft>& aircraft);
    void HandleAircraftUser(std::shared_ptr<GSAircraft>& aircraft);
    bool HandleScanMessage(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE& entry);
    void PrintAircrafts(bool parkedOnly, double distKM) const;
    void SpawnTestAircrafts();

    std::unordered_map<DWORD, std::shared_ptr<GSAircraft>> m_tracked;
    std::unordered_set<DWORD> m_lastScanIDs;
    GSDefinitions::SendResult m_rc{};
    bool m_scanInProgress = false;
    GSSimConnect& m_singleObserver;
    std::chrono::steady_clock::time_point m_lastScanTime{};
    std::jthread m_thread;
    std::shared_ptr<GSAircraft> m_userAircraft;

    class GSReqScan : public GSRequest {
    public:
        using GSRequest::GSRequest;
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;
    };

    class GSReqSpawnAircraft : public GSRequest {
    public:
        GSReqSpawnAircraft(GSSimConnect& simHandle, int t) : GSRequest(simHandle), m_type(t) {}
        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;
    private:
        int m_type;
        SIMCONNECT_DATA_INITPOSITION m_pos{};
    };
};
} // namespace NS_GSLiveAirportMSFS
