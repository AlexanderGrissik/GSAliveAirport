#pragma once

#include "Aircraft.h"
#include "LogSink.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace parking_services
{
class AircraftTrackerThread;
class AnimationThread;
class SimConnectThread;

enum class GroundServicesDecision { Keep, Add, Remove };

struct GroundServicesStatus
{
    std::size_t createdObjects{};
    std::size_t pendingCreates{};
    std::size_t servicedAircraft{};
};

class GroundServicesThread final
{
  public:
    GroundServicesThread(SimConnectThread &simConnect,
                         AircraftTrackerThread &aircraftTracker,
                         AnimationThread &animation, LogSink log);
    ~GroundServicesThread();

    GroundServicesThread(const GroundServicesThread &) = delete;
    GroundServicesThread &operator=(const GroundServicesThread &) = delete;

    void Stop();
    void SpawnFullTest();
    void ClearCreated(bool announce = true);
    void Reset();
    [[nodiscard]] GroundServicesStatus Status() const;

  private:
    struct PendingCreate
    {
        AircraftSnapshot aircraft;
        std::string title;
        bool cancelled{};
    };

    static void GroundServicesLoop(std::stop_token stopToken,
                                   GroundServicesThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void EvaluateTrackedAircraft();
    static GroundServicesDecision Decide(const AircraftSnapshot &aircraft);
    void EnsureAutomaticServices(const AircraftSnapshot &aircraft);
    void RemoveForAircraft(AircraftId aircraftId);
    void RequestObject(const AircraftSnapshot &aircraft, std::string title,
                       double forwardMeters, double rightMeters);
    void CompleteCreate(std::uint64_t token, AircraftId objectId);
    void HandleObjectRemoved(AircraftId objectId);
    void HandleConnection(bool connected);
    void SpawnFullTestInternal();
    void ClearCreatedInternal(bool announce);
    void PublishStatus();
    [[nodiscard]] bool HasPendingFor(AircraftId aircraftId) const;

    SimConnectThread &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    AnimationThread &m_animation;
    LogSink m_log;
    bool m_connected = false;
    std::atomic_bool m_stopping{false};
    std::uint64_t m_nextCreateToken = 1;
    std::map<std::uint64_t, PendingCreate> m_pendingCreates;
    std::set<AircraftId> m_createdObjects;
    std::map<AircraftId, std::set<AircraftId>> m_objectsByAircraft;
    std::unordered_map<AircraftId, AircraftId> m_aircraftByObject;
    std::vector<AircraftSnapshot> m_aircraftSnapshotBuffer;

    mutable std::mutex m_statusMutex;
    GroundServicesStatus m_status;
    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
