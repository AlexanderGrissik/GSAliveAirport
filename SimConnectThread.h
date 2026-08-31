#pragma once

#include "Aircraft.h"
#include "GroundObject.h"
#include "SimConnectSession.h"
#include "SimObjectCatalog.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace parking_services
{
struct AircraftScanResult
{
    bool succeeded{};
    std::vector<AircraftSnapshot> aircraft;
};

struct GroundScanResult
{
    bool succeeded{};
    std::vector<GroundSnapshot> objects;
};

struct AnimationUpdate
{
    DWORD objectId{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double headingDegrees{};
    double animationFrame{};
};

struct AnimationProbeSample
{
    double elapsedSeconds{};
    DWORD objectId{};
    std::string title;
    double groundSpeedKnots{};
    double velocityBodyXMetersPerSecond{};
    double velocityBodyYMetersPerSecond{};
    double velocityBodyZMetersPerSecond{};
    double headingDegrees{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    bool onGround{};
};

class SimConnectThread final : public ISimConnectMessageSink
{
  public:
    using AircraftScanCallback = std::function<void(AircraftScanResult)>;
    using ObjectCreatedCallback = std::function<void(DWORD)>;
    using ObjectRemovedCallback = std::function<void(DWORD)>;
    using ConnectionCallback = std::function<void(bool)>;
    using ProbeSampleCallback = std::function<void(AnimationProbeSample)>;

    explicit SimConnectThread(LogSink log);
    ~SimConnectThread();

    SimConnectThread(const SimConnectThread &) = delete;
    SimConnectThread &operator=(const SimConnectThread &) = delete;
    void Stop();

    void RequestAircraftScan(AircraftScanCallback callback);
    std::future<GroundScanResult> RequestGroundObjects();
    void CreateObject(std::string title, SIMCONNECT_DATA_INITPOSITION position,
                      ObjectCreatedCallback callback);
    void RemoveObject(DWORD objectId);
    void FreezeObject(DWORD objectId);
    void PublishAnimationUpdates(const std::vector<AnimationUpdate> &updates);
    void CancelAnimationObject(DWORD objectId);
    void StartAnimationProbe(DWORD objectId, ProbeSampleCallback callback);
    void StopAnimationProbe();
    void RequestCatalog();
    bool FillAvailableSimObjectTitles(std::vector<std::string> &destination) const;

    void SubscribeObjectRemoved(ObjectRemovedCallback callback);
    void SubscribeConnection(ConnectionCallback callback);
    [[nodiscard]] bool IsConnected() const;

    void OnSimConnectMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;

  private:
    struct PendingAircraftScan;
    struct PendingGroundScan;
    struct PendingCreate;
    struct PendingProbe;

    static void SimConnectLoop(std::stop_token stopToken, SimConnectThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void ProcessAnimationUpdates();
    void MaintainPendingRequests();
    bool Connect();
    void Disconnect();
    bool DefineDataAndEvents();

    void BeginAircraftScan(AircraftScanCallback callback);
    void CompleteAircraftScan(bool succeeded);
    void BeginGroundScan(std::shared_ptr<std::promise<GroundScanResult>> promise);
    void CompleteGroundScan(bool succeeded);
    void BeginCreate(std::string title, SIMCONNECT_DATA_INITPOSITION position,
                     ObjectCreatedCallback callback);
    void CompleteCreate(DWORD requestId, DWORD objectId);
    void BeginProbe(DWORD objectId, ProbeSampleCallback callback);
    void EndProbe();
    void HandleObjectData(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry, DWORD messageSize);
    void HandleProbeData(const SIMCONNECT_RECV_SIMOBJECT_DATA &entry, DWORD messageSize);
    void HandleException(const SIMCONNECT_RECV_EXCEPTION &exception);
    void NotifyObjectRemoved(DWORD objectId);
    void NotifyConnection(bool connected);
    void PublishAvailableSimObjectTitles(std::vector<std::string> titles);

    LogSink m_log;
    SimConnectSession m_session;
    SimObjectCatalog m_catalog;
    std::atomic_bool m_connected{false};
    bool m_reportedWaiting = false;
    bool m_disconnectRequested = false;

    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::mutex m_animationMutex;
    std::map<DWORD, AnimationUpdate> m_latestAnimationUpdates;
    mutable std::mutex m_catalogSnapshotMutex;
    std::vector<std::string> m_availableSimObjectTitles;
    bool m_catalogSnapshotReady{};
    std::mutex m_subscriberMutex;
    std::vector<ObjectRemovedCallback> m_objectRemovedCallbacks;
    std::vector<ConnectionCallback> m_connectionCallbacks;

    std::unique_ptr<PendingAircraftScan> m_aircraftScan;
    std::unique_ptr<PendingGroundScan> m_groundScan;
    std::map<DWORD, PendingCreate> m_creates;
    std::unique_ptr<PendingProbe> m_probe;
    std::jthread m_thread;
};
} // namespace parking_services
