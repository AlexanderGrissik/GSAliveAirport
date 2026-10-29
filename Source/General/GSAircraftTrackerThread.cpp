// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAircraftTrackerThread.h"
#include "../Requests/GSReqTrackerScanAircrafts.h"
#include "../Requests/GSReqTrackerAircraftsInfo.h"
#include "../Requests/GSReqTrackerAirportInfo.h"
#include "../Requests/GSReqTrackerScanAirports.h"
#include "../Requests/GSReqTrackerSpawnAircraft.h"
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
    InvokeMapClientEvent(GSEventID_Jetway, "TOGGLE_JETWAY");
    InvokeMapClientEvent(GSEventID_BeaconLights, "BEACON_LIGHTS_SET");
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
        m_singleObserver.PostCommand(new GSCmdAircraftUpdate(GS_CMD_SPAWNER_AIRCRAFT_REMOVED, aircraft.second->GetObjID()));
    }); 

    UnsubscribeAirportList();
    m_tracked.clear();
    m_airportCache.clear();
    m_activeAirports.clear();
}

void GSAircraftTrackerThread::RequestScan()
{
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Request Aircraft Scan");
    PostReqCommand(new GSReqTrackerScanAircrafts(*this));
    m_scanInProgress = true;
    m_lastScanTime = std::chrono::steady_clock::now();
}

void GSAircraftTrackerThread::RequestAirportScan()
{
    m_airportCache.clear();
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Request Airport Full Scan");
    PostReqCommand(new GSReqTrackerScanAirports(*this));
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

        m_singleObserver.PostCommand(new GSCmdAircraftUpdate(GS_CMD_SPAWNER_AIRCRAFT_REMOVED, node.mapped()->GetObjID()));
    }); 

    m_lastScanIDs.clear();
}

void GSAircraftTrackerThread::OnCommand(GSCommand& cmd)
{
    switch (cmd.GetCmdID()) {
    case GS_CMD_SPAWN_TEST_AIRCRAFTS:
        SpawnTestAircrafts();
        break;
    default:
        GSLogStream::LogError("GSAircraftTrackerThread - Unexpected cmd: ") << cmd.GetCmdID();
        break;
    }
}

void GSAircraftTrackerThread::SpawnTestAircrafts()
{
    PostReqCommand(new GSReqTrackerSpawnAircraft(*this, 1));
    PostReqCommand(new GSReqTrackerSpawnAircraft(*this, 2));
    PostReqCommand(new GSReqTrackerSpawnAircraft(*this, 3));
    PostReqCommand(new GSReqTrackerSpawnAircraft(*this, 4));
    PostReqCommand(new GSReqTrackerSpawnAircraft(*this, 5));
}

void GSAircraftTrackerThread::SubscribeAirportList()
{
    GSInvokeRC rc = Invoke(SimConnect_SubscribeToFacilities_EX1,
        SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, m_airportAddedReqID, m_airportRemovedReqID);
    if (!rc.isOK())
        GSLogStream::LogError("SimConnect_SubscribeToFacilities_EX1 failed: ") << rc.rc;
}

void GSAircraftTrackerThread::UnsubscribeAirportList()
{
    GSInvokeRC rc = Invoke(SimConnect_UnsubscribeToFacilities_EX1,
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

    m_singleObserver.PostCommand(new GSCommand(GS_CMD_AIRCRAFT_USER));
    m_animThread.PostCommand(new GSCommand(GS_CMD_AIRCRAFT_USER));
}

bool GSAircraftTrackerThread::HandleExistingAircraft(
    GSAircraft &aircraft, std::shared_ptr<GSAircraft> &exisitng, const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE& entry)
{
    if (!aircraft.IsOnGround() && !exisitng->IsOnGround()) 
        return false;

    aircraft.LoadDynamicState(entry);
    if (aircraft != *exisitng) {
        exisitng->CopyDynInfo(aircraft);
        m_singleObserver.PostCommand(new GSCmdAircraftUpdate(GS_CMD_SPAWNER_AIRCRAFT_MODIFIED, exisitng->GetObjID()));
    }

    return true;
}

void GSAircraftTrackerThread::PostNewAircraft(std::shared_ptr<GSAircraft>& aircraft)
{
    m_singleObserver.PostCommand(new GSCmdAircraftAdd(GS_CMD_SPAWNER_AIRCRAFT_ADDED, aircraft));
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
                PostReqCommand(new GSReqTrackerAircraftsInfo(*this, aircraft.GetObjID(), *airportInfo));
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
            PostReqCommand(new GSReqTrackerAirportInfo(*this, entry.dwObjectID, airportInfo)); // Wait to be loaded.
        }
    } else {
        aircraft.SetAirport(m_activeAirports.emplace(
            airportInfo.first, std::make_pair<size_t, std::shared_ptr<GSAirport>>(
                1, std::make_shared<GSAirport>(airportInfo.first, airportInfo.second, entry.dwObjectID))).first->second.second);
        PostReqCommand(new GSReqTrackerAirportInfo(*this, entry.dwObjectID, airportInfo));
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
            PostReqCommand(new GSReqTrackerAirportInfo(*this, aircraftID, airportInfo));
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

GSReqRC GSAircraftTrackerThread::SpawnTestAircraft(int type, SIMCONNECT_DATA_INITPOSITION& pos)
{
    const auto posUser = m_userAircraft.GetLongLat();
    if (posUser.Long() < 900.0 || !m_userAircraft.GetAirport()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqTrackerSpawnAircraft::Process No User Aircrat/Airport");
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
    GSReqRC rc = {
        InvokeRequest(id, SimConnect_AICreateSimulatedObject_EX1, title.c_str(), "", pos, id), true };

    if (!rc.isOK()) {
        GSLogStream::LogError("GSAircraftTrackerThread::GSReqTrackerSpawnAircraft::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
    }

    return rc;
}

} // namespace NS_GSAliveAirport
