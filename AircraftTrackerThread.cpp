#include "AircraftTrackerThread.h"

#include "GSCommon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr SIMCONNECT_DATA_DEFINITION_ID kAircraftDefinition = 1;
constexpr std::size_t kInteractivePointProbeCount = 32;

struct DatumSpec
{
    const char *name;
    const char *units;
    SIMCONNECT_DATATYPE type;
};

constexpr std::array kAircraftDatums{
    DatumSpec{"TITLE", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"ATC ID", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"ATC AIRLINE", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"ATC FLIGHT NUMBER", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"PLANE LATITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"PLANE LONGITUDE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"PLANE ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"GROUND ALTITUDE", "feet", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"PLANE HEADING DEGREES TRUE", "degrees", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"GROUND VELOCITY", "knots", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"WING SPAN", "meters", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"SIM ON GROUND", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"IS USER SIM", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"AI TRAFFIC CURRENT AIRPORT", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"AI TRAFFIC ASSIGNED PARKING", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"AI TRAFFIC ASSIGNED RUNWAY", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"AI TRAFFIC FROMAIRPORT", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"AI TRAFFIC TOAIRPORT", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"AI TRAFFIC ETD", "seconds", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"AI TRAFFIC ETA", "seconds", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"AI TRAFFIC STATE", "", SIMCONNECT_DATATYPE_STRING256},
    DatumSpec{"AI TRAFFIC ISIFR", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"NUMBER OF ENGINES", "number", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG COMBUSTION:1", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG COMBUSTION:2", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG COMBUSTION:3", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG COMBUSTION:4", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG STARTER ACTIVE:1", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG STARTER ACTIVE:2", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG STARTER ACTIVE:3", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"GENERAL ENG STARTER ACTIVE:4", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"TURB ENG N1:1", "percent", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"TURB ENG N1:2", "percent", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"TURB ENG N1:3", "percent", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"TURB ENG N1:4", "percent", SIMCONNECT_DATATYPE_FLOAT64},
    DatumSpec{"LIGHT BEACON", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"LIGHT NAV", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"LIGHT TAXI", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"LIGHT STROBE", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"BRAKE PARKING POSITION", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"PUSHBACK ATTACHED", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"PUSHBACK WAIT", "bool", SIMCONNECT_DATATYPE_INT32},
    DatumSpec{"TRANSPONDER STATE:1", "enum", SIMCONNECT_DATATYPE_INT32},
};
} // namespace

AircraftTrackerThread::AircraftTrackerThread(ISimConnectHandler &simConnect)
    : m_simConnect(simConnect)
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

void AircraftTrackerThread::OnSimConnected()
{
    Post([this] { InitializeSimConnect(); });
}

void AircraftTrackerThread::OnSimStarted()
{
    Post([this] {
        m_connected = true;
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
        m_discardScanResult = true;
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
        m_discardScanResult = true;
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

void AircraftTrackerThread::InitializeSimConnect()
{
    CollectFinishedSetupRequests();
    for (const DatumSpec &datum : kAircraftDatums) {
        m_simConnect.AddDatum(kAircraftDefinition, datum.name, datum.units,
                              datum.type, NewSetupRequest());
    }
    for (std::size_t index = 0; index < kInteractivePointProbeCount; ++index) {
        const std::string suffix = ":" + std::to_string(index);
        m_simConnect.AddDatum(kAircraftDefinition,
                              "INTERACTIVE POINT TYPE EX1" + suffix, "enum",
                              SIMCONNECT_DATATYPE_INT32, NewSetupRequest());
        m_simConnect.AddDatum(kAircraftDefinition,
                              "INTERACTIVE POINT POSX EX1" + suffix, "feet",
                              SIMCONNECT_DATATYPE_FLOAT64, NewSetupRequest());
        m_simConnect.AddDatum(kAircraftDefinition,
                              "INTERACTIVE POINT POSY EX1" + suffix, "feet",
                              SIMCONNECT_DATATYPE_FLOAT64, NewSetupRequest());
        m_simConnect.AddDatum(kAircraftDefinition,
                              "INTERACTIVE POINT POSZ EX1" + suffix, "feet",
                              SIMCONNECT_DATATYPE_FLOAT64, NewSetupRequest());
        m_simConnect.AddDatum(kAircraftDefinition,
                              "INTERACTIVE POINT HEADING EX1" + suffix, "degrees",
                              SIMCONNECT_DATATYPE_FLOAT64, NewSetupRequest());
    }
}

GSReqCommand &AircraftTrackerThread::NewSetupRequest()
{
    auto request = std::make_unique<GSReqCommand>();
    GSReqCommand &reference = *request;
    m_setupRequests.push_back(std::move(request));
    return reference;
}

void AircraftTrackerThread::CollectFinishedSetupRequests()
{
    std::erase_if(m_setupRequests, [](const auto &request) {
        return request->IsFinished();
    });
}

void AircraftTrackerThread::PollScan()
{
    CollectFinishedSetupRequests();
    if (!m_scanRequest || !m_scanRequest->IsFinished()) return;
    GSAircraftScanResult result = m_scanRequest->TakeResult();
    m_scanRequest.reset();
    const bool discard = std::exchange(m_discardScanResult, false);
    if (result.succeeded && !discard) ApplyScan(std::move(result.aircraft));
}

void AircraftTrackerThread::RequestScan()
{
    m_nextScan = std::chrono::steady_clock::now() + 10s;
    m_scanRequest = std::make_unique<GSReqAircraftScan>();
    m_simConnect.RequestObjectDataByType(
        kAircraftDefinition, static_cast<DWORD>(RetentionRadiusMeters),
        SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT, *m_scanRequest);
}

void AircraftTrackerThread::ApplyScan(std::vector<AircraftSnapshot> observations)
{
    const auto user = std::ranges::find_if(
        observations, [](const AircraftSnapshot &aircraft) { return aircraft.isUser; });
    if (user == observations.end()) {
        GSLog("Aircraft scan did not contain the user aircraft; retained the previous snapshot.");
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
        GSLog("Tracker update: added " + std::to_string(added) + ", removed " +
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
            GSLog("MSFS removed the user-aircraft ObjectID; invalidated its approximate position.");
        } else {
            GSLog("MSFS removed aircraft ObjectID " + std::to_string(objectId) +
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
