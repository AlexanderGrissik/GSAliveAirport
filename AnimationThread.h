#pragma once

#include "AnimationObject.h"
#include "LogSink.h"

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace parking_services
{
class SimConnectThread;
class AircraftTrackerThread;
struct AnimationProbeSample;

struct AnimationStatus
{
    std::size_t walkingWorkers{};
    bool probeActive{};
    AircraftId probeObjectId{};
    std::size_t probeSamples{};
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
    void AddObject(AircraftId objectId, GroundServiceAnimation animation,
                   std::optional<AnimationRoute> route = std::nullopt);
    void RemoveObject(AircraftId objectId);
    void Reset();
    void StartProbe(AircraftId objectId);
    void StopProbe(bool announce = true);
    [[nodiscard]] AnimationStatus Status() const;

  private:
    struct AnimationDistance
    {
        AircraftId animationId{};
        double meters{};
    };

    static void AnimationLoop(std::stop_token stopToken, AnimationThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void Tick(std::chrono::steady_clock::time_point now);
    void AddObjectInternal(AircraftId objectId, GroundServiceAnimation animation,
                           std::optional<AnimationRoute> route);
    void RemoveObjectInternal(AircraftId objectId);
    void HandleSimulatorObjectRemoved(AircraftId objectId);
    void ResetInternal();
    void StartProbeInternal(AircraftId objectId);
    void StopProbeInternal(bool announce);
    void RecordProbeSample(AnimationProbeSample sample);
    void PublishStatus();
    [[nodiscard]] bool ContainsAnimationObject(AircraftId objectId) const;

    SimConnectThread &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    LogSink m_log;
    AnimationFrame m_frame;
    std::map<AircraftId, std::unique_ptr<AnimationObject>> m_animations;
    std::vector<AnimationDistance> m_distanceRanking;

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
