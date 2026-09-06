#include "GSAircraftTrackerThread.h"
#include "GSAircraft.h"
#include "GSDefinitions.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <utility>
#include <algorithm>

namespace parking_services
{

GSAircraftTrackerThread::~GSAircraftTrackerThread()
{
}

void AircraftTrackerThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&GSAircraftTrackerThread::AircraftTrackerLoop, this);
}

void AircraftTrackerThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_thread.join();
}

void AircraftTrackerThread::AircraftTrackerLoop(std::stop_token stopToken,
                                                 AircraftTrackerThread *self)
{
    self->RunLoopTracker(stopToken);
}

void AircraftTrackerThread::RunLoopTracker(std::stop_token stopToken)
{
    while (!stopToken.stop_requested()) {
        m_lastLoopMsg = false;
        RunDispatch();

        if (m_simStarted && !m_scanInProgress) {
            if (RequestScan()) {
                m_scanInProgress = true;
            } 
        } else if (!m_scanInProgress) {
            std::this_thread::sleep_for(1000ms);
        } else if (!m_lastLoopMsg) {
            std::this_thread::sleep_for(50ms);
        } 
    }

    OnDisconnect();
    Disconnect();
}

void GSAircraftTrackerThread::OnConnect()
{
    GSAircraft::InitDatums(*this);
}

void GSAircraftTrackerThread::OnDisconnect()
{
    OnSimStop();
    m_scanInProgress = false;
}

void GSAircraftTrackerThread::OnSimStart()
{
    m_simStarted = true;
}

void GSAircraftTrackerThread::OnSimStop()
{
    m_simStarted = false;

    m_nearby.clear();

    std::for_each(m_subscribers.begin(), m_subscribers.end(), [m_tracked&](auto* tracker) { 
        std::for_each(m_tracked.begin(), m_tracked.end(), [tracker&](auto& aircraft) { tracker->OnAircraftRemoved(aircraft); }); 
    });

    m_tracked.clear();
}

bool AircraftTrackerThread::RequestScan()
{
    DOWRD reqId = NextRequestId();
    m_rc = InvokeRequest(reqId, SimConnect_RequestDataOnSimObjectType, reqId, GSDefinitions::GSDefID_Aircraft, DiscoveryRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT);
    if (!m_rc.isOK()) {
        GSLogError("Unable to request aircraft scan: ") << m_rc.rc << std::endl;
    }
    return m_rc.isOK();
}

void GSAircraftTrackerThread::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    if (message->dwSendID == m_rc.sendID) {
        GSLogError("Request Exception: ") << message->dwID << ", Size: " << messageSize << 
            "Exception: " << message->dwException << "Index:" << message->dwIndex << std::endl;
    } else {
        GSLogError("Unknown Exception: ") << message->dwID << ", Size: " << messageSize << 
            "Exception: " << message->dwException << "Index:" << message->dwIndex << std::endl;
    }

    m_scanInProgress = false;
    HandleRemoved();
}

void GSAircraftTrackerThread::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    m_lastLoopMsg = true;
    if (!m_simStarted) {
        return;
    }

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE) {
        GSLogError("Unexpected message: ") << message->dwID << ", Size: " << messageSize << std::endl;
        return;
    }

    const auto &entry = *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message);

    if (entry.dwoutof == 0) { // Empty Scan
        m_scanInProgress = false;
        return;
    }

    shared_ptr<GSAircraft> aircraft = std::make_shared<GSAircraft>();


    if (entry.dwObjectID == SIMCONNECT_OBJECT_ID_USER) {
        aircraft->LoadDynamicState(entry);
    } else {
        auto itr = m_tracked.find(entry.dwObjectID);
        if (itr != m_tracked.end()) { // Already tracking
            aircraft->LoadDynamicState(entry);
            HandleExistingAircraft(aircraft, itr->second);
        } else {
            aircraft->LoadFullState(entry);
            HandleNewAircraft(aircraft);
        }

        m_lastScanIDs.emplace(entry.dwObjectID);
    }
    if (entry.dwoutof == entry.dwentrynumber) {
        HandleRemoved();
        m_scanInProgress = false;
    }
}

void GSAircraftTrackerThread::HandleExistingAircraft(shared_ptr<GSAircraft> &aircraft, shared_ptr<GSAircraft> &exisitng)
{
    if (aircraft != exisitng) {
        exisitng->CopyDynInfo(aircraft);
        std::for_each(m_subscribers.begin(), m_subscribers.end(), [exisitng](auto* tracker) { tracker->OnAircraftModified(exisitng); });
    }
}

void GSAircraftTrackerThread::HandleNewAircraft(shared_ptr<GSAircraft> &aircraft)
{
    m_tracked.emplace(aircraft);
    std::for_each(m_subscribers.begin(), m_subscribers.end(), [aircraft&](auto* tracker) { tracker->OnAircraftAdded(aircraft); });
}

void GSAircraftTrackerThread::HandleRemoved()
{
    std::vector<DWORD> toRemove;
    toRemove.reserve(m_tracked.szie() / 2);

    std::for_each(m_tracked.begin(), m_tracked.end(), [this, toRemove](auto& aircraft) { 
        if (!m_lastScanIDs.contains(aircraft->objectID))
            toRemove.emplace_back(aircraft->objectID);
    });

    std::for_each(toRemove.begin(), toRemove.end(), [this](DWORD id) {
        auto node = m_tracked.extract(id);
        std::for_each(m_subscribers.begin(), m_subscribers.end(), [node&](auto* tracker) { tracker->OnAircraftRemoved(node.mapped()); });
    }); 

    m_lastScanIDs.clear();
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
        const bool parked = observation.onGround &&
                            std::abs(observation.groundSpeedKnots) < 1.0;
        if (parked) {
            observation.parkedSince = previous == m_nearby.end()
                ? std::optional{now}
                : previous->second.parkedSince.value_or(now);
        }

        const auto tracked = m_tracked.find(observation.objectId);
        if (tracked != m_tracked.end()) {
            tracked->second = observation;
            seenTracked.insert(observation.objectId);
        } else if (observation.distanceFromUserMeters <= DiscoveryRadiusMeters) {
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

} // namespace parking_services
