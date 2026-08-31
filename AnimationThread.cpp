#include "AnimationThread.h"

#include "AircraftTrackerThread.h"
#include "SimConnectThread.h"
#include "SimObjectPositioning.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr double kWalkingSpeedMetersPerSecond = 2.5 * 0.514444;
constexpr double kAnimationFramesPerSecond = 30.0;
constexpr auto kUpdateInterval = std::chrono::microseconds(33'333);
constexpr std::string_view kWingwalkerTitle = "FSDT_Wingwalker_Male_04";
}

AnimationThread::AnimationThread(SimConnectThread &simConnect,
                                 AircraftTrackerThread &aircraftTracker, LogSink log)
    : m_simConnect(simConnect), m_aircraftTracker(aircraftTracker),
      m_log(std::move(log))
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

void AnimationThread::AddWorker(AircraftId objectId, std::string title,
                                AircraftSnapshot aircraft)
{
    Post([this, objectId, title = std::move(title), aircraft = std::move(aircraft)]() mutable {
        AddWorkerInternal(objectId, std::move(title), aircraft);
    });
}

void AnimationThread::AddWorker(AircraftId objectId, std::string title,
                                AircraftSnapshot aircraft, RelativeWalkingPath path)
{
    Post([this, objectId, title = std::move(title), aircraft = std::move(aircraft),
          path]() mutable {
        AddWorkerInternal(objectId, std::move(title), aircraft, path);
    });
}

void AnimationThread::RemoveWorker(AircraftId objectId)
{
    Post([this, objectId] { RemoveWorkerInternal(objectId); });
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
    ApproximateUserPosition userPosition{};
    const bool hasUserPosition =
        m_aircraftTracker.TryGetApproximateUserPosition(userPosition);

    m_distanceRanking.clear();
    if (hasUserPosition) {
        if (m_distanceRanking.capacity() < m_workers.size()) {
            m_distanceRanking.reserve(m_workers.size());
        }
        for (const auto &[objectId, worker] : m_workers) {
            m_distanceRanking.push_back({
                objectId,
                DistanceMeters(userPosition.latitude, userPosition.longitude,
                               worker.currentLatitude, worker.currentLongitude)});
        }
        std::ranges::sort(m_distanceRanking, [](const WorkerDistance &left,
                                                const WorkerDistance &right) {
            if (left.meters != right.meters) return left.meters < right.meters;
            return left.objectId < right.objectId;
        });
    }

    const std::size_t walkingCount = (std::min)(
        MaximumWalkingWorkers, m_distanceRanking.size());
    m_updateBuffer.clear();
    if (m_updateBuffer.capacity() < walkingCount) {
        m_updateBuffer.reserve(walkingCount);
    }

    bool selectionChanged = false;
    for (auto &[objectId, worker] : m_workers) {
        bool shouldWalk = false;
        for (std::size_t index = 0; index < walkingCount; ++index) {
            if (m_distanceRanking[index].objectId == objectId) {
                shouldWalk = true;
                break;
            }
        }

        const bool marshaller = worker.title.starts_with("FSDT_Marshaller_");
        const double transitionStart = marshaller ? 1445.0 : 192.0;
        const double transitionEnd = marshaller ? 1530.0 : 229.0;
        const double loopStart = marshaller ? 1530.0 : 230.0;
        const double loopEnd = marshaller ? 1572.0 : 268.0;

        if (!shouldWalk) {
            if (worker.walking) {
                worker.walking = false;
                selectionChanged = true;
                m_updateBuffer.push_back({objectId, worker.currentLatitude,
                                          worker.currentLongitude,
                                          worker.currentAltitudeFeet,
                                          worker.currentHeadingDegrees,
                                          transitionStart});
            }
            continue;
        }

        if (!worker.walking) {
            worker.walking = true;
            worker.animationStarted = now;
            worker.movementUpdated = now;
            m_simConnect.FreezeObject(objectId);
            selectionChanged = true;
        }

        const double movementSeconds = std::chrono::duration<double>(
            now - worker.movementUpdated).count();
        worker.movementUpdated = now;
        worker.routeDistance = std::fmod(
            worker.routeDistance + movementSeconds * kWalkingSpeedMetersPerSecond,
            worker.routeLength);

        double segmentDistance = worker.routeDistance;
        std::size_t segment = 0;
        while (segment + 1 < worker.routePointCount &&
               segmentDistance > worker.segmentLengths[segment]) {
            segmentDistance -= worker.segmentLengths[segment++];
        }
        const auto &from = worker.route[segment];
        const auto &to = worker.route[(segment + 1) % worker.routePointCount];
        const double fraction = worker.segmentLengths[segment] > 0.0
                                    ? segmentDistance / worker.segmentLengths[segment]
                                    : 0.0;
        const double north = to.latitude - from.latitude;
        const double east = to.longitude - from.longitude;
        const double heading = std::fmod(std::atan2(east, north) * 180.0 /
                                             3.14159265358979323846 + 360.0,
                                         360.0);
        const double elapsed = std::chrono::duration<double>(
            now - worker.animationStarted).count();
        const double transitionDuration =
            (transitionEnd - transitionStart) / kAnimationFramesPerSecond;
        const double frame = elapsed < transitionDuration
            ? transitionStart + elapsed * kAnimationFramesPerSecond
            : loopStart + std::fmod((elapsed - transitionDuration) *
                                        kAnimationFramesPerSecond,
                                    loopEnd - loopStart);
        worker.currentLatitude =
            from.latitude + (to.latitude - from.latitude) * fraction;
        worker.currentLongitude =
            from.longitude + (to.longitude - from.longitude) * fraction;
        worker.currentAltitudeFeet =
            from.altitudeFeet + (to.altitudeFeet - from.altitudeFeet) * fraction;
        worker.currentHeadingDegrees = heading;
        m_updateBuffer.push_back({objectId, worker.currentLatitude,
                                  worker.currentLongitude,
                                  worker.currentAltitudeFeet,
                                  worker.currentHeadingDegrees, frame});
    }
    if (!m_updateBuffer.empty()) {
        m_simConnect.PublishAnimationUpdates(m_updateBuffer);
    }
    if (selectionChanged) PublishStatus();
}

void AnimationThread::AddWorkerInternal(AircraftId objectId, std::string title,
                                        const AircraftSnapshot &aircraft)
{
    const std::array<std::pair<double, double>, 4> offsets = title == kWingwalkerTitle
        ? std::array<std::pair<double, double>, 4>{{{-3.0, 19.0}, {-8.0, 19.0},
                                                    {-8.0, 25.0}, {-3.0, 25.0}}}
        : std::array<std::pair<double, double>, 4>{{{0.0, 14.0}, {4.0, 14.0},
                                                    {4.0, 18.0}, {0.0, 18.0}}};
    AddWorkerInternal(objectId, std::move(title), aircraft, offsets.data(), offsets.size());
}

void AnimationThread::AddWorkerInternal(AircraftId objectId, std::string title,
                                        const AircraftSnapshot &aircraft,
                                        const RelativeWalkingPath &path)
{
    const std::array<std::pair<double, double>, 2> offsets{{
        {path.relY1, path.relX1}, {path.relY2, path.relX2}}};
    AddWorkerInternal(objectId, std::move(title), aircraft, offsets.data(), offsets.size());
}

void AnimationThread::AddWorkerInternal(AircraftId objectId, std::string title,
                                        const AircraftSnapshot &aircraft,
                                        const std::pair<double, double> *offsets,
                                        std::size_t offsetCount)
{
    if (objectId == 0 || m_workers.contains(objectId) || offsetCount < 2 ||
        offsetCount > 4) return;
    Worker worker{};
    worker.title = std::move(title);
    worker.routePointCount = offsetCount;
    worker.currentHeadingDegrees = aircraft.headingDegrees;
    for (std::size_t index = 0; index < offsetCount; ++index) {
        const auto position = RelativePosition(
            aircraft.headingDegrees, aircraft.longitude, aircraft.latitude,
            aircraft.altitudeFeet, offsets[index].first, offsets[index].second);
        worker.route[index] = {position.Latitude, position.Longitude,
                               aircraft.groundAltitudeFeet};
    }
    worker.currentLatitude = worker.route[0].latitude;
    worker.currentLongitude = worker.route[0].longitude;
    worker.currentAltitudeFeet = worker.route[0].altitudeFeet;
    for (std::size_t index = 0; index < worker.routePointCount; ++index) {
        const auto &from = worker.route[index];
        const auto &to = worker.route[(index + 1) % worker.routePointCount];
        worker.segmentLengths[index] = DistanceMeters(
            from.latitude, from.longitude, to.latitude, to.longitude);
        worker.routeLength += worker.segmentLengths[index];
    }
    if (worker.routeLength <= 0.0) {
        m_log("Could not register a zero-length animation route for ObjectID " +
              std::to_string(objectId) + ".");
        return;
    }
    const std::string workerTitle = worker.title;
    m_workers.emplace(objectId, std::move(worker));
    m_log("Registered " + workerTitle + " (ObjectID " +
          std::to_string(objectId) +
          ") as a distance-ranked walking candidate.");
}

void AnimationThread::RemoveWorkerInternal(AircraftId objectId)
{
    m_simConnect.CancelAnimationObject(objectId);
    if (m_workers.erase(objectId) != 0) PublishStatus();
    if (m_probeActive && m_probeObjectId == objectId) StopProbeInternal(false);
}

void AnimationThread::HandleSimulatorObjectRemoved(AircraftId objectId)
{
    const bool wasAnimated = m_workers.contains(objectId);
    const bool wasProbed = m_probeActive && m_probeObjectId == objectId;
    RemoveWorkerInternal(objectId);
    if (wasAnimated || wasProbed) {
        m_log("MSFS removed ObjectID " + std::to_string(objectId) +
              "; cleared its animation and probe state.");
    }
}

void AnimationThread::ResetInternal()
{
    for (const auto &[objectId, worker] : m_workers) {
        m_simConnect.CancelAnimationObject(objectId);
    }
    m_workers.clear();
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
        std::ranges::count_if(m_workers, [](const auto &entry) {
            return entry.second.walking;
        }));
    std::scoped_lock lock(m_statusMutex);
    m_status = {walkingWorkers, m_probeActive, m_probeObjectId, m_probeSamples};
}
} // namespace parking_services
