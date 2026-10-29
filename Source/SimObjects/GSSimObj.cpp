// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSSimObj.h"
#include "../Commands/GSCmdReq.h"
#include "../Commands/GSCmdAnimObj.h"
#include "../General/GSSimConnect.h"
#include "../General/GSGeography.h"
#include "../Commands/GSCmdSimObj.h"
#include "../Requests/GSReqSimObjCreate.h"
#include "../Requests/GSReqSimObjDelete.h"
#include "../Requests/GSReqSimObjSetData.h"
#include "../Requests/GSReqSimObjTxClientEvent.h"
#include "../Requests/GSReqSimObjContDespawn.h"

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_Position{
    GSDatum{"Initial Position", nullptr, SIMCONNECT_DATATYPE_INITPOSITION},
};

constexpr std::array GSDatums_AIWaypoints{
    GSDatum{"AI WAYPOINT LIST", "number", SIMCONNECT_DATATYPE_WAYPOINT},
};

void GSSimObj::InitDatums(GSSimConnect& handler)
{
    GSAnimationObject::InitDatums(handler);
    handler.InvokeAddDatums(GSDatums_AIWaypoints, GSDatumID_AIWaypoints);

    handler.InvokeMapClientEvent(GSEventID_Freeze_LongLat, "FREEZE_LATITUDE_LONGITUDE_SET");
    handler.InvokeMapClientEvent(GSEventID_Freeze_Altitude, "FREEZE_ALTITUDE_SET");
    handler.InvokeMapClientEvent(GSEventID_Freeze_Attitude, "FREEZE_ATTITUDE_SET");
    handler.InvokeMapClientEvent(GSEventID_OpenDoors, "OPEN_AIRCRAFT_DOORS");
    handler.InvokeMapClientEvent(GSEventID_CloseDoors, "CLOSE_AIRCRAFT_DOORS");
}

void GSSimObj::Spawn()
{
    if (m_spawnRequested) {
        return;
    }

    m_spawnRequested = true;
    auto* req = new GSReqSimObjCreate(m_simHandle, *this);
    if (m_deferredSpawn.has_value()) {
        m_simHandle.PostReqCommandDeferred(req, *m_deferredSpawn);
    } else {
        m_simHandle.PostReqCommand(req);
    }
}

void GSSimObj::Despawn()
{
    if (m_attached.size() > 0) {
        DespawnAttached();
    } else if (m_hasAnim) {
        UnregisterAnim();
    } else {
        OnDespawning();
        m_simHandle.PostReqCommand(new GSReqSimObjDelete(m_simHandle, *this));
    }
}

void GSSimObj::SpawnAttached()
{
    for (auto& obj : m_attached)
        obj->Spawn();
}

void GSSimObj::DespawnAttached()
{
    for (auto& obj : m_attached)
        obj->Despawn();
}

void GSSimObj::OnSpawned(bool ok, GSSimObj& obj)
{
    (void)obj; (void)ok;
    if (++m_childSpawned == m_attached.size())
        OnObjSpawned(true);
}

void GSSimObj::OnDespawned()
{
    if (--m_childSpawned <= 0) {
        m_attached.clear();
        Despawn();
    }
}

void GSSimObj::Freeze()
{
    //auto reqID = m_simHandle.NextRequestID();
    //m_simHandle.Invoke(SimConnect_AIReleaseControl, m_simObjectID, reqID);

    m_simHandle.PostReqCommand(new GSReqSimObjTxClientEvent(m_simHandle, *this, GSEventID_Freeze_LongLat, 1));
    m_simHandle.PostReqCommand(new GSReqSimObjTxClientEvent(m_simHandle, *this, GSEventID_Freeze_Altitude, 1));
    m_simHandle.PostReqCommand(new GSReqSimObjTxClientEvent(m_simHandle, *this, GSEventID_Freeze_Attitude, 1));

    std::array updatePos{ m_initPos };
    m_simHandle.PostReqCommand(new GSReqSimObjSetData<SIMCONNECT_DATA_INITPOSITION, 1>(
        m_simHandle, *this, GSDatumID_Position, std::move(updatePos)));
}

void GSSimObj::RegisterAnim(GSAnimationObject* animObj)
{
    m_simHandle.PostCommand(new GSCmdAnimObj(GS_CMD_ANIM_OBJ_ADD, m_simHandle, animObj));
    m_hasAnim = true;
}

void GSSimObj::UnregisterAnim()
{
    m_simHandle.PostCommand(
        new GSCmdAnimObj(GS_CMD_ANIM_OBJ_REM, m_simHandle, m_simObjectID, new GSReqSimObjContDespawn(m_simHandle, *this)));
    m_hasAnim = false;
}

void GSSimObj::AddWaypoint(const GSCoord& pos, float alt, float ktsSpeed, float percThrot, unsigned flags)
{
    m_aiWaypoints.emplace_back(SIMCONNECT_DATA_WAYPOINT{ pos.Lat(), pos.Long(), alt, flags, ktsSpeed, percThrot });
}

void GSSimObj::ShootWaypoints()
{
    //GSLogStream::Log("GSSimObj::ShootWaypoints - object ") << m_simObjectID << " points=" << m_aiWaypoints.size();
    //for (size_t index = 0; index < m_aiWaypoints.size(); ++index) {
    //    const auto& waypoint = m_aiWaypoints[index];
    //    GSLogStream::Log() << "  [" << index << "] lat=" << waypoint.Latitude << " long=" << waypoint.Longitude << " alt=" << waypoint.Altitude;
    //}

    m_simHandle.Invoke(SimConnect_SetDataOnSimObject,
        GSDatumID_AIWaypoints, m_simObjectID, 0, static_cast<DWORD>(m_aiWaypoints.size()),
        static_cast<DWORD>(sizeof(SIMCONNECT_DATA_WAYPOINT)), m_aiWaypoints.data());

    m_startMoveTime = std::chrono::steady_clock::now();
    m_simHandle.PostCommand(new GSCmdSimObj(GS_CMD_MVMNT_OBJ_ADD, *this));
}

void GSSimObj::PrepareRoute()
{
    const auto* ap = m_aircraft.GetAirport();
    const auto* acPrk = m_aircraft.GetParking();
    if (!ap || !acPrk)
        return;

    const GSRoadsNetwork::RoadNode* srcNode = nullptr;
    auto prk = ap->GetRoadNet().GetRandomVehicleParking();
    if (prk.has_value()) {
        srcNode = prk.value();
    } else {
        auto prk2 = ap->GetRoadNet().GetRandomNode();
        if (prk2.has_value())
            srcNode = prk2.value();
        else
            return;
    }
    
    std::list<const GSRoadsNetwork::RoadNode*> route;
    ap->GetRoadNet().FindShortestPath(*srcNode, *acPrk, route);
    if (route.size() < 2) {
        route.clear();
        return;
    }

    static unsigned flags = SIMCONNECT_WAYPOINT_ON_GROUND | SIMCONNECT_WAYPOINT_SPEED_REQUESTED;
    m_aiWaypoints.reserve(route.size() + 10);
    for (const auto* pnt : route) {
        m_aiWaypoints.emplace_back(pnt->m_loc.Lat(), pnt->m_loc.Long(), m_initPos.Altitude, flags, 30.0, 0.0);
    }

    m_aiWaypoints.pop_back();

    m_preMoveInitPos = { m_initPos.Longitude, m_initPos.Latitude };
    const auto& src = m_aiWaypoints.front();
    m_initPos.Longitude = src.Longitude;
    m_initPos.Latitude = src.Latitude;
    m_routePrepared = true;
}

void GSSimObj::FinalizeRoute()
{
    if (m_aiWaypoints.empty()) {
        OnArrived();
        return;
    }

    static unsigned flags = SIMCONNECT_WAYPOINT_ON_GROUND | SIMCONNECT_WAYPOINT_SPEED_REQUESTED;
    auto intrCoord = GSGeography::FindReverseCircleIntersection(
        m_aircraft.GetLongLat(), m_aircraft.GetWingSpanMeters() / 2.0,
        { m_initPos.Longitude, m_initPos.Latitude }, m_initPos.Heading + 180.0);

    m_aiWaypoints.emplace_back(intrCoord.Lat(), intrCoord.Long(), m_initPos.Altitude, flags, 30.0, 0.0);
    m_aiWaypoints.emplace_back(m_initPos.Latitude, m_initPos.Longitude, m_initPos.Altitude, flags, 30.0, 0.0);

    ShootWaypoints();
}

void GSSimObj::RestoreInitPos()
{ 
    if (m_routePrepared) {
        m_initPos.Longitude = m_preMoveInitPos.Long(); 
        m_initPos.Latitude = m_preMoveInitPos.Lat(); 
        m_routePrepared = false;
    }
}

void GSSimObj::ContinueSpawn()
{
    if (m_attached.size())
        SpawnAttached();
    else
        OnObjSpawned(true);
}


}
