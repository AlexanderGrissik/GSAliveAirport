#pragma once

#include "Aircraft.h"
#include "AnimationThread.h"
#include "GroundServicesConfig.h"
#include "LogSink.h"
#include "SimConnectIds.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
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
class SimConnectThread;
struct BaggageLoaderGeometry;

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
                         AnimationThread &animation,
                         GroundServicesConfig &configuration, LogSink log);
    ~GroundServicesThread();

    GroundServicesThread(const GroundServicesThread &) = delete;
    GroundServicesThread &operator=(const GroundServicesThread &) = delete;

    void Stop();
    void Reset();
    [[nodiscard]] GroundServicesStatus Status() const;

  private:
    struct SpawnPose
    {
        double latitude{};
        double longitude{};
        double altitudeFeet{};
        double headingDegrees{};
        bool onGround{true};
    };

    struct CargoDoorPlacement
    {
        std::uint32_t interactivePointIndex{};
        double cargoForwardMeters{};
        double cargoRightMeters{};
        double cargoHeightMeters{};
        double modelRelativeHeadingDegrees{};
    };

    struct PendingCreate
    {
        AircraftSnapshot aircraft;
        GroundServiceObject object;
        SpawnPose pose;
        std::optional<AnimationRoute> route;
        std::optional<CargoDoorPlacement> cargoDoor;
        AircraftId parentObjectId{};
        bool cancelled{};
    };

    struct PendingCargoDoorAlignment
    {
        enum class Stage { MeasureInitialGeometry, WaitForRampTarget };

        AircraftSnapshot aircraft;
        GroundServiceObject object;
        SpawnPose initialPose;
        std::optional<AnimationRoute> route;
        AircraftId parentObjectId{};
        CargoDoorPlacement cargoDoor;
        double rampAngleDegrees{};
        std::chrono::steady_clock::time_point geometryRequestDue{};
        Stage stage{Stage::MeasureInitialGeometry};
        bool geometryRequested{};
    };

    static void GroundServicesLoop(std::stop_token stopToken,
                                   GroundServicesThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void MaintainCargoDoorAlignments(std::chrono::steady_clock::time_point now);
    void CompleteCargoDoorAlignment(AircraftId objectId, BaggageLoaderGeometry geometry);
    void EvaluateTrackedAircraft();
    void ResolveConfigurationIfAvailable();
    static GroundServicesDecision Decide(const AircraftSnapshot &aircraft);
    void EnsureAutomaticServices(const AircraftSnapshot &aircraft);
    void QueueService(const AircraftSnapshot &aircraft, const GroundServiceRequest &request);
    void QueueObject(PendingCreate pending);
    void QueueAttachments(const AircraftSnapshot &aircraft, AircraftId parentObjectId,
                          const SpawnPose &parentPose,
                          const std::vector<GroundServiceObject> &attachments);
    void CompleteCreate(std::uint64_t token, AircraftId objectId);
    void FinalizeCreatedObject(AircraftId objectId, const PendingCreate &created,
                               const SpawnPose &actualPose);
    void RemoveForAircraft(AircraftId aircraftId, bool closeCargoDoors = true);
    void RemoveCreatedObject(AircraftId objectId, bool requestSimulatorRemoval);
    void CloseCargoDoors(AircraftId aircraftId);
    void HandleObjectRemoved(AircraftId objectId);
    void HandleConnection(bool connected);
    void RemoveAllServicesInternal();
    void PublishStatus();
    [[nodiscard]] bool HasPendingFor(AircraftId aircraftId) const;
    [[nodiscard]] static SpawnPose RelativeToAircraft(const AircraftSnapshot &aircraft,
                                                       double relX, double relY,
                                                       bool faceAircraft);
    [[nodiscard]] static SpawnPose RelativeToParent(const SpawnPose &parent,
                                                     const GroundServiceObject &child);
    [[nodiscard]] static SIMCONNECT_DATA_INITPOSITION ToInitialPosition(
        const SpawnPose &pose);

    SimConnectThread &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    AnimationThread &m_animation;
    GroundServicesConfig &m_configuration;
    LogSink m_log;
    bool m_connected = false;
    std::atomic_bool m_stopping{false};
    std::uint64_t m_nextCreateToken = 1;
    std::map<std::uint64_t, PendingCreate> m_pendingCreates;
    std::map<AircraftId, PendingCargoDoorAlignment> m_pendingCargoDoorAlignments;
    std::set<AircraftId> m_createdObjects;
    std::set<AircraftId> m_configuredAircraft;
    std::map<AircraftId, std::set<AircraftId>> m_objectsByAircraft;
    std::unordered_map<AircraftId, AircraftId> m_aircraftByObject;
    std::unordered_map<AircraftId, AircraftId> m_parentByObject;
    std::map<AircraftId, std::set<AircraftId>> m_childrenByObject;
    std::map<AircraftId, SpawnPose> m_createdPoses;
    std::map<AircraftId, std::set<std::uint32_t>> m_openCargoDoorIndices;
    std::map<AircraftId, std::uint32_t> m_cargoDoorPointByObject;
    std::vector<AircraftSnapshot> m_aircraftSnapshotBuffer;
    std::vector<std::string> m_catalogTitleBuffer;
    std::vector<std::string> m_configurationMessageBuffer;
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
