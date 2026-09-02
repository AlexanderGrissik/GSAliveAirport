#include "AnimationThread.h"

#include "AircraftTrackerThread.h"
#include "AnimationConfiguredObject.h"
#include "SimConnectThread.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kUpdateInterval = std::chrono::microseconds(33'333);
}

AnimationThread::AnimationThread(SimConnectThread &simConnect,
                                 AircraftTrackerThread &aircraftTracker, LogSink log)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker),
      m_log(std::move(log)), m_frame(m_simConnect)
{
    m_simConnect.SubscribeConnection([this](bool connected) {
        if (!connected) Post([this] { ResetInternal(); });
    });
    m_simConnect.SubscribeObjectRemoved(
        [this](AircraftId objectId) {
            Post([this, objectId] { HandleSimulatorObjectRemoved(objectId); });
        });
    m_thread = std::jthread(&AnimationThread::AnimationLoop, this);
}

AnimationThread::~AnimationThread()
{
    Stop();
}

void AnimationThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
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

void AnimationThread::StartProbe(AircraftId objectId)
{
    Post([this, objectId] { StartProbeInternal(objectId); });
}

void AnimationThread::StopProbe(bool announce)
{
    Post([this, announce] { StopProbeInternal(announce); });
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
    if (m_probeActive && m_probeObjectId == objectId) StopProbeInternal(false);
}

void AnimationThread::HandleSimulatorObjectRemoved(AircraftId objectId)
{
    const bool wasAnimated = ContainsAnimationObject(objectId);
    const bool wasProbed = m_probeActive && m_probeObjectId == objectId;
    RemoveObjectInternal(objectId);
    if (wasAnimated || wasProbed) {
        m_log("MSFS removed ObjectID " + std::to_string(objectId) +
              "; cleared its animation and probe state.");
    }
}

void AnimationThread::ResetInternal()
{
    for (const auto &[animationId, animation] : m_animations) {
        static_cast<void>(animationId);
        animation->Cancel(m_frame);
    }
    m_animations.clear();
    StopProbeInternal(false);
    PublishStatus();
}

void AnimationThread::StartProbeInternal(AircraftId objectId)
{
    StopProbeInternal(false);
    if (!m_simConnect.IsConnected() || objectId == 0) {
        m_log("Could not start animation probe: SimConnect is unavailable or ObjectID is invalid.");
        return;
    }
    std::array<wchar_t, 32768> executablePath{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
                                            static_cast<DWORD>(executablePath.size()));
    m_probePath = "animation_probe.csv";
    if (length != 0 && length < executablePath.size()) {
        m_probePath = std::filesystem::path(executablePath.data()).parent_path() /
                      m_probePath;
    }
    m_probeFile.open(m_probePath, std::ios::trunc);
    if (!m_probeFile) {
        m_log("Could not open animation_probe.csv for writing.");
        return;
    }
    m_probeFile << "elapsed_seconds,object_id,title,ground_velocity_knots,velocity_body_x_mps,"
                   "velocity_body_y_mps,velocity_body_z_mps,heading_degrees,latitude,longitude,"
                   "altitude_feet,on_ground\n";
    m_probeActive = true;
    m_probeObjectId = objectId;
    m_probeSamples = 0;
    PublishStatus();
    m_simConnect.StartAnimationProbe(objectId, [this](AnimationProbeSample sample) mutable {
        Post([this, sample = std::move(sample)]() mutable {
            RecordProbeSample(std::move(sample));
        });
    });
    m_log("Recording ObjectID " + std::to_string(objectId) +
          " until stopprobe. Output: " + m_probePath.string());
}

void AnimationThread::StopProbeInternal(bool announce)
{
    if (!m_probeActive) {
        if (announce) m_log("No animation probe is currently recording.");
        return;
    }
    m_simConnect.StopAnimationProbe();
    m_probeFile.flush();
    m_probeFile.close();
    const auto samples = m_probeSamples;
    m_probeActive = false;
    m_probeObjectId = 0;
    PublishStatus();
    if (announce) {
        m_log("Stopped animation probe after " + std::to_string(samples) +
              " samples. Output: " + m_probePath.string());
    }
}

void AnimationThread::RecordProbeSample(AnimationProbeSample sample)
{
    if (!m_probeActive || sample.objectId != m_probeObjectId || !m_probeFile) return;
    for (std::size_t quote = 0;
         (quote = sample.title.find('"', quote)) != std::string::npos; quote += 2) {
        sample.title.insert(quote, 1, '"');
    }
    m_probeFile << std::fixed << std::setprecision(6) << sample.elapsedSeconds << ','
                << sample.objectId << ",\"" << sample.title << "\","
                << sample.groundSpeedKnots << ',' << sample.velocityBodyXMetersPerSecond << ','
                << sample.velocityBodyYMetersPerSecond << ','
                << sample.velocityBodyZMetersPerSecond << ',' << sample.headingDegrees << ','
                << sample.latitude << ',' << sample.longitude << ',' << sample.altitudeFeet << ','
                << sample.onGround << '\n';
    ++m_probeSamples;
    if (m_probeSamples % 20 == 0) m_probeFile.flush();
    PublishStatus();
}

void AnimationThread::PublishStatus()
{
    const std::size_t walkingWorkers = static_cast<std::size_t>(
        std::ranges::count_if(m_animations, [](const auto &entry) {
            return entry.second->IsWalking();
        }));
    std::scoped_lock lock(m_statusMutex);
    m_status = {walkingWorkers, m_probeActive, m_probeObjectId, m_probeSamples};
}

bool AnimationThread::ContainsAnimationObject(AircraftId objectId) const
{
    return std::ranges::any_of(m_animations, [objectId](const auto &entry) {
        return entry.second->OwnsObject(objectId);
    });
}
} // namespace parking_services
