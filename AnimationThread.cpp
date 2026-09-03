#include "AnimationThread.h"

#include "AircraftTrackerThread.h"
#include "AnimationConfiguredObject.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kUpdateInterval = std::chrono::microseconds(33'333);
}

AnimationThread::AnimationThread(ISimConnectHandler &simConnect,
                                 AircraftTrackerThread &aircraftTracker, LogSink log)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker),
      m_log(std::move(log)), m_frame(m_simConnect)
{
}

AnimationThread::~AnimationThread()
{
    Stop();
}

void AnimationThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&AnimationThread::AnimationLoop, this);
}

void AnimationThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
}

void AnimationThread::OnSimConnected()
{
    Post([this] { m_frame.InitializeSimConnect(); });
}

void AnimationThread::OnSimStarted()
{
    // No action needed on (re)connect; the loop picks up new work on demand.
}

void AnimationThread::OnSimDisconnected()
{
    Post([this] {
        ResetInternal();
        m_frame.ResetDefinitions();
    });
}

void AnimationThread::OnSimStopped()
{
    Post([this] { ResetInternal(); });
}

void AnimationThread::OnObjRemoved(std::uint32_t objectId)
{
    Post([this, objectId] { HandleSimulatorObjectRemoved(objectId); });
}

void AnimationThread::AddObject(AircraftId objectId, GroundServiceAnimation animation,
                                std::optional<AnimationRoute> route)
{
    Post([this, objectId, animation = std::move(animation), route = std::move(route)]() mutable {
        AddObjectInternal(objectId, std::move(animation), std::move(route));
    });
}

void AnimationThread::RemoveObject(AircraftId objectId)
{
    Post([this, objectId] { RemoveObjectInternal(objectId); });
}

void AnimationThread::Reset()
{
    Post([this] { ResetInternal(); });
}

AnimationStatus AnimationThread::Status() const
{
    std::scoped_lock lock(m_statusMutex);
    return m_status;
}

void AnimationThread::AnimationLoop(std::stop_token stopToken, AnimationThread *self)
{
    self->RunLoop(stopToken);
}

void AnimationThread::RunLoop(std::stop_token stopToken)
{
    auto nextUpdate = std::chrono::steady_clock::now();
    while (!stopToken.stop_requested()) {
        ProcessCommands();
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextUpdate) {
            Tick(now);
            nextUpdate = now + kUpdateInterval;
        }
        std::unique_lock lock(m_commandMutex);
        m_wake.wait_until(lock, stopToken, nextUpdate,
                          [this] { return !m_commands.empty(); });
    }
    ProcessCommands();
    ResetInternal();
}

void AnimationThread::Post(std::function<void()> command)
{
    {
        std::scoped_lock lock(m_commandMutex);
        m_commands.push_back(std::move(command));
    }
    m_wake.notify_all();
}

void AnimationThread::ProcessCommands()
{
    std::deque<std::function<void()>> commands;
    {
        std::scoped_lock lock(m_commandMutex);
        commands.swap(m_commands);
    }
    for (auto &command : commands) command();
}

void AnimationThread::Tick(std::chrono::steady_clock::time_point now)
{
    m_frame.Begin(now);
    ApproximateUserPosition userPosition{};
    const bool hasUserPosition =
        m_aircraftTracker.TryGetApproximateUserPosition(userPosition);

    m_distanceRanking.clear();
    if (hasUserPosition) {
        if (m_distanceRanking.capacity() < m_animations.size()) {
            m_distanceRanking.reserve(m_animations.size());
        }
        for (const auto &[animationId, animation] : m_animations) {
            const auto target = animation->ProximityTarget();
            if (!target) continue;
            m_distanceRanking.push_back({
                animationId,
                DistanceMeters(userPosition.latitude, userPosition.longitude,
                               target->latitude, target->longitude)});
        }
        std::ranges::sort(m_distanceRanking, [](const AnimationDistance &left,
                                                const AnimationDistance &right) {
            if (left.meters != right.meters) return left.meters < right.meters;
            return left.animationId < right.animationId;
        });
    }

    const std::size_t activeCount = (std::min)(
        MaximumWalkingWorkers, m_distanceRanking.size());
    bool selectionChanged = false;
    for (auto &[animationId, animation] : m_animations) {
        const bool active = std::ranges::any_of(
            m_distanceRanking.begin(), m_distanceRanking.begin() + activeCount,
            [animationId](const AnimationDistance &entry) {
                return entry.animationId == animationId;
            });
        selectionChanged |= animation->SetDistanceRankedActive(active, m_frame);
    }

    for (auto &[animationId, animation] : m_animations) {
        static_cast<void>(animationId);
        animation->Animate(m_frame);
    }
    m_frame.Flush();
    if (selectionChanged) PublishStatus();
}

void AnimationThread::AddObjectInternal(AircraftId objectId,
                                        GroundServiceAnimation animation,
                                        std::optional<AnimationRoute> route)
{
    if (ContainsAnimationObject(objectId)) return;
    auto configured = std::make_unique<AnimationConfiguredObject>(
        objectId, std::move(animation), std::move(route));
    if (!configured->IsValid()) {
        m_log("Could not register an invalid configured animation for ObjectID " +
              std::to_string(objectId) + ".");
        return;
    }
    configured->Start(m_frame);
    const bool walking = configured->ProximityTarget().has_value();
    m_animations.emplace(objectId, std::move(configured));
    m_log("Registered configured animation for ObjectID " +
          std::to_string(objectId) +
          (walking ? " as a distance-ranked walking candidate." : "."));
}

void AnimationThread::RemoveObjectInternal(AircraftId objectId)
{
    bool changed = false;
    for (auto animation = m_animations.begin(); animation != m_animations.end();) {
        if (animation->second->OwnsObject(objectId)) {
            animation->second->Cancel(m_frame);
            animation = m_animations.erase(animation);
            changed = true;
        } else {
            ++animation;
        }
    }
    if (changed) PublishStatus();
}

void AnimationThread::HandleSimulatorObjectRemoved(AircraftId objectId)
{
    const bool wasAnimated = ContainsAnimationObject(objectId);
    RemoveObjectInternal(objectId);
    if (wasAnimated) {
        m_log("MSFS removed ObjectID " + std::to_string(objectId) +
              "; cleared its animation.");
    }
}

void AnimationThread::ResetInternal()
{
    for (const auto &[animationId, animation] : m_animations) {
        static_cast<void>(animationId);
        animation->Cancel(m_frame);
    }
    m_animations.clear();
    PublishStatus();
}

void AnimationThread::PublishStatus()
{
    const std::size_t walkingWorkers = static_cast<std::size_t>(
        std::ranges::count_if(m_animations, [](const auto &entry) {
            return entry.second->IsWalking();
        }));
    std::scoped_lock lock(m_statusMutex);
    m_status = {walkingWorkers};
}

bool AnimationThread::ContainsAnimationObject(AircraftId objectId) const
{
    return std::ranges::any_of(m_animations, [objectId](const auto &entry) {
        return entry.second->OwnsObject(objectId);
    });
}
} // namespace parking_services
