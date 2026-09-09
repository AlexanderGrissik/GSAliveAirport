#include "GSAircraftTrackerThread.h"
#include "GSAircraft.h"
#include "GSDefinitions.h"
#include "GSGeography.h"
#include "GSLogStream.h"
#include "cmds/GSCmdAircraftUpdate.h"
#include "GSSpawnerThread.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <set>
#include <string>
#include <utility>

namespace NS_GSLiveAirportMSFS
{
using namespace std::chrono_literals;

void GSAircraftTrackerThread::Start()
{
    if (m_thread.joinable()) return;
    m_thread = std::jthread(&GSAircraftTrackerThread::AircraftTrackerLoop, this);
}

void GSAircraftTrackerThread::Stop()
{
    if (!m_thread.joinable()) return;
    m_thread.request_stop();
    m_thread.join();
}

void GSAircraftTrackerThread::AircraftTrackerLoop(std::stop_token stopToken,
                                                  GSAircraftTrackerThread *self)
{
    self->RunLoopTracker(stopToken);
}

void GSAircraftTrackerThread::RunLoopTracker(std::stop_token stopToken)
{
    while (!stopToken.stop_requested()) {
        m_lastLoopMsg = false;
        RunDispatch(stopToken);
        RunCommands();

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

    std::for_each(m_tracked.begin(), m_tracked.end(), [this](auto& aircraft) { 
        std::unique_ptr<GSCommand> cmd(new GSCmdAircraftUpdate(GSSpawnerThread::CMD_SPAWNER_AIRCRAFT_REMOVED, aircraft.second));
        m_singleObserver.PostCommand(cmd);        
    }); 

    m_tracked.clear();
}

bool GSAircraftTrackerThread::RequestScan()
{
    DWORD reqId = NextRequestID();
    m_rc = InvokeRequest(reqId, SimConnect_RequestDataOnSimObjectType, reqId, GSDefinitions::GSDefID_Aircraft, DiscoveryRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT);
    if (!m_rc.isOK()) {
        GSLogStream::LogError("Unable to request aircraft scan: ") << m_rc.rc << std::endl;
    }
    return m_rc.isOK();
}

void GSAircraftTrackerThread::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    if (message->dwSendID == m_rc.sendID) {
        GSLogStream::LogError("Request Exception: ") << message->dwID <<
            ", Exception: " << message->dwException << "Index:" << message->dwIndex << std::endl;
    } else {
        GSLogStream::LogError("Unknown Exception: ") << message->dwID <<
            ", Exception: " << message->dwException << "Index:" << message->dwIndex << std::endl;
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
        GSLogStream::LogError("Unexpected message: ") << message->dwID << ", Size: " << messageSize << std::endl;
        return;
    }

    const auto &entry = *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message);

    if (entry.dwoutof == 0) { // Empty Scan
        m_scanInProgress = false;
        return;
    }

    std::shared_ptr<GSAircraft> aircraft = std::make_shared<GSAircraft>();

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

void GSAircraftTrackerThread::HandleExistingAircraft(std::shared_ptr<GSAircraft> &aircraft, std::shared_ptr<GSAircraft> &exisitng)
{
    if (aircraft != exisitng) {
        exisitng->CopyDynInfo(*aircraft);
        std::unique_ptr<GSCommand> cmd(new GSCmdAircraftUpdate(GSSpawnerThread::CMD_SPAWNER_AIRCRAFT_MODIFIED, exisitng));
        m_singleObserver.PostCommand(cmd);
    }
}

void GSAircraftTrackerThread::HandleNewAircraft(std::shared_ptr<GSAircraft> &aircraft)
{
    m_tracked.emplace(aircraft->objectID, aircraft);
    std::unique_ptr<GSCommand> cmd(new GSCmdAircraftUpdate(GSSpawnerThread::CMD_SPAWNER_AIRCRAFT_ADDED, aircraft));
    m_singleObserver.PostCommand(cmd);
}

void GSAircraftTrackerThread::HandleRemoved()
{
    std::vector<DWORD> toRemove;
    toRemove.reserve(m_tracked.size() / 2);

    std::for_each(m_tracked.begin(), m_tracked.end(), [this, &toRemove](auto& pair) { 
        if (!m_lastScanIDs.contains(pair.first))
            toRemove.emplace_back(pair.first);
    });

    std::for_each(toRemove.begin(), toRemove.end(), [this](DWORD id) {
        auto node = m_tracked.extract(id);
        //std::for_each(m_subscribers.begin(), m_subscribers.end(), [node&](auto* tracker) { tracker->OnAircraftRemoved(node.mapped()); });
    }); 

    m_lastScanIDs.clear();
}

bool GSAircraftTrackerThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case CMD_PRINT_AIRCRAFT_ALL:
        PrintAircrafts(false, 9999.0);
        break;
    case CMD_PRINT_AIRCRAFT_1KM:
        PrintAircrafts(false, 1.0);
        break;
    case CMD_PRINT_AIRCRAFT_PARKED:
        PrintAircrafts(true, 9999.0);
        break;
    default:
        GSLogStream::LogError("GSAircraftTrackerThread - Unexpected cmd: ") << cmd.GetCmdID() << std::endl;
        break;
    }

    return true;
}

void GSAircraftTrackerThread::PrintAircrafts(bool parkedOnly, double distKM) const
{
    GSLogStream::Log() << "=== PrintAircrafts, ParkedOnly =" << (parkedOnly ? "Y" : "N")
            << " MaxDistKm =" << distKM << " Total =" << m_tracked.size() << " ===";

    const GSAircraft *user = nullptr;
    for (const auto &pair : m_tracked) {
        if (pair.second && pair.second->m_rawData.isUser != 0) {
            user = pair.second.get();
            break;
        }
    }

    if (!user) {
        GSLogStream::Log() << "no user aircraft found; distance filter disabled";
    }

    int printed = 0;
    int skipped = 0;

    for (const auto &pair : m_tracked) {
        const GSAircraft &ac = *pair.second;
        if (ac.m_rawData.isUser || (parkedOnly && !ac.IsParkedActive())) {
           ++skipped;
            continue;
        }

        if (user && (distKM < GSGeography::DistanceMeters(user->m_rawData.latitude, user->m_rawData.longitude, ac.m_rawData.latitude, ac.m_rawData.longitude) / 1000.0)) {
            ++skipped;
            continue;
        }

        ac.Print();

        ++printed;
    }

    GSLogStream::Log() << "=== end: printed=" << printed << " skipped=" << skipped << " ===";
}

} // namespace NS_GSLiveAirportMSFS
