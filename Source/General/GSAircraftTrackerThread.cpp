// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraftTrackerThread.h"
#include "../SimObjects/GSAircraft.h"
#include "GSDefinitions.h"
#include "GSGeography.h"
#include "GSLogStream.h"
#include "../Commands/GSCmdAircraftUpdate.h"
#include "../Commands/GSCmdAircraftAdd.h"
#include "GSSpawnerThread.h"
#include "GSStringUtils.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <set>
#include <string>
#include <utility>

namespace NS_GSAliveAirport
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

        const auto currTime = std::chrono::steady_clock::now();
        if (IsSimStarted() && !m_scanInProgress && (currTime - m_lastScanTime) > 4s) {
            RequestScan();

            if (!m_scanAirportInProgress && (currTime - m_lastAirportScanTime) > 300s) {
                RequestAirportScan(); // Full Scan
            }
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
    InvokeMapClientEvent(GSDefinitions::GSDefID_Jetway, "TOGGLE_JETWAY");
    InvokeMapClientEvent(GSDefinitions::GSDefID_BeaconLights, "BEACON_LIGHTS_SET");
    m_lastScanTime = std::chrono::steady_clock::now() - 10s;
    m_lastAirportScanTime = std::chrono::steady_clock::now() - 400s;
}

void GSAircraftTrackerThread::OnDisconnect()
{
    OnSimStop();
    m_scanInProgress = false;
    m_scanAirportInProgress = false;
}

void GSAircraftTrackerThread::OnSimStart()
{
    AddRawRequestHandler(m_airportAddedReqID, m_airportAddedReq);
    AddRawRequestHandler(m_airportRemovedReqID, m_airportRemovedReq);
    SubscribeAirportList();
}

void GSAircraftTrackerThread::OnSimStop()
{
    std::for_each(m_tracked.begin(), m_tracked.end(), [this](auto& aircraft) { 
        CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_REMOVED, aircraft.second->GetObjID()));
        m_singleObserver.PostCommand(cmd);        
    }); 

    UnsubscribeAirportList();
    m_tracked.clear();
    m_airportCache.clear();
    m_activeAirports.clear();
}

void GSAircraftTrackerThread::RequestScan()
{
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Request Aircraft Scan");
    PostReqCommand(new GSReqScan(*this));
    m_scanInProgress = true;
    m_lastScanTime = std::chrono::steady_clock::now();
}

void GSAircraftTrackerThread::RequestAirportScan()
{
    m_airportCache.clear();
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Request Airport Full Scan");
    PostReqCommand(new GSReqScanAirport(*this));
    m_scanAirportInProgress = true;
    m_lastAirportScanTime = std::chrono::steady_clock::now();
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
        if (node.mapped()->GetAirport()) {
            auto itrAirport = m_activeAirports.find(node.mapped()->GetAirport()->GetICAO());
            if (--itrAirport->second.first == 0 && m_activeAirports.size() > 10) {
                m_activeAirports.erase(itrAirport);
                GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Airport erased: ") << node.mapped()->GetAirport()->GetICAO();
            }
        }

        CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_REMOVED, node.mapped()->GetObjID()));
        m_singleObserver.PostCommand(cmd);
    }); 

    m_lastScanIDs.clear();
}

void GSAircraftTrackerThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
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

void GSAircraftTrackerThread::SpawnTestAircrafts()
{
    PostReqCommand(new GSReqSpawnAircraft(*this, 1));
    PostReqCommand(new GSReqSpawnAircraft(*this, 2));
    PostReqCommand(new GSReqSpawnAircraft(*this, 3));
    PostReqCommand(new GSReqSpawnAircraft(*this, 4));
    PostReqCommand(new GSReqSpawnAircraft(*this, 5));
}

void GSAircraftTrackerThread::SubscribeAirportList()
{
    GSDefinitions::SendResult rc = Invoke(SimConnect_SubscribeToFacilities_EX1,
        SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, m_airportAddedReqID, m_airportRemovedReqID);
    if (!rc.isOK())
        GSLogStream::LogError("SimConnect_SubscribeToFacilities_EX1 failed: ") << rc.rc;
}

void GSAircraftTrackerThread::UnsubscribeAirportList()
{
    GSDefinitions::SendResult rc = Invoke(SimConnect_UnsubscribeToFacilities_EX1,
        SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, TRUE, TRUE);
    if (!rc.isOK())
        GSLogStream::LogError("SimConnect_UnsubscribeToFacilities_EX1 failed: ") << rc.rc;
}

void GSAircraftTrackerThread::HandleAircraftUser(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{ 
    m_userAircraft.LoadInitState(entry);
    m_userAircraft.LoadDynamicState(entry);

    if (!m_userAircraft.GetAirport() || 
        GSGeography::DistanceDegPow(m_userAircraft.GetAirport()->GetLongLat(), m_userAircraft.GetLongLat()) > (MinAirportRefreshDistDegPow / 2.0)) {
        const auto* airportInfo = FindClosesAirportToAircraft(m_userAircraft);
        if (airportInfo) {
            if (!m_userAircraft.GetAirport() || m_userAircraft.GetAirport()->GetICAO() != airportInfo->first) {
                auto newAirport = std::make_shared<GSAirport>(airportInfo->first, airportInfo->second, 0);
                m_userAircraft.SetAirport(newAirport);
            }
        }
    }

    CmdPtr cmd1(new GSCommand(GSDefinitions::CMD_AIRCRAFT_USER));
    m_singleObserver.PostCommand(cmd1);
    CmdPtr cmd2(new GSCommand(GSDefinitions::CMD_AIRCRAFT_USER));
    m_animThread.PostCommand(cmd2);
}

bool GSAircraftTrackerThread::HandleExistingAircraft(
    GSAircraft &aircraft, std::shared_ptr<GSAircraft> &exisitng, const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE& entry)
{
    if (!aircraft.IsOnGround() && !exisitng->IsOnGround()) 
        return false;

    aircraft.LoadDynamicState(entry);
    if (aircraft != *exisitng) {
        exisitng->CopyDynInfo(aircraft);
        CmdPtr cmd(new GSCmdAircraftUpdate(GSDefinitions::CMD_SPAWNER_AIRCRAFT_MODIFIED, exisitng->GetObjID()));
        m_singleObserver.PostCommand(cmd);
    }

    return true;
}

void GSAircraftTrackerThread::PostNewAircraft(std::shared_ptr<GSAircraft>& aircraft)
{
    CmdPtr cmd(new GSCmdAircraftAdd(GSDefinitions::CMD_SPAWNER_AIRCRAFT_ADDED, aircraft));
    m_singleObserver.PostCommand(cmd);
}

const GSAircraftTrackerThread::AirportScanCache::value_type* GSAircraftTrackerThread::IsNewAircraftEligable(const GSAircraft &aircraft) const
{
    if (aircraft.GetTitleLower().find("jetwayhelper") != std::string::npos || !aircraft.IsOnGround())
        return nullptr;

    return FindClosesAirportToAircraft(aircraft);
}

bool GSAircraftTrackerThread::HandleScanMessage(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry)
{
    if (entry.dwoutof == 0) { // Empty Scan
        m_scanInProgress = false;
        GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Ending Aircraft Scan (Empty)");
        return true;
    }

    GSAircraft aircraft;
    aircraft.LoadInitState(entry);
    
    if (aircraft.IsUser()) {
        HandleAircraftUser(entry);
    } else {
        auto itr = m_tracked.find(aircraft.GetObjID());
        if (itr != m_tracked.end()) { // Already tracking
            if (HandleExistingAircraft(aircraft, itr->second, entry)) {
                 m_lastScanIDs.emplace(aircraft.GetObjID());
            }
        } else {
            const auto* airportInfo = IsNewAircraftEligable(aircraft);
            if (airportInfo) {
                m_tracked.emplace(aircraft.GetObjID(), std::make_shared<GSAircraft>(aircraft)).first->second->LoadDynamicState(entry);
                PostReqCommand(new GSReqAircraftConstInfo(*this, aircraft.GetObjID(), *airportInfo));
                m_lastScanIDs.emplace(aircraft.GetObjID());
            }
        }
    }

    if (entry.dwoutof == entry.dwentrynumber) {
        HandleRemoved();
        m_scanInProgress = false;
        GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Ending Aircraft Scan");
        return true;
    }

    return false;
}

void GSAircraftTrackerThread::HandleAircraftConstScan(const SIMCONNECT_RECV_SIMOBJECT_DATA& entry, const AirportScanCache::value_type& airportInfo)
{
    auto itr = m_tracked.find(entry.dwObjectID);
    if (itr == m_tracked.end())
        return;

    GSAircraft& aircraft = *itr->second;
    aircraft.LoadFullState(entry);

    auto itrAirport = m_activeAirports.find(airportInfo.first);
    if (itrAirport != m_activeAirports.end()) {
        ++itrAirport->second.first;
        aircraft.SetAirport(itrAirport->second.second);
        if (itrAirport->second.second->IsFinalized()) {
            PostNewAircraft(itr->second);
        } else {
            PostReqCommand(new GSReqAirportInfo(*this, entry.dwObjectID, airportInfo)); // Wait to be loaded.
        }
    } else {
        aircraft.SetAirport(m_activeAirports.emplace(
            airportInfo.first, std::make_pair<size_t, std::shared_ptr<GSAirport>>(
                1, std::make_shared<GSAirport>(airportInfo.first, airportInfo.second, entry.dwObjectID))).first->second.second);
        PostReqCommand(new GSReqAirportInfo(*this, entry.dwObjectID, airportInfo));
    }
}

void GSAircraftTrackerThread::HandleAirportInfoMessage(const SIMCONNECT_RECV_FACILITY_DATA& entry, DWORD aircraftID, const AirportScanCache::value_type& airportInfo)
{
    auto itrAirport = m_activeAirports.find(airportInfo.first);
    if (itrAirport == m_activeAirports.end())
        return;

    if (itrAirport->second.second->LoadingReqID() == aircraftID) { // Another aircraft may load the airport simultaneously.
        itrAirport->second.second->LoadInfo(entry);
    }

    if (entry.dwID == SIMCONNECT_RECV_ID_FACILITY_DATA_END) {
        if (itrAirport->second.second->LoadingReqID() == aircraftID)
            itrAirport->second.second->OrganizeStructures();
        
        if (!itrAirport->second.second->IsFinalized()) { // Rare case of simultaneous load.
            // Try again. Let the LoadingRequestID to finalize the airport
            PostReqCommand(new GSReqAirportInfo(*this, aircraftID, airportInfo));
        } else {
            auto itrAircraft = m_tracked.find(aircraftID);
            if (itrAircraft != m_tracked.end()) {
                PostNewAircraft(itrAircraft->second);
            }
        }
    }
}

bool GSAircraftTrackerThread::HandleScanAirportMessage(const SIMCONNECT_RECV_AIRPORT_LIST &entry)
{
    if (entry.dwOutOf == 0) { // Empty Scan
        GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Ending Airport Scan (Empty)");
        return true;
    }

    for (DWORD i = 0; i < entry.dwArraySize; ++i) {
        const auto& airport = entry.rgData[i];
        if (m_airportCache.find(airport.Ident) == m_airportCache.end()) {
            m_airportCache.emplace(airport.Ident, GSCoord{ airport.Longitude, airport.Latitude });
        }
    }

    if (entry.dwEntryNumber + 1 == entry.dwOutOf) {
        GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Ending Airport Scan");
        return true;
    }

    return false;
}

void GSAircraftTrackerThread::HandleScanAirportRemovedMessage(const SIMCONNECT_RECV_AIRPORT_LIST &entry)
{
    for (DWORD i = 0; i < entry.dwArraySize; ++i) {
        const auto& airport = entry.rgData[i];
        m_airportCache.erase(airport.Ident);
    }
}

const GSAircraftTrackerThread::AirportScanCache::value_type* GSAircraftTrackerThread::FindClosesAirportToAircraft(const GSAircraft& aircraft) const
{
    const AirportScanCache::value_type* closest = nullptr;
    double closestDistancePow = std::numeric_limits<double>::max();
    for (const auto& entry : m_airportCache) {
        const double distancePow = GSGeography::DistanceDegPow(aircraft.GetLongLat(), entry.second);
        if (distancePow < DiscoveryRadiusDegPow && distancePow < closestDistancePow) {
            closestDistancePow = distancePow;
            closest = &entry;
        }
    }

    return closest;
}

GSRequest::SendResult GSAircraftTrackerThread::SpawnTestAircraft(int type, SIMCONNECT_DATA_INITPOSITION& pos)
{
    const auto posUser = m_userAircraft.GetLongLat();
    if (posUser.Long() < 900.0 || !m_userAircraft.GetAirport()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqSpawnAircraft::Process No User Aircrat/Airport");
        return { {0,0,E_FAIL}, false };
    }

    pos.Altitude = m_userAircraft.GetAltitudeFeet();
    pos.Heading = 0.0;
    pos.Pitch = 0;
    pos.Bank = 0;
    pos.OnGround = 1;
    pos.Airspeed = 0;

    std::string title = "747-8i";
    pos.Latitude = posUser.Lat() - 0.001;
    pos.Longitude = posUser.Long() - 0.001;

    if (type == 2) {
        title = "737 Max 8 Passengers";
        const auto parking = m_userAircraft.GetAirport()->GetRoadNet().GetClosestJetwayParking(posUser);
        if (parking) {
            pos.Latitude = (*parking)->m_loc.Lat();
            pos.Longitude = (*parking)->m_loc.Long();
            pos.Heading = (*parking)->m_heading;
        }
    }
    else if (type == 3) {
        title = "Cessna C152";
        pos.Latitude = posUser.Lat() + 0.001;
        pos.Longitude = posUser.Long() + 0.001;
    }
    else if (type == 4) {
        title = "A330-300 (RR)";
        pos.Latitude = posUser.Lat() + 0.001;
        pos.Longitude = posUser.Long() - 0.001;
    }
    else if (type == 5) {
        title = "737 Max 8 Passengers";
        pos.Latitude = posUser.Lat() - 0.001;
        pos.Longitude = posUser.Long() + 0.001;
        pos.Airspeed = 5;
    }

    auto id = NextRequestID();
    GSRequest::SendResult rc = {
        InvokeRequest(id, SimConnect_AICreateSimulatedObject_EX1, title.c_str(), "", pos, id), true };

    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqSpawnAircraft::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    }

    return rc;
}

GSRequest::SendResult GSAircraftTrackerThread::GSReqScan::Process()
{
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Starting Aircraft Scan");

    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestDataOnSimObjectType, id, GSDefinitions::GSDefID_Aircraft, DiscoveryRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT), true };
    
    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqScan::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    } 

    return rc;
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

GSRequest::SendResult GSAircraftTrackerThread::GSReqAircraftConstInfo::Process()
{
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Starting Aircraft Const Info");

    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(
            id, SimConnect_RequestDataOnSimObject, id, GSDefinitions::GSDefID_AircraftConst, m_objectID, SIMCONNECT_PERIOD_ONCE, 0, 0, 0, 0), true };
    
    if (!rc.m_simRC.isOK()) {
        GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
        tracker.ClearFailedAircraft(m_objectID);
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqAircraftConstInfo::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    } 

    return rc;
}

bool GSAircraftTrackerThread::GSReqAircraftConstInfo::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        tracker.ClearFailedAircraft(m_objectID);
        return false;
    }

    tracker.HandleAircraftConstScan(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message), m_airportInfo);
    return true;
}

void GSAircraftTrackerThread::GSReqAircraftConstInfo::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    tracker.ClearFailedAircraft(m_objectID);

    GSLogStream::LogError("GSAircraftTrackerThread::GSReqAircraftConstInfo:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

GSRequest::SendResult GSAircraftTrackerThread::GSReqAirportInfo::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestFacilityData, GSDefinitions::GSDefID_AirportInfo, id, m_airportInfo.first.c_str(), ""), true};
    
    if (!rc.m_simRC.isOK()) {
        GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
        tracker.ClearFailedAircraft(m_objectID);
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqAirportInfo::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    } 

    return rc;
}

bool GSAircraftTrackerThread::GSReqAirportInfo::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    (void)messageSize;
    const auto& entry = *reinterpret_cast<const SIMCONNECT_RECV_FACILITY_DATA*>(message);

    GSAircraftTrackerThread& tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    if (message->dwID == SIMCONNECT_RECV_ID_FACILITY_DATA_END) {
        tracker.HandleAirportInfoMessage(entry, m_objectID, m_airportInfo);
        return true;
    }

    if (message->dwID != SIMCONNECT_RECV_ID_FACILITY_DATA) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqAirportInfo unexpected message: ") << message->dwID;
        tracker.ClearFailedAircraft(m_objectID);
        return true;
    }

    tracker.HandleAirportInfoMessage(entry, m_objectID, m_airportInfo);
    return false;
}

void GSAircraftTrackerThread::GSReqAirportInfo::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    tracker.ClearFailedAircraft(m_objectID);
    GSLogStream::LogError("GSReqAirportInfo request failed: ") << message->dwException;
}

GSRequest::SendResult GSAircraftTrackerThread::GSReqScanAirport::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestFacilitiesList_EX1, SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, id), true };
    
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

    if (tracker.HandleScanAirportMessage(*reinterpret_cast<SIMCONNECT_RECV_AIRPORT_LIST *>(message))) {
        tracker.SetAirportScanInProgress(false);
        return true;
    }

    return false;
}
    
void GSAircraftTrackerThread::GSReqScanAirport::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSAircraftTrackerThread& tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    tracker.SetAirportScanInProgress(false);
    GSLogStream::LogError("GSAircraftTrackerThread::GSReqScanAirport:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

GSRequest::SendResult GSAircraftTrackerThread::GSReqSpawnAircraft::Process()
{
    GSAircraftTrackerThread& tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    return tracker.SpawnTestAircraft(m_type, m_pos);
}

bool GSAircraftTrackerThread::GSReqSpawnAircraft::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    if (message->dwID == SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID) {
        auto* msg = static_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID*>(message);
        
        auto requestId = m_simHandle.NextRequestID();
        auto aiRC = m_simHandle.InvokeRequest(requestId, SimConnect_AIReleaseControl, msg->dwObjectID, requestId);
        auto beaconRC = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSDefinitions::GSDefID_BeaconLights, 0,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

        std::this_thread::sleep_for(100ms);
        m_pos.Airspeed = 0;

        auto simRC1 = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSDefinitions::GSDefID_Freeze_LongLat, 1,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        auto simRC2 = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSDefinitions::GSDefID_Freeze_Altitude, 1,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        auto simRC = m_simHandle.Invoke(SimConnect_SetDataOnSimObject, GSDefinitions::GSDefID::GSDefID_Position, msg->dwObjectID, 0, 1,
            static_cast<DWORD>(sizeof(m_pos)), &m_pos);
        
        if (!simRC1.isOK() || !simRC2.isOK() || !simRC.isOK() || !aiRC.isOK() || !beaconRC.isOK()) {
            GSLogStream::LogError("GSSimObjReq::GSReqSpawnAircraft::OnMessage Failed call Freeze: ") << 
                simRC1.rc << "," << simRC2.rc << "," << simRC.rc << "," << beaconRC.rc;
        }

        std::this_thread::sleep_for(100ms);
    
        if (m_type == 2) {
            m_simHandle.Invoke(
                SimConnect_TransmitClientEvent, msg->dwObjectID, GSDefinitions::GSDefID_Jetway,
                0, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
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

bool GSAircraftTrackerThread::GSReqAirportAdded::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_AIRPORT_LIST) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        return false;
    }

    tracker.HandleScanAirportMessage(*reinterpret_cast<SIMCONNECT_RECV_AIRPORT_LIST *>(message));
    return false;
}

void GSAircraftTrackerThread::GSReqAirportAdded::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSLogStream::LogError("GSAircraftTrackerThread::GSReqAirportAdded Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

bool GSAircraftTrackerThread::GSReqAirportRemoved::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_AIRPORT_LIST) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        return false;
    }

    tracker.HandleScanAirportRemovedMessage(*reinterpret_cast<SIMCONNECT_RECV_AIRPORT_LIST *>(message));
    return false;
}

void GSAircraftTrackerThread::GSReqAirportRemoved::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSLogStream::LogError("GSAircraftTrackerThread::GSReqAirportRemoved Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

} // namespace NS_GSAliveAirport
