// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSSimUtilObj.h"
#include "Commands/GSCmdSimObj.h"
#include "Requests/GSReqSimObjCreate.h"

namespace NS_GSAliveAirport
{

void GSSimUtilObj::Spawn()
{
    auto* req = new GSReqSimObjCreate(m_simHandle, *this);
    m_simHandle.PostReqCommand(req);
}

void GSSimUtilObj::Despawn()
{
    if (m_despawnRequested) {
        return;
    }
    m_despawnRequested = true;
    DespawnAttached();
    m_simHandle.PostCommand(new GSCmdSimObj(GS_CMD_MVMNT_OBJ_REM, *this));
}

bool GSSimUtilObj::OnCreated()
{
    PrepareNextDestRoute();
    ShootWaypoints(false);
    return true;
}

void GSSimUtilObj::OnArrival()
{
    if (m_despawnRequested) {
        return;
    }
    PrepareNextDestRoute();
    ShootWaypoints(false);
}

void GSSimUtilObj::DespawnCont()
{
    DespawnReady();
}

// This method assumes at least 1 Road Path. Otherwise this object must not be used.
void GSSimUtilObj::PrepareInitPos()
{
    m_lastDest = m_airport.GetRoadNet().GetRandomRoadNode(nullptr);
    m_initPos.Heading = 0;
    m_initPos.Longitude = m_lastDest->m_loc.Long();
    m_initPos.Latitude = m_lastDest->m_loc.Lat();
}

// This method assumes at least 1 Road Path. Otherwise this object must not be used.
void GSSimUtilObj::PrepareNextDestRoute()
{
    const auto nextDest = m_airport.GetRoadNet().GetRandomRoadNode(m_lastDest);
    
    std::list<const GSRoadsNetwork::RoadNode*> route;
    m_airport.GetRoadNet().FindShortestPath(*m_lastDest, *nextDest, route);
    
    static unsigned flags = SIMCONNECT_WAYPOINT_ON_GROUND | SIMCONNECT_WAYPOINT_SPEED_REQUESTED;
    m_aiWaypoints.reserve(route.size() + 10);
    m_aiWaypoints.clear();
    for (const auto* pnt : route) {
        m_aiWaypoints.emplace_back(pnt->m_loc.Lat(), pnt->m_loc.Long(), m_initPos.Altitude, flags, 30.0, 0.0);
    }

    if (route.size()) {
        m_lastDest = route.back();
    }
}

} // namespace NS_GSAliveAirport
