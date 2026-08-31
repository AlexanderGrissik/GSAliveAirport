#pragma once

#include "Aircraft.h"
#include "AnimationThread.h"
#include "GroundServicesConfig.h"
#include "LogSink.h"

#include <atomic>
#include <condition_variable>
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
    void SpawnFullTest();
    void ClearCreated(bool announce = true);
    void Reset();
    [[nodiscard]] GroundServicesStatus Status() const;

  private:
    enum class PendingCreateKind
    {
        Standard,
        BaggageBeltLoader,
        BaggageBeltWorker,
    };

    struct PendingCreate
    {
        AircraftSnapshot aircraft;
        std::string title;
        std::optional<RelativeWalkingPath> walkingPath;
        PendingCreateKind kind{PendingCreateKind::Standard};
        std::string companionTitle;
        AircraftId pairedObjectId{};
        double forwardMeters{};
        double rightMeters{};
        double baggageBeltRampAngleDegrees{};
        double cargoHeightMeters{};
        double modelRelativeHeadingDegrees{};
        bool faceAircraft{};
        std::optional<double> headingDegrees;
        bool cancelled{};
    };

    struct PendingBaggageBeltAlignment
    {
        enum class Stage { MeasureInitialGeometry, WaitForRampTarget };

        AircraftSnapshot aircraft;
        std::string workerTitle;
        double cargoForwardMeters{};
        double cargoRightMeters{};
        double cargoHeightMeters{};
        double modelRelativeHeadingDegrees{};
        double headingDegrees{};
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
    void MaintainBaggageBeltAlignments(std::chrono::steady_clock::time_point now);
    void EvaluateTrackedAircraft();
    void ResolveConfigurationIfAvailable();
    static GroundServicesDecision Decide(const AircraftSnapshot &aircraft);
    void EnsureAutomaticServices(const AircraftSnapshot &aircraft);
    void RemoveForAircraft(AircraftId aircraftId, bool closeCargoDoor = true);
    void CloseCargoDoor(AircraftId aircraftId);
    void RequestObject(const AircraftSnapshot &aircraft, std::string title,
                       double forwardMeters, double rightMeters,
                       std::optional<RelativeWalkingPath> walkingPath = std::nullopt,
                       bool faceAircraft = false,
                       std::optional<double> headingDegrees = std::nullopt);
    bool RequestPassengerBaggageBelt(const AircraftSnapshot &aircraft,
                                     std::string title);
    void QueueCreate(PendingCreate pending);
    void CompleteCreate(std::uint64_t token, AircraftId objectId);
    void CompleteBaggageBeltAlignment(AircraftId loaderObjectId,
                                      BaggageLoaderGeometry geometry);
    void HandleObjectRemoved(AircraftId objectId);
    void HandleConnection(bool connected);
    void SpawnFullTestInternal();
    void ClearCreatedInternal(bool announce);
    void PublishStatus();
    [[nodiscard]] bool HasPendingFor(AircraftId aircraftId) const;

    SimConnectThread &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    AnimationThread &m_animation;
    GroundServicesConfig &m_configuration;
    LogSink m_log;
    bool m_connected = false;
    std::atomic_bool m_stopping{false};
    std::uint64_t m_nextCreateToken = 1;
    std::map<std::uint64_t, PendingCreate> m_pendingCreates;
    std::map<AircraftId, PendingBaggageBeltAlignment> m_pendingBaggageBeltAlignments;
    std::set<AircraftId> m_createdObjects;
    std::set<AircraftId> m_configuredAircraft;
    std::map<AircraftId, std::set<AircraftId>> m_objectsByAircraft;
    std::unordered_map<AircraftId, AircraftId> m_aircraftByObject;
    std::map<AircraftId, std::uint32_t> m_openCargoDoorIndices;
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
