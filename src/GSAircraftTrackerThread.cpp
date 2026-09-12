#include "GSAircraftTrackerThread.h"
#include "simobj/GSAircraft.h"
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
        RunDispatch(stopToken);

        if (IsSimStarted() && !m_scanInProgress && (std::chrono::steady_clock::now() - m_lastScanTime) > 4s) {
            RequestScan();
        } else if (!IsLastLoopMessage()) {
            if (!m_scanInProgress)
                std::this_thread::sleep_for(1000ms);
            else
                std::this_thread::sleep_for(50ms);
        }
    }

    OnDisconnect();
    Disconnect();
}

void GSAircraftTrackerThread::OnConnect()
{
    GSAircraft::InitDatums(*this);
    m_lastScanTime = std::chrono::steady_clock::now() - 10s;
}

void GSAircraftTrackerThread::OnDisconnect()
{
    OnSimStop();
    m_scanInProgress = false;
}

void GSAircraftTrackerThread::OnSimStop()
{
    std::for_each(m_tracked.begin(), m_tracked.end(), [this](auto& aircraft) { 
        CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_REMOVED, aircraft.second));
        m_singleObserver.PostCommand(cmd);        
    }); 

    m_tracked.clear();
}

void GSAircraftTrackerThread::RequestScan()
{
    PostReqCommand(new GSReqScan(*this));
    m_scanInProgress = true;
    m_lastScanTime = std::chrono::steady_clock::now();
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
        CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_REMOVED, node.mapped()));
        m_singleObserver.PostCommand(cmd);
    }); 

    m_lastScanIDs.clear();
}

void GSAircraftTrackerThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GSDefinitions::CMD_PRINT_AIRCRAFT_ALL: {
        PrintAircrafts(false, 9999.0);
        CmdPtr ret(new GSCommand(cmd));
        cmd.Finish(ret);
        break;
    }
    case GSDefinitions::CMD_PRINT_AIRCRAFT_1KM: {
        PrintAircrafts(false, 1.0);
        CmdPtr ret(new GSCommand(cmd));
        cmd.Finish(ret);
        break;
    }
    case GSDefinitions::CMD_PRINT_AIRCRAFT_PARKED: {
        PrintAircrafts(true, 9999.0);
        CmdPtr ret(new GSCommand(cmd));
        cmd.Finish(ret);
        break;
    }
    default:
        GSLogStream::LogError("GSAircraftTrackerThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSAircraftTrackerThread::PrintAircrafts(bool parkedOnly, double distKM) const
{
    GSLogStream::Log() << "=== PrintAircrafts, ParkedOnly =" << (parkedOnly ? "Y" : "N")
            << " MaxDistKm =" << distKM << " Total =" << m_tracked.size() << " ===";

    const GSAircraft *user = nullptr;
    for (const auto &pair : m_tracked) {
        if (pair.second && pair.second->IsUser()) {
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
        if (ac.IsUser() || (parkedOnly && !ac.IsParkedActive())) {
           ++skipped;
            continue;
        }

        if (user && (distKM < (user->LateralDistanceMetersFrom(ac) / 1000.0))) {
            ++skipped;
            continue;
        }

        ac.Print();

        ++printed;
    }

    GSLogStream::Log() << "=== end: printed=" << printed << " skipped=" << skipped << " ===";
}

GSRequest::SendResult GSAircraftTrackerThread::GSReqScan::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestDataOnSimObjectType, id, GSDefinitions::GSDefID_Aircraft, DiscoveryRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT), true };
    
    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqScan::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    } 

    return rc;
}

void GSAircraftTrackerThread::HandleAircraftUser(std::shared_ptr<GSAircraft>& aircraft)
{
    CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_USER, aircraft));
    m_singleObserver.PostCommand(cmd);
}

bool GSAircraftTrackerThread::HandleExistingAircraft(std::shared_ptr<GSAircraft> &aircraft, std::shared_ptr<GSAircraft> &exisitng)
{
    if (!aircraft->GetRawData().onGround) 
        return false;

    if (aircraft != exisitng) {
        exisitng->CopyDynInfo(*aircraft);
        CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_MODIFIED, exisitng));
        m_singleObserver.PostCommand(cmd);
    }
    return true;
}

bool GSAircraftTrackerThread::HandleNewAircraft(std::shared_ptr<GSAircraft> &aircraft)
{
    if (!aircraft->GetRawData().onGround) 
        return false;

    m_tracked.emplace(aircraft->GetObjID(), aircraft);
    CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_ADDED, aircraft));
    m_singleObserver.PostCommand(cmd);
    return true;
}

bool GSAircraftTrackerThread::HandleScanMessage(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    if (entry.dwoutof == 0) { // Empty Scan
        m_scanInProgress = false;
        return true;
    }

    std::shared_ptr<GSAircraft> aircraft = std::make_shared<GSAircraft>();
    aircraft->LoadDynamicState(entry);

    if (aircraft->IsUser()) {
        HandleAircraftUser(aircraft);
    } else {
        bool rc = true;
        auto itr = m_tracked.find(entry.dwObjectID);
        if (itr != m_tracked.end()) { // Already tracking
            rc &= HandleExistingAircraft(aircraft, itr->second);
        } else {
            aircraft->LoadFullState(entry);
            rc &= HandleNewAircraft(aircraft);
        }

        if (rc)
            m_lastScanIDs.emplace(entry.dwObjectID);
    }

    if (entry.dwoutof == entry.dwentrynumber) {
        HandleRemoved();
        m_scanInProgress = false;
        return true;
    }

    return false;
}

bool GSAircraftTrackerThread::GSReqScan::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE) {
        GSLogStream::LogError("Unexpected message: ") << message->dwID << ", Size: " << messageSize;
        return false;
    }

    return tracker.HandleScanMessage(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message));
}

void GSAircraftTrackerThread::GSReqScan::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    GSLogStream::LogError("GSAircraftTrackerThread::GSReqScan:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
    
    tracker.SetInProgress(false);
    tracker.HandleRemoved();
}

} // namespace NS_GSLiveAirportMSFS
