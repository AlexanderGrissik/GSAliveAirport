#include "GSAircraftTrackerThread.h"
#include "../SimObjects/GSAircraft.h"
#include "GSDefinitions.h"
#include "GSGeography.h"
#include "GSLogStream.h"
#include "../Commands/GSCmdAircraftUpdate.h"
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

void GSAircraftTrackerThread::AircraftTrackerLoop(std::stop_token stopToken, GSAircraftTrackerThread *self)
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
    GSAirport::InitDatums(*this);
    GSAircraft::InitDatums(*this);
    GSSimObj::InitDatums(*this);
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

void GSAircraftTrackerThread::RequestAirportScan()
{
    m_tempClosest.reset();
    m_tempClosestDistance = std::numeric_limits<double>::max();
    PostReqCommand(new GSReqScanAirport(*this));
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
    case GSDefinitions::CMD_SPAWN_TEST_AIRCRAFT: {
        SpawnTestAircrafts();
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

void GSAircraftTrackerThread::SpawnTestAircrafts()
{
    PostReqCommand(new GSReqSpawnAircraft(*this, 1));
    PostReqCommand(new GSReqSpawnAircraft(*this, 2));
    PostReqCommand(new GSReqSpawnAircraft(*this, 3));
    PostReqCommand(new GSReqSpawnAircraft(*this, 4));
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
    m_userAircraft = aircraft;
    if (!m_currAirport || GSGeography::DistanceMeters(m_currAirport->GetLongLat(), m_userAircraft->GetLongLat()) > MinAirportRefreshDistMtr) {
        RequestAirportScan();
    }

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

bool GSAircraftTrackerThread::HandleScanAirportMessage(SIMCONNECT_RECV_AIRPORT_LIST &entry)
{
    for (DWORD i = 0; i < entry.dwArraySize; ++i) {
        const auto& airport = entry.rgData[i];
        const GSCoord airportPos{ airport.Longitude, airport.Latitude };
        const double distance = GSGeography::DistanceMeters(m_userAircraft->GetLongLat(), airportPos);
        if (distance < m_tempClosestDistance) {
            m_tempClosestDistance = distance;
            m_tempClosest.reset(new SIMCONNECT_DATA_FACILITY_AIRPORT(airport));
        }
    }

    if (entry.dwEntryNumber + 1 == entry.dwOutOf) {
        if (m_tempClosest.get() && m_tempClosestDistance < DiscoveryRadiusMeters) {
            m_currAirport = std::make_shared<GSAirport>(m_tempClosest->Ident, GSCoord{ m_tempClosest->Longitude, m_tempClosest->Latitude });
            m_currAirport->LoadInfo(*this);
        }

        return true;
    }

    return false;
}

bool GSAircraftTrackerThread::GSReqScan::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
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

GSRequest::SendResult GSAircraftTrackerThread::GSReqScanAirport::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestFacilitiesList, SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, id), true };
    
    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqScanAirport::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    } 

    return rc;
}

bool GSAircraftTrackerThread::GSReqScanAirport::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) 
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_AIRPORT_LIST) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        return false;
    }

    return tracker.HandleScanAirportMessage(*reinterpret_cast<SIMCONNECT_RECV_AIRPORT_LIST *>(message));
}
    
void GSAircraftTrackerThread::GSReqScanAirport::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSLogStream::LogError("GSAircraftTrackerThread::GSReqScanAirport:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

GSRequest::SendResult GSAircraftTrackerThread::GSReqSpawnAircraft::Process()
{
    const auto userAircraft = static_cast<GSAircraftTrackerThread&>(m_simHandle).GetUserAircraft();
    if (!userAircraft) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqSpawnAircraft::Process No User Aircrat");
        return { {0,0,E_FAIL}, false };
    }
    const auto posUser = userAircraft->GetLongLat();
       
    m_pos.Altitude = userAircraft->GetRawData().altitudeFeet;
    m_pos.Heading = 0.0;
    m_pos.Pitch = 0;
    m_pos.Bank = 0;
    m_pos.OnGround = 1;
    m_pos.Airspeed = 0;

    std::string title = "747-8i";
    m_pos.Latitude = posUser.Lat() - 0.001;
    m_pos.Longitude = posUser.Long() - 0.001;

    if (m_type == 2) {
        title = "737 Max 8 Passengers";
        m_pos.Latitude = posUser.Lat() - 0.001;
        m_pos.Longitude = posUser.Long() + 0.001;
    } else if (m_type == 3) {
        title = "Cessna C152";
        m_pos.Latitude = posUser.Lat() + 0.001;
        m_pos.Longitude = posUser.Long() + 0.001;
    } else if (m_type == 4) {
        title = "A330-300 (RR)";
        m_pos.Latitude = posUser.Lat() + 0.001;
        m_pos.Longitude = posUser.Long() - 0.001;
    }

    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_AICreateSimulatedObject_EX1, title.c_str(), "", m_pos, id), true};

    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqSpawnAircraft::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    }

    return rc;
}

bool GSAircraftTrackerThread::GSReqSpawnAircraft::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    if (message->dwID == SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID) {
        auto* msg = static_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID*>(message);
        
        auto requestId = m_simHandle.NextRequestID();
        auto aiRC = m_simHandle.InvokeRequest(requestId, SimConnect_AIReleaseControl, msg->dwObjectID, requestId);

        std::this_thread::sleep_for(300ms);

        auto simRC1 = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSDefinitions::GSDefID_Freeze_LongLat, 1,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        auto simRC2 = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSDefinitions::GSDefID_Freeze_Altitude, 1,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        auto simRC = m_simHandle.Invoke(SimConnect_SetDataOnSimObject, GSDefinitions::GSDefID::GSDefID_Position, msg->dwObjectID, 0, 1,
            static_cast<DWORD>(sizeof(m_pos)), &m_pos);
        
        if (!simRC1.isOK() || !simRC2.isOK() || !simRC.isOK() || !aiRC.isOK()) {
            GSLogStream::LogError("GSSimObjReq::GSReqSpawnAircraft::OnMessage Failed call Freeze: ") << 
                simRC1.rc << "," << simRC2.rc << "," << simRC.rc;
        }
    } else {
        GSLogStream::LogError("GSSimObjReq::GSReqSpawnAircraft::OnMessage Unexpected Message: ") << message->dwID;
    }
    (void)messageSize;
    return true;
}

void GSAircraftTrackerThread::GSReqSpawnAircraft::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSLogStream::LogError("GSSimObjReq::GSReqSpawnAircraft::OnException: ") << message->dwException << ", " << message->dwIndex;
}

} // namespace NS_GSLiveAirportMSFS
