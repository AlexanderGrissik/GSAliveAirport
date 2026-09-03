#pragma once

#include "Aircraft.h"
#include "ISimConnectHandler.h"
#include "ISimConnectStatus.h"
#include "LogSink.h"
#include "GSRequests/GSReqAircraftScan.h"
#include "GSRequests/GSReqCommand.h"

#include <condition_variable>
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
struct ApproximateUserPosition
{
    AircraftId objectId{};
    double latitude{};
    double longitude{};
};

class AircraftTrackerThread final : public ISimConnectStatus
{
  public:
    static constexpr double DiscoveryRadiusMeters = 1'000.0;
    static constexpr double RetentionRadiusMeters = 5'000.0;

    AircraftTrackerThread(ISimConnectHandler &simConnect, LogSink log);
    ~AircraftTrackerThread();

    AircraftTrackerThread(const AircraftTrackerThread &) = delete;
    AircraftTrackerThread &operator=(const AircraftTrackerThread &) = delete;

    void Start();
    void Stop();
    void Reset();

    void OnSimConnected() override;
    void OnSimDisconnected() override;
    void OnSimStarted() override;
    void OnSimStopped() override;
    void OnObjRemoved(std::uint32_t objectId) override;

    void FillTrackedAircraftSnapshot(std::vector<AircraftSnapshot> &destination) const;
    void FillNearbyAircraftSnapshot(
        std::vector<AircraftSnapshot> &destination,
        double radiusMeters = RetentionRadiusMeters) const;
    [[nodiscard]] bool TryGetApproximateUserPosition(
        ApproximateUserPosition &destination) const;
    [[nodiscard]] std::size_t TrackedCount() const;
    [[nodiscard]] std::size_t NearbyCount() const;

  private:
    static void AircraftTrackerLoop(std::stop_token stopToken,
                                    AircraftTrackerThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void InitializeSimConnect();
    GSReqCommand &NewSetupRequest();
    void CollectFinishedSetupRequests();
    void PollScan();
    void RequestScan();
    void ApplyScan(std::vector<AircraftSnapshot> observations);
    void RemoveObject(AircraftId objectId);
    void ClearState();
    void PublishSnapshot();

    ISimConnectHandler &m_simConnect;
    LogSink m_log;
    bool m_connected = false;
    std::unique_ptr<GSReqAircraftScan> m_scanRequest;
    std::vector<std::unique_ptr<GSReqCommand>> m_setupRequests;
    bool m_discardScanResult{};
    std::chrono::steady_clock::time_point m_nextScan{};
    std::map<AircraftId, AircraftSnapshot> m_tracked;
    std::map<AircraftId, AircraftSnapshot> m_nearby;
    std::optional<ApproximateUserPosition> m_userPosition;

    mutable std::mutex m_snapshotMutex;
    std::map<AircraftId, AircraftSnapshot> m_publishedTracked;
    std::map<AircraftId, AircraftSnapshot> m_publishedNearby;
    std::optional<ApproximateUserPosition> m_publishedUserPosition;

    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
