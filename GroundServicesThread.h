#pragma once

#include "Aircraft.h"
#include "AnimationThread.h"
#include "GroundServicesConfig.h"
#include "GSObject.h"
#include "GSRequests/GSReqBaggageGeometry.h"
#include "GSRequests/GSReqCommand.h"
#include "GSRequests/GSReqCreateObject.h"
#include "ISimConnectHandler.h"
#include "ISimConnectStatus.h"
#include "LogSink.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace parking_services
{
class AircraftTrackerThread;

enum class GroundServicesDecision { Keep, Add, Remove };

struct GroundServicesStatus
{
    std::size_t createdObjects{};
    std::size_t pendingCreates{};
    std::size_t servicedAircraft{};
};

// Drives ground-service objects on its own thread. It owns the object
// collection as polymorphic GSObject instances (created through
// GSObject::Create, keyed by token) and implements the GSObjectServices
// capabilities that those objects call into. Type-specific behaviours
// (cargo-door ramp alignment, route walking) live in the GSObject subclasses,
// not here; this thread only sequences their lifecycle: create, maintain,
// geometry, finalize, and remove.
class GroundServicesThread final : public ISimConnectStatus
{
  public:
    GroundServicesThread(ISimConnectHandler &simConnect,
                         AircraftTrackerThread &aircraftTracker,
                         AnimationThread &animation,
                         GroundServicesConfig configuration, LogSink log);
    ~GroundServicesThread();

    GroundServicesThread(const GroundServicesThread &) = delete;
    GroundServicesThread &operator=(const GroundServicesThread &) = delete;

    void Start();
    void Stop();
    void Reset();
    [[nodiscard]] GroundServicesStatus Status() const;

    void OnSimConnected() override;
    void OnSimDisconnected() override;
    void OnSimStarted() override;
    void OnSimStopped() override;
    void OnObjRemoved(std::uint32_t objectId) override;

  private:
    static void GroundServicesLoop(std::stop_token stopToken, GroundServicesThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void InitializeSimConnect();
    void PollRequests();
    GSReqCommand &NewCommandRequest();
    void CollectFinishedCommandRequests();
    void MaintainObjects(std::chrono::steady_clock::time_point now);
    void EvaluateTrackedAircraft();
    static GroundServicesDecision Decide(const AircraftSnapshot &aircraft);
    void EnsureAutomaticServices(const AircraftSnapshot &aircraft);
    void QueueService(const AircraftSnapshot &aircraft, const GroundServiceRequest &request);
    void BeginCreate(std::uint64_t token, std::unique_ptr<GSObject> object);
    void RegisterObject(GSObject *object, AircraftId objectId,
                        const GSObject::GSObjectPos &pose);
    void FinalizeObject(GSObject *object, const GSObject::GSObjectPos &actualPose);
    void QueueAttachments(const AircraftSnapshot &aircraft, AircraftId parentObjectId,
                          const GSObject::GSObjectPos &parentPose,
                          const std::vector<GroundServiceObject> &attachments);
    void RemoveForAircraft(AircraftId aircraftId);
    void RemoveResolvedObjects(AircraftId aircraftId, bool requestSimulatorRemoval);
    void FinalizeDeferredRemoval(AircraftId aircraftId);
    void DecrementPendingCreations(AircraftId aircraftId);
    [[nodiscard]] std::size_t PendingCount(AircraftId aircraftId) const;
    void RemoveObject(AircraftId objectId, bool requestSimulatorRemoval);
    void HandleObjectRemoved(AircraftId objectId);
    void HandleConnect();
    void HandleDisconnect();
    void RemoveAllServicesInternal();
    void PublishStatus();
    [[nodiscard]] bool HasPendingFor(AircraftId aircraftId) const;
    GSObjectServices MakeServices();

    struct CreateOperation
    {
        AircraftId aircraftId{};
        std::unique_ptr<GSReqCreateObject> request;
    };

    ISimConnectHandler &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    AnimationThread &m_animation;
    GroundServicesConfig m_configuration;
    LogSink m_log;
    bool m_connected = false;
    std::uint64_t m_nextCreateToken = 1;
    // Capability bag injected into every GSObject. Built once in the constructor;
    // its lambdas capture 'this' and are stateless, so a single shared instance is
    // safe (every GSObject method runs on this thread).
    GSObjectServices m_services;
    // Every ground-service object under management, keyed by create token. The
    // concrete subclass (GSWalker, GSLuggageLoaderFSDT, ...) is created by
    // GSObject::Create and drives its own special-type behaviour through the
    // GSObjectServices capabilities this thread injects.
    std::map<std::uint64_t, std::unique_ptr<GSObject>> m_objects;
    std::map<std::uint64_t, CreateOperation> m_createRequests;
    std::map<AircraftId, std::unique_ptr<GSReqBaggageGeometry>>
        m_geometryRequests;
    std::vector<std::unique_ptr<GSReqCommand>> m_commandRequests;
    std::set<AircraftId> m_createdObjects;
    std::set<AircraftId> m_configuredAircraft;
    std::map<AircraftId, std::set<AircraftId>> m_objectsByAircraft;
    std::unordered_map<AircraftId, AircraftId> m_aircraftByObject;
    std::unordered_map<AircraftId, AircraftId> m_parentByObject;
    std::map<AircraftId, std::set<AircraftId>> m_childrenByObject;
    // In-flight create count per aircraft. Drives deferred deletion: an
    // aircraft's services are removed only once this drains to zero.
    std::map<AircraftId, std::size_t> m_pendingCreations;
    // Aircraft whose removal was requested while creations were still in flight;
    // finalized by FinalizeDeferredRemoval once its in-flight count drains to zero.
    std::set<AircraftId> m_deferredRemoval;
    std::vector<AircraftSnapshot> m_aircraftSnapshotBuffer;
    std::vector<GroundServiceRequest> m_serviceRequestBuffer;
    std::mt19937 m_random{std::random_device{}()};

    mutable std::mutex m_statusMutex;
    GroundServicesStatus m_status;
    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
