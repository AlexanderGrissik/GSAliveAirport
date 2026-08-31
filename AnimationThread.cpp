#include "AnimationThread.h"

#include "SimConnectThread.h"
#include "SimObjectPositioning.h"

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
constexpr std::string_view kMarshallerTitle = "FSDT_Marshaller_01";
}

AnimationThread::AnimationThread(SimConnectThread &simConnect, LogSink log)
    : m_simConnect(simConnect), m_log(std::move(log))
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
    std::vector<AnimationUpdate> updates;
    updates.reserve(m_workers.size());
    for (const auto &[objectId, worker] : m_workers) {
        const bool marshaller = worker.title == kMarshallerTitle;
        const double transitionStart = marshaller ? 1445.0 : 192.0;
        const double transitionEnd = marshaller ? 1530.0 : 229.0;
        const double loopStart = marshaller ? 1530.0 : 230.0;
        const double loopEnd = marshaller ? 1572.0 : 268.0;
        const double elapsed = std::chrono::duration<double>(now - worker.started).count();

        std::array<double, 4> lengths{};
        double routeLength = 0.0;
        for (std::size_t index = 0; index < worker.route.size(); ++index) {
            const auto &from = worker.route[index];
            const auto &to = worker.route[(index + 1) % worker.route.size()];
            lengths[index] = DistanceMeters(from.latitude, from.longitude,
                                             to.latitude, to.longitude);
            routeLength += lengths[index];
        }
        if (routeLength <= 0.0) continue;

        double routeDistance = std::fmod(elapsed * kWalkingSpeedMetersPerSecond,
                                         routeLength);
        std::size_t segment = 0;
        while (segment + 1 < lengths.size() && routeDistance > lengths[segment]) {
            routeDistance -= lengths[segment++];
        }
        const auto &from = worker.route[segment];
        const auto &to = worker.route[(segment + 1) % worker.route.size()];
        const double fraction = lengths[segment] > 0.0
                                    ? routeDistance / lengths[segment]
                                    : 0.0;
        const double meanLatitude = (from.latitude + to.latitude) * 0.5 *
                                    3.14159265358979323846 / 180.0;
        const double north = to.latitude - from.latitude;
        const double east = (to.longitude - from.longitude) * std::cos(meanLatitude);
        const double heading = std::fmod(std::atan2(east, north) * 180.0 /
                                             3.14159265358979323846 + 360.0,
                                         360.0);
        const double transitionDuration =
            (transitionEnd - transitionStart) / kAnimationFramesPerSecond;
        const double frame = elapsed < transitionDuration
            ? transitionStart + elapsed * kAnimationFramesPerSecond
            : loopStart + std::fmod((elapsed - transitionDuration) *
                                        kAnimationFramesPerSecond,
                                    loopEnd - loopStart);
        updates.push_back({objectId,
                           from.latitude + (to.latitude - from.latitude) * fraction,
                           from.longitude + (to.longitude - from.longitude) * fraction,
                           from.altitudeFeet + (to.altitudeFeet - from.altitudeFeet) * fraction,
                           heading, frame});
    }
    if (!updates.empty()) m_simConnect.PublishAnimationUpdates(std::move(updates));
}

void AnimationThread::AddWorkerInternal(AircraftId objectId, std::string title,
                                        const AircraftSnapshot &aircraft)
{
    if (objectId == 0 || m_workers.contains(objectId)) return;
    if (m_workers.size() >= MaximumWorkers) {
        m_log("Animation worker limit (" + std::to_string(MaximumWorkers) +
              ") reached; ObjectID " + std::to_string(objectId) + " will remain standing.");
        return;
    }
    const std::array<std::pair<double, double>, 4> offsets = title == kWingwalkerTitle
        ? std::array<std::pair<double, double>, 4>{{{-3.0, 19.0}, {-8.0, 19.0},
                                                    {-8.0, 25.0}, {-3.0, 25.0}}}
        : std::array<std::pair<double, double>, 4>{{{0.0, 14.0}, {4.0, 14.0},
                                                    {4.0, 18.0}, {0.0, 18.0}}};
    Worker worker{};
    worker.title = std::move(title);
    worker.started = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const auto position = RelativePosition(
            aircraft.headingDegrees, aircraft.longitude, aircraft.latitude,
            aircraft.altitudeFeet, offsets[index].first, offsets[index].second);
        worker.route[index] = {position.Latitude, position.Longitude,
                               aircraft.groundAltitudeFeet};
    }
    m_simConnect.FreezeObject(objectId);
    const std::string workerTitle = worker.title;
    m_workers.emplace(objectId, std::move(worker));
    PublishStatus();
    m_log("Started 30 Hz direct walking animation for " + workerTitle +
          " (ObjectID " + std::to_string(objectId) + ").");
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
    std::scoped_lock lock(m_statusMutex);
    m_status = {m_workers.size(), m_probeActive, m_probeObjectId, m_probeSamples};
}
} // namespace parking_services
