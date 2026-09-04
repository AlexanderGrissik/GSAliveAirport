#pragma once

#include "Aircraft.h"
#include "AnimationThread.h"
#include "GroundServicesConfig.h"
#include "GSObject.h"
#include "GSRequests/GSReqCommand.h"
#include "GSRequests/GSReqCreateObject.h"
#include "GSRequests/GSReqObjectPose.h"
#include "ISimConnectHandler.h"
#include "ISimConnectStatus.h"

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

// Drives polymorphic GSObject roots on its own thread. Each root owns its
// runtime attachment tree; concrete behavior stays in the GSObject subclasses.
// This thread only sequences their generic lifecycle and provides services.
class GroundServicesThread final : public ISimConnectStatus
{
  public:
    GroundServicesThread(ISimConnectHandler &simConnect,
                         AircraftTrackerThread &aircraftTracker,
                         AnimationThread &animation,
                         GroundServicesConfig configuration);
    ~GroundServicesThread();

    GroundServicesThread(const GroundServicesThread &) = delete;
    GroundServicesThread &operator=(const GroundServicesThread &) = delete;

    void Start();
    void Stop();
    void Reset();
    void RepositionStaticObject(AircraftId objectId, double x, double y,
                                double z, double headingDegrees);
    void FindClosestRootObject(std::string family);
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
    void CollectRetiredObjects();
    void PollFindOperation();
    void MaintainObjects(std::chrono::steady_clock::time_point now);
    void EvaluateTrackedAircraft();
    static GroundServicesDecision Decide(const AircraftSnapshot &aircraft);
    void EnsureAutomaticServices(const AircraftSnapshot &aircraft);
    void QueueService(const AircraftSnapshot &aircraft, const GroundServiceRequest &request);
    void BeginCreate(std::uint64_t token, GSObject &object);
    void RegisterObject(GSObject *object, AircraftId objectId,
                        const GSObject::GSObjectPos &pose);
    void FinalizeObject(GSObject *object, const GSObject::GSObjectPos &actualPose);
    void QueueAttachments(GSObject &parent, const GSObject::GSObjectPos &parentPose,
                          const std::vector<GroundServiceObject> &attachments);
    void RequestRemovalOfAllServices();
    void RemoveForAircraft(AircraftId aircraftId);
    void RemoveResolvedObjects(AircraftId aircraftId);
    void FinalizeDeferredRemoval(AircraftId aircraftId);
    void TryFinalizeDeferredRemovals();
    [[nodiscard]] bool AircraftWorkComplete(AircraftId aircraftId) const;
    void DecrementPendingCreations(AircraftId aircraftId);
    [[nodiscard]] std::size_t PendingCount(AircraftId aircraftId) const;
    void RemoveObject(AircraftId objectId, bool requestSimulatorRemoval);
    void RemoveObjectState(GSObject &object, bool requestSimulatorRemoval);
    void HandleObjectRemoved(AircraftId objectId);
    void HandleRepositionStaticObject(AircraftId objectId, double x, double y,
                                      double z, double headingDegrees);
    void HandleFindClosestRootObject(const std::string &family);
    void HandleConnect();
    void HandleDisconnect();
    void RemoveAllServicesInternal();
    void PublishStatus();
    [[nodiscard]] bool HasPendingFor(AircraftId aircraftId) const;
    GSObjectServices MakeServices();

    struct CreateOperation
    {
        AircraftId aircraftId{};
        // Non-owning: root/child ownership stays in the GSObject tree. Forced
        // teardown clears this pointer before releasing that tree.
        GSObject *object{};
        std::unique_ptr<GSReqCreateObject> request;
    };

    struct FindRow
    {
        AircraftId objectId{};
        std::string family;
        double relativeX{};
        double relativeY{};
        double relativeZ{};
        double relativeHeading{};
        double storedWorldHeading{};
        std::size_t depth{};
        bool root{};
        std::unique_ptr<GSReqObjectPose> request;
    };

    struct FindOperation
    {
        std::string family;
        double distanceMeters{};
        std::vector<FindRow> rows;
    };

    ISimConnectHandler &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    AnimationThread &m_animation;
    GroundServicesConfig m_configuration;
    bool m_connected = false;
    std::uint64_t m_nextCreateToken = 1;
    // Capability bag injected into every GSObject. Built once in the constructor;
    // its lambdas capture 'this' and are stateless, so a single shared instance is
    // safe (every GSObject method runs on this thread).
    GSObjectServices m_services;
    // Root objects under management, keyed by create token. Each root owns its
    // complete runtime attachment tree.
    std::map<std::uint64_t, std::unique_ptr<GSObject>> m_rootObjects;
    std::map<std::uint64_t, CreateOperation> m_createRequests;
    std::vector<std::unique_ptr<GSReqCommand>> m_commandRequests;
    std::unique_ptr<FindOperation> m_findOperation;
    std::vector<std::unique_ptr<GSObject>> m_retiredObjects;
    std::set<AircraftId> m_createdObjects;
    std::set<AircraftId> m_configuredAircraft;
    std::map<AircraftId, std::set<AircraftId>> m_objectsByAircraft;
    std::unordered_map<AircraftId, AircraftId> m_aircraftByObject;
    // In-flight create count per aircraft.
    std::map<AircraftId, std::size_t> m_pendingCreations;
    // Removal intents stay pending until all GSObjects for the aircraft have
    // completed creation and subtype-specific finalization.
    std::set<AircraftId> m_deferredAircraftRemovals;
    // MSFS may report a ground object gone while its object graph is still being
    // finalized. Keep driving that graph and retire its subtree only afterward.
    std::set<AircraftId> m_deferredObjectRemovals;
    std::vector<AircraftSnapshot> m_aircraftSnapshotBuffer;
    std::vector<GroundServiceRequest> m_serviceRequestBuffer;
    DWORD m_nextObjectDataDefinition = 10'000;
    DWORD m_nextObjectClientEvent = 10'000;
    SIMCONNECT_DATA_DEFINITION_ID m_findPoseDefinition{};
    std::mt19937 m_random{std::random_device{}()};

    mutable std::mutex m_statusMutex;
    GroundServicesStatus m_status;
    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
