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

namespace parking_services
{
class SimConnectThread;
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
    static constexpr std::size_t MaximumWorkers = 20;

    AnimationThread(SimConnectThread &simConnect, LogSink log);
    ~AnimationThread();

    AnimationThread(const AnimationThread &) = delete;
    AnimationThread &operator=(const AnimationThread &) = delete;

    void Stop();
    void AddWorker(AircraftId objectId, std::string title, AircraftSnapshot aircraft);
    void RemoveWorker(AircraftId objectId);
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
        std::chrono::steady_clock::time_point started{};
        std::array<RoutePoint, 4> route{};
    };

    static void AnimationLoop(std::stop_token stopToken, AnimationThread *self);
    void RunLoop(std::stop_token stopToken);
    void Post(std::function<void()> command);
    void ProcessCommands();
    void Tick(std::chrono::steady_clock::time_point now);
    void AddWorkerInternal(AircraftId objectId, std::string title,
                           const AircraftSnapshot &aircraft);
    void RemoveWorkerInternal(AircraftId objectId);
    void HandleSimulatorObjectRemoved(AircraftId objectId);
    void ResetInternal();
    void StartProbeInternal(AircraftId objectId);
    void StopProbeInternal(bool announce);
    void RecordProbeSample(AnimationProbeSample sample);
    void PublishStatus();

    SimConnectThread &m_simConnect;
    LogSink m_log;
    std::map<AircraftId, Worker> m_workers;

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
