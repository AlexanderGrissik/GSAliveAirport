// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSSimObj.h"
#include "Commands/GSCmdReq.h"
#include "Commands/GSCmdAnimObj.h"
#include "General/GSSimConnect.h"
#include "General/GSGeography.h"
#include "Commands/GSCmdSimObj.h"
#include "Requests/GSReqSimObjCreate.h"
#include "Requests/GSReqSimObjDelete.h"
#include "Requests/GSReqSimObjSetData.h"
#include "Requests/GSReqSimObjTxClientEvent.h"
#include "Requests/GSReqSimObjContDespawn.h"

using namespace std::chrono_literals;

namespace NS_GSAliveAirport
{

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

GSTimePoint GSSimObj::NextSpawnTime()
{
    static GSTimePoint s_LastSpawnSlot = std::chrono::steady_clock::now();

    GSTimePoint curr = std::chrono::steady_clock::now();
    GSTimePoint next = s_LastSpawnSlot + 1s;

    s_LastSpawnSlot = next < curr ? curr : next;
    return s_LastSpawnSlot;
}

void GSSimObj::SetTgtSpawnTime(const GSTimePoint& tp)
{
    if (!m_tgtSpawnTime) {
        m_tgtSpawnTime = std::make_shared<GSTimePoint>(tp);
    } else {
        *m_tgtSpawnTime = tp;
    }
}

void GSSimObj::Spawn()
{
    if (m_spawnRequested) {
        return;
    }

    if (!m_tgtSpawnTime && IsDelayedSpawnable()) {
        SetTgtSpawnTime(NextSpawnTime());
    }

    m_spawnRequested = true;
    auto* req = new GSReqSimObjCreate(m_simHandle, *this);
    if (m_tgtSpawnTime)
        m_simHandle.PostReqCommandDeferred(req, m_tgtSpawnTime);
    else
        m_simHandle.PostReqCommand(req);
}

void GSSimObj::Despawn()
{
    if (m_attached.size() > 0) {
        for (auto& obj : m_attached)
            obj->Despawn();
    } else if (m_hasAnim) {
        UnregisterAnim();
    } else {
        OnDespawning();
        m_simHandle.PostReqCommand(new GSReqSimObjDelete(m_simHandle, *this));
    }
}

void GSSimObj::QuickArrive()
{
    if (!m_quickArrive) {
        m_quickArrive = true;
        SetTgtSpawnTime(std::chrono::steady_clock::now());
        if (!m_aiWaypoints.empty() && m_simObjectID != 0)
            m_simHandle.PostCommand(new GSCmdSimObj(GS_CMD_MVMNT_OBJ_QUICK, *this));
        
        for (auto& obj : m_attached)
            obj->QuickArrive();
    }
}

void GSSimObj::AddAttached(GSSimObj* obj)
{
    if (m_quickArrive) {
        obj->SetTgtSpawnTime(std::chrono::steady_clock::now());
        obj->SetQuickArriveFlag();
    }

    m_attached.emplace_back(std::unique_ptr<GSSimObj>(obj));
}

void GSSimObj::OnSpawned(bool ok, GSSimObj& obj)
{
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
    m_hasAnim = true;
    m_simHandle.PostCommand(new GSCmdAnimObj(GS_CMD_ANIM_OBJ_ADD, m_simHandle, animObj));
}

void GSSimObj::UnregisterAnim()
{
    m_hasAnim = false;
    m_simHandle.PostCommand(
        new GSCmdAnimObj(GS_CMD_ANIM_OBJ_REM, m_simHandle, m_simObjectID, new GSReqSimObjContDespawn(m_simHandle, *this)));
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

    size_t skipLast = m_arriveReverse ? 1 : 0;
    m_arriveIdx = m_aiWaypoints.size() - skipLast - 1U;
    m_simHandle.Invoke(SimConnect_SetDataOnSimObject,
        GSDatumID_AIWaypoints, m_simObjectID, 0U, static_cast<DWORD>(m_aiWaypoints.size() - skipLast),
        static_cast<DWORD>(sizeof(SIMCONNECT_DATA_WAYPOINT)), 
        m_aiWaypoints.data());

    m_startMoveTime = std::chrono::steady_clock::now();
    m_simHandle.PostCommand(new GSCmdSimObj(GS_CMD_MVMNT_OBJ_ADD, *this));
}

void GSSimObj::ShootWaypointsRev()
{
    m_arriveReverse = false;
    m_arriveIdx = m_aiWaypoints.size() - 1U;
    m_simHandle.Invoke(SimConnect_SetDataOnSimObject,
        GSDatumID_AIWaypoints, m_simObjectID, 0U, 1U,
        static_cast<DWORD>(sizeof(SIMCONNECT_DATA_WAYPOINT)),
        m_aiWaypoints.data() + m_arriveIdx);
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
    m_initPos.Longitude = src.Longitude; // Spawn at the start of the road.
    m_initPos.Latitude = src.Latitude;
    m_routePrepared = true;
}

void GSSimObj::FinalizeRoute()
{
    if (m_aiWaypoints.empty() || m_quickArrive) {
        OnArrived();
        return;
    }

    unsigned flags = SIMCONNECT_WAYPOINT_ON_GROUND | SIMCONNECT_WAYPOINT_SPEED_REQUESTED;
    GSCoord intrCoord = m_aircraft.GetLongLat();
    GSCoord endIntrersection;
    GSCoord otherIntersection;
    bool pointInside = false;
    double halfWing = (m_aircraft.GetWingSpanMeters() / 2.0) * 1.5;

    bool intersects = GSGeography::FindReverseCircleIntersection(
        m_aircraft.GetLongLat(), halfWing,
        { m_initPos.Longitude, m_initPos.Latitude }, m_initPos.Heading,
        endIntrersection, otherIntersection, pointInside); // Intersection of the m_initPos with the circle.

    if (intersects && pointInside) {
        const auto& lastRoadPntWP = m_aiWaypoints.back();
        GSCoord lastRoadPnt(lastRoadPntWP.Longitude, lastRoadPntWP.Latitude);
        double roadHeading = GSGeography::Azz(lastRoadPnt, m_aircraft.GetLongLat());
        if (m_arriveReverse)
            endIntrersection = otherIntersection;
        
        GSCoord startIntersection = endIntrersection;
        intersects = GSGeography::FindReverseCircleIntersection(
            m_aircraft.GetLongLat(), halfWing, lastRoadPnt, roadHeading,
            startIntersection, otherIntersection, pointInside); // Intersection of the lastRoadPnt with the circle.

        m_aiWaypoints.emplace_back(startIntersection.Lat(), startIntersection.Long(), m_initPos.Altitude, flags, 15.0, 0.0);

        double startHeading = GSGeography::NormDeg(roadHeading + 180.0);
        double endHeading = GSGeography::Azz(m_aircraft.GetLongLat(), endIntrersection);
        double outHeading = startHeading;
        size_t index = 0;
        while (GSGeography::PointOnCircle(startHeading, endHeading, 10.0, index++, outHeading) && index < 36) {
            otherIntersection = GSGeography::RelativePosition(outHeading, m_aircraft.GetLongLat(), halfWing, 0.0);
            m_aiWaypoints.emplace_back(otherIntersection.Lat(), otherIntersection.Long(), m_initPos.Altitude, flags, 15.0, 0.0);
        }

        m_aiWaypoints.emplace_back(endIntrersection.Lat(), endIntrersection.Long(), m_initPos.Altitude, flags, 15.0, 0.0);

        if (m_arriveReverse) {
            otherIntersection = GSGeography::RelativePosition(m_initPos.Heading, endIntrersection, 15.0, 0.0);
            m_aiWaypoints.emplace_back(otherIntersection.Lat(), otherIntersection.Long(), m_initPos.Altitude, flags, 15.0, 0.0);
            flags |= SIMCONNECT_WAYPOINT_REVERSE;
        }
    }
    
    m_aiWaypoints.emplace_back(m_initPos.Latitude, m_initPos.Longitude, m_initPos.Altitude, flags, 5.0, 0.0);

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
    if (m_attached.size()) {
        for (auto& obj : m_attached)
            obj->Spawn();
    } else
        OnObjSpawned(true);
}


}
