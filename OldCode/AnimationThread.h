#pragma once

#include "AnimatedObject.h"
#include "GSRequests/GSReqCommand.h"
#include "ISimConnectHandler.h"

#include <chrono>
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
#include <utility>
#include <vector>

namespace parking_services
{
class AircraftTrackerThread;

struct AnimationStatus
{
    std::size_t movingObjects{};
};

// Executes generic AnimatedObject instances supplied and removed by
// GroundServicesThread. It owns no simulator-object lifecycle decisions.
class AnimationThread final
{
  public:
    static constexpr std::size_t MaximumMovingObjects = 20;

    AnimationThread(ISimConnectHandler &simConnect,
                    AircraftTrackerThread &aircraftTracker);
    ~AnimationThread();

    AnimationThread(const AnimationThread &) = delete;
    AnimationThread &operator=(const AnimationThread &) = delete;

    void Start();
    void Stop();
    void InitializeSimConnect();
    void AddObject(std::unique_ptr<AnimatedObject> object);
    void RemoveObject(AircraftId objectId);
    void Reset();
    [[nodiscard]] AnimationStatus Status() const;

  private:
    friend class AnimatedObject;

    struct MotionUpdate
    {
        AircraftId objectId{};
        AnimationCoordinate coordinate;
        double velocityBodyYMetersPerSecond{};
    };

    struct PositionUpdate
    {
        AircraftId objectId{};
        AnimationCoordinate coordinate;
    };

    struct CarrierUpdate
    {
        AircraftId objectId{};
        std::string carrier;
        double value{};
    };

    struct AnimationDistance
    {
        AircraftId animationId{};
        double meters{};
    };

    static void AnimationLoop(std::stop_token stopToken, AnimationThread *self);
    void RunLoop(std::stop_token stopToken);
    template <typename Command> void Post(Command &&command)
    {
        {
            std::scoped_lock lock(m_commandMutex);
            m_commands.emplace_back(std::forward<Command>(command));
        }
        m_wake.notify_all();
    }
    void ProcessCommands();
    void Tick(std::chrono::steady_clock::time_point now);
    void AddObjectInternal(std::unique_ptr<AnimatedObject> object);
    void RemoveObjectInternal(AircraftId objectId);
    void ResetInternal();
    void PublishStatus();
    [[nodiscard]] bool ContainsAnimationObject(AircraftId objectId) const;

    [[nodiscard]] std::chrono::steady_clock::time_point Now() const;
    void FreezeObject(AircraftId objectId);
    void CancelObjectAnimation(AircraftId objectId);
    void QueueMotionUpdate(AircraftId objectId,
                           const AnimationCoordinate &coordinate,
                           double velocityBodyYMetersPerSecond);
    void QueuePositionUpdate(AircraftId objectId,
                             const AnimationCoordinate &coordinate);
    void QueueCarrierUpdate(AircraftId objectId, std::string carrier,
                            double value);
    void Flush();
    GSReqCommand &NewCommandRequest();
    void CollectFinishedRequests();

    ISimConnectHandler &m_simConnect;
    AircraftTrackerThread &m_aircraftTracker;
    std::chrono::steady_clock::time_point m_now{};
    std::map<AircraftId, std::unique_ptr<AnimatedObject>> m_animations;
    std::vector<AnimationDistance> m_distanceRanking;
    std::map<AircraftId, MotionUpdate> m_motionUpdates;
    std::map<AircraftId, PositionUpdate> m_positionUpdates;
    std::map<std::pair<AircraftId, std::string>, CarrierUpdate> m_carrierUpdates;
    std::map<std::string, SIMCONNECT_DATA_DEFINITION_ID, std::less<>>
        m_carrierDefinitions;
    DWORD m_nextCarrierDefinitionId = 100;
    std::vector<std::unique_ptr<GSReqCommand>> m_commandRequests;

    mutable std::mutex m_statusMutex;
    AnimationStatus m_status;
    std::mutex m_commandMutex;
    std::condition_variable_any m_wake;
    std::deque<std::packaged_task<void()>> m_commands;
    std::jthread m_thread;
};
} // namespace parking_services
