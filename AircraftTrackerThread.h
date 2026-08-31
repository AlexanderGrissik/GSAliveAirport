#pragma once

#include "Aircraft.h"
#include "LogSink.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace parking_services
{
class SimConnectThread;

class AircraftTrackerThread final
{
  public:
    static constexpr double DiscoveryRadiusMeters = 1'000.0;
    static constexpr double RetentionRadiusMeters = 5'000.0;

    AircraftTrackerThread(SimConnectThread &simConnect, LogSink log);
    ~AircraftTrackerThread();

    AircraftTrackerThread(const AircraftTrackerThread &) = delete;
    AircraftTrackerThread &operator=(const AircraftTrackerThread &) = delete;

    void Stop();
    void Reset();

    void FillTrackedAircraftSnapshot(std::vector<AircraftSnapshot> &destination) const;
    void FillNearbyAircraftSnapshot(
        std::vector<AircraftSnapshot> &destination,
        double radiusMeters = RetentionRadiusMeters) const;
    [[nodiscard]] std::size_t TrackedCount() const;
    [[nodiscard]] std::size_t NearbyCount() const;

  private:
    static void AircraftTrackerLoop(std::stop_token stopToken,
                                    AircraftTrackerThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void RequestScan();
    void ApplyScan(std::vector<AircraftSnapshot> observations);
    void RemoveObject(AircraftId objectId);
    void ClearState();
    void PublishSnapshot();

    SimConnectThread &m_simConnect;
    LogSink m_log;
    bool m_connected = false;
    bool m_scanPending = false;
    std::chrono::steady_clock::time_point m_nextScan{};
    std::map<AircraftId, AircraftSnapshot> m_tracked;
    std::map<AircraftId, AircraftSnapshot> m_nearby;

    mutable std::mutex m_snapshotMutex;
    std::map<AircraftId, AircraftSnapshot> m_publishedTracked;
    std::map<AircraftId, AircraftSnapshot> m_publishedNearby;

    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
