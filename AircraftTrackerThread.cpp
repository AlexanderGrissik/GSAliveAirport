#include "AircraftTrackerThread.h"

#include "SimConnectIds.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

AircraftTrackerThread::AircraftTrackerThread(ISimConnectHandler &simConnect, LogSink log)
    : m_simConnect(simConnect), m_log(std::move(log))
{
}

AircraftTrackerThread::~AircraftTrackerThread()
{
    Stop();
}

void AircraftTrackerThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&AircraftTrackerThread::AircraftTrackerLoop, this);
}

void AircraftTrackerThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_wake.notify_all();
    m_thread.join();
}

void AircraftTrackerThread::OnSimStarted()
{
    Post([this] {
        m_connected = true;
        m_scanRequest.reset();
        m_nextScan = std::chrono::steady_clock::now();
    });
}

void AircraftTrackerThread::OnSimDisconnected()
{
    OnSimStopped();
}

void AircraftTrackerThread::OnSimStopped()
{
    Post([this] {
        m_connected = false;
        m_scanRequest.reset();
        ClearState();
    });
}

void AircraftTrackerThread::OnObjRemoved(std::uint32_t objectId)
{
    Post([this, objectId] { RemoveObject(objectId); });
}

void AircraftTrackerThread::Reset()
{
    Post([this] {
        m_scanRequest.reset();
        ClearState();
        if (m_connected) m_nextScan = std::chrono::steady_clock::now() + 10s;
    });
}

void AircraftTrackerThread::FillTrackedAircraftSnapshot(
    std::vector<AircraftSnapshot> &destination) const
{
    destination.clear();
    std::scoped_lock lock(m_snapshotMutex);
    if (destination.capacity() < m_publishedTracked.size()) {
        destination.reserve(m_publishedTracked.size());
    }
    for (const auto &[objectId, aircraft] : m_publishedTracked) {
        destination.push_back(aircraft);
    }
}

void AircraftTrackerThread::FillNearbyAircraftSnapshot(
    std::vector<AircraftSnapshot> &destination, double radiusMeters) const
{
    destination.clear();
    std::scoped_lock lock(m_snapshotMutex);
    if (destination.capacity() < m_publishedNearby.size()) {
        destination.reserve(m_publishedNearby.size());
    }
    for (const auto &[objectId, aircraft] : m_publishedNearby) {
        if (aircraft.distanceFromUserMeters <= radiusMeters) {
            destination.push_back(aircraft);
        }
    }
}

bool AircraftTrackerThread::TryGetApproximateUserPosition(
    ApproximateUserPosition &destination) const
{
    std::scoped_lock lock(m_snapshotMutex);
    if (!m_publishedUserPosition) return false;
    destination = *m_publishedUserPosition;
    return true;
}

std::size_t AircraftTrackerThread::TrackedCount() const
{
    std::scoped_lock lock(m_snapshotMutex);
    return m_publishedTracked.size();
}

std::size_t AircraftTrackerThread::NearbyCount() const
{
    std::scoped_lock lock(m_snapshotMutex);
    return m_publishedNearby.size();
}

void AircraftTrackerThread::AircraftTrackerLoop(std::stop_token stopToken,
                                                 AircraftTrackerThread *self)
{
    self->RunLoop(stopToken);
}

void AircraftTrackerThread::RunLoop(std::stop_token stopToken)
{
    while (!stopToken.stop_requested()) {
        ProcessCommands();
        PollScan();
        const auto now = std::chrono::steady_clock::now();
        if (m_connected && !m_scanRequest && now >= m_nextScan) RequestScan();

        std::unique_lock lock(m_commandMutex);
        m_wake.wait_for(lock, stopToken, 100ms, [this] { return !m_commands.empty(); });
    }
    ProcessCommands();
}

void AircraftTrackerThread::Post(std::function<void()> command)
{
    {
        std::scoped_lock lock(m_commandMutex);
        m_commands.push_back(std::move(command));
    }
    m_wake.notify_all();
}

void AircraftTrackerThread::ProcessCommands()
{
    std::deque<std::function<void()>> commands;
    {
        std::scoped_lock lock(m_commandMutex);
        commands.swap(m_commands);
    }
    for (auto &command : commands) command();
}

void AircraftTrackerThread::PollScan()
{
    if (!m_scanRequest || !m_scanRequest->IsFinished()) return;
    GSAircraftScanResult result = m_scanRequest->TakeResult();
    m_scanRequest.reset();
    if (result.succeeded) ApplyScan(std::move(result.aircraft));
}

void AircraftTrackerThread::RequestScan()
{
    m_nextScan = std::chrono::steady_clock::now() + 10s;
    m_scanRequest = std::make_shared<GSReqAircraftScan>();
    m_simConnect.RequestObjectDataByType(
        DefinitionAircraft, static_cast<DWORD>(RetentionRadiusMeters),
        SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT, m_scanRequest);
}

void AircraftTrackerThread::ApplyScan(std::vector<AircraftSnapshot> observations)
{
    const auto user = std::ranges::find_if(
        observations, [](const AircraftSnapshot &aircraft) { return aircraft.isUser; });
    if (user == observations.end()) {
        m_log("Aircraft scan did not contain the user aircraft; retained the previous snapshot.");
        return;
    }

    m_userPosition = ApproximateUserPosition{
        user->objectId, user->latitude, user->longitude};

    const auto now = TrackerClock::now();
    std::map<AircraftId, AircraftSnapshot> nextNearby;
    std::set<AircraftId> seenTracked;
    std::size_t added = 0;
    for (AircraftSnapshot observation : observations) {
        observation.distanceFromUserMeters = DistanceMeters(
            user->latitude, user->longitude, observation.latitude, observation.longitude);
        if (observation.isUser || observation.distanceFromUserMeters > RetentionRadiusMeters) {
            continue;
        }

        const auto previous = m_nearby.find(observation.objectId);
        observation.firstSeen = previous == m_nearby.end() ? now : previous->second.firstSeen;
        const bool parked = observation.onGround &&
                            std::abs(observation.groundSpeedKnots) < 1.0;
        if (parked) {
            observation.parkedSince = previous == m_nearby.end()
                ? std::optional{now}
                : previous->second.parkedSince.value_or(now);
        }
        observation.lastSeen = now;

        const auto tracked = m_tracked.find(observation.objectId);
        if (tracked != m_tracked.end()) {
            observation.firstTracked = tracked->second.firstTracked;
            tracked->second = observation;
            seenTracked.insert(observation.objectId);
        } else if (observation.distanceFromUserMeters <= DiscoveryRadiusMeters) {
            observation.firstTracked = now;
            m_tracked.emplace(observation.objectId, observation);
            seenTracked.insert(observation.objectId);
            ++added;
        }
        nextNearby.emplace(observation.objectId, std::move(observation));
    }

    std::size_t removed = 0;
    for (auto tracked = m_tracked.begin(); tracked != m_tracked.end();) {
        if (!seenTracked.contains(tracked->first)) {
            tracked = m_tracked.erase(tracked);
            ++removed;
        } else {
            ++tracked;
        }
    }
    m_nearby = std::move(nextNearby);
    PublishSnapshot();
    if (added != 0 || removed != 0) {
        m_log("Tracker update: added " + std::to_string(added) + ", removed " +
              std::to_string(removed) + ", total " + std::to_string(m_tracked.size()) + ".");
    }
}

void AircraftTrackerThread::RemoveObject(AircraftId objectId)
{
    const bool removed = m_tracked.erase(objectId) != 0;
    const bool observed = m_nearby.erase(objectId) != 0;
    const bool userRemoved = m_userPosition && m_userPosition->objectId == objectId;
    if (userRemoved) m_userPosition.reset();
    if (removed || observed || userRemoved) {
        PublishSnapshot();
        if (userRemoved) {
            m_log("MSFS removed the user-aircraft ObjectID; invalidated its approximate position.");
        } else {
            m_log("MSFS removed aircraft ObjectID " + std::to_string(objectId) +
                  (removed ? "; evicted it from the tracked set."
                           : "; removed it from the nearby snapshot."));
        }
    }
}

void AircraftTrackerThread::ClearState()
{
    m_tracked.clear();
    m_nearby.clear();
    m_userPosition.reset();
    PublishSnapshot();
}

void AircraftTrackerThread::PublishSnapshot()
{
    std::scoped_lock lock(m_snapshotMutex);
    m_publishedTracked = m_tracked;
    m_publishedNearby = m_nearby;
    m_publishedUserPosition = m_userPosition;
}
} // namespace parking_services
