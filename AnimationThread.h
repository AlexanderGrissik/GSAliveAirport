#pragma once

#include "Aircraft.h"
#include "LogSink.h"

#include <array>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace parking_services
{
class SimConnectThread;
class AircraftTrackerThread;
struct AnimationUpdate;
struct BaggageBeltAnimationUpdate;
struct AnimationProbeSample;

enum class BaggageBeltDirection { Load, Unload };

struct AnimationStatus
{
    std::size_t walkingWorkers{};
    bool probeActive{};
    AircraftId probeObjectId{};
    std::size_t probeSamples{};
};

struct RelativeWalkingPath
{
    double relX1{};
    double relY1{};
    double relX2{};
    double relY2{};
};

class AnimationThread final
{
  public:
    static constexpr std::size_t MaximumWalkingWorkers = 20;

    AnimationThread(SimConnectThread &simConnect,
                    AircraftTrackerThread &aircraftTracker, LogSink log);
    ~AnimationThread();

    AnimationThread(const AnimationThread &) = delete;
    AnimationThread &operator=(const AnimationThread &) = delete;

    void Stop();
    void AddWorker(AircraftId objectId, std::string title, AircraftSnapshot aircraft);
    void AddWorker(AircraftId objectId, std::string title, AircraftSnapshot aircraft,
                   RelativeWalkingPath path);
    void AddBaggageBelt(AircraftId loaderObjectId, AircraftId workerObjectId,
                        double rampAngleDegrees, BaggageBeltDirection direction);
    void RemoveObject(AircraftId objectId);
    void Reset();
    void StartProbe(AircraftId objectId);
    void StopProbe(bool announce = true);
    [[nodiscard]] AnimationStatus Status() const;

  private:
    struct Worker
    {
        struct RoutePoint
        {
            double latitude{};
            double longitude{};
            double altitudeFeet{};
        };

        std::string title;
        std::array<RoutePoint, 4> route{};
        std::array<double, 4> segmentLengths{};
        std::size_t routePointCount{};
        double routeLength{};
        double routeDistance{};
        double currentLatitude{};
        double currentLongitude{};
        double currentAltitudeFeet{};
        double currentHeadingDegrees{};
        bool walking{};
        std::chrono::steady_clock::time_point animationStarted{};
        std::chrono::steady_clock::time_point movementUpdated{};
    };

    struct WorkerDistance
    {
        AircraftId objectId{};
        double meters{};
    };

    struct BaggageBelt
    {
        AircraftId loaderObjectId{};
        AircraftId workerObjectId{};
        double rampAngleDegrees{};
        BaggageBeltDirection direction{BaggageBeltDirection::Load};
        std::chrono::steady_clock::time_point animationStarted{};
    };

    static void AnimationLoop(std::stop_token stopToken, AnimationThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void Tick(std::chrono::steady_clock::time_point now);
    void AddWorkerInternal(AircraftId objectId, std::string title,
                           const AircraftSnapshot &aircraft);
    void AddWorkerInternal(AircraftId objectId, std::string title,
                           const AircraftSnapshot &aircraft,
                           const RelativeWalkingPath &path);
    void AddWorkerInternal(AircraftId objectId, std::string title,
                           const AircraftSnapshot &aircraft,
                           const std::pair<double, double> *offsets,
                           std::size_t offsetCount);
    void AddBaggageBeltInternal(AircraftId loaderObjectId,
                                AircraftId workerObjectId,
                                double rampAngleDegrees,
                                BaggageBeltDirection direction);
    void RemoveObjectInternal(AircraftId objectId);
    void HandleSimulatorObjectRemoved(AircraftId objectId);
    void ResetInternal();
    void StartProbeInternal(AircraftId objectId);
    void StopProbeInternal(bool announce);
    void RecordProbeSample(AnimationProbeSample sample);
    void PublishStatus();

    SimConnectThread &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    LogSink m_log;
    std::map<AircraftId, Worker> m_workers;
    std::map<AircraftId, BaggageBelt> m_baggageBelts;
    std::vector<WorkerDistance> m_distanceRanking;
    std::vector<AnimationUpdate> m_updateBuffer;
    std::vector<BaggageBeltAnimationUpdate> m_baggageBeltUpdateBuffer;

    bool m_probeActive = false;
    AircraftId m_probeObjectId = 0;
    std::size_t m_probeSamples = 0;
    std::filesystem::path m_probePath;
    std::ofstream m_probeFile;

    mutable std::mutex m_statusMutex;
    AnimationStatus m_status;
    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
