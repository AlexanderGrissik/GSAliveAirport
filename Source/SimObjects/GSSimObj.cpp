// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSSimObj.h"
#include "Commands/GSCmdMvmntObjAdd.h"
#include "Requests/GSReqSimObjDelete.h"

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_AIWaypoints{
    GSDatum{"AI WAYPOINT LIST", "number", SIMCONNECT_DATATYPE_WAYPOINT},
};

void GSSimObj::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_AIWaypoints, GSDatumID_AIWaypoints);
}

void GSSimObj::DespawnReady()
{
    m_readyDespawn = true;
    DespawnCommand();
}

void GSSimObj::DespawnCommand()
{
    if (m_readyDespawn && m_attached.empty())
        m_simHandle.PostReqCommand(new GSReqSimObjDelete(m_simHandle, *this));
}

void GSSimObj::DespawnAttached()
{
    if (m_attached.size() > 0) {
        for (auto& obj : m_attached)
            obj->Despawn();
    }
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
        DespawnCommand();
    }
}

void GSSimObj::AddWaypoint(const GSCoord& pos, float alt, float ktsSpeed, float percThrot, unsigned flags)
{
    m_aiWaypoints.emplace_back(SIMCONNECT_DATA_WAYPOINT{ pos.Lat(), pos.Long(), alt, flags, ktsSpeed, percThrot });
}

void GSSimObj::ShootWaypoints(bool reverseArrival)
{
    //GSLogStream::Log("GSSimObj::ShootWaypoints - object ") << m_simObjectID << " points=" << m_aiWaypoints.size();
    //for (size_t index = 0; index < m_aiWaypoints.size(); ++index) {
    //    const auto& waypoint = m_aiWaypoints[index];
    //    GSLogStream::Log() << "  [" << index << "] lat=" << waypoint.Latitude << " long=" << waypoint.Longitude << " alt=" << waypoint.Altitude;
    //}

    size_t skipLast = reverseArrival ? 1 : 0;
    if (m_aiWaypoints.size() > skipLast) {
        m_arriveIdx = m_aiWaypoints.size() - skipLast - 1U;
        m_simHandle.Invoke(SimConnect_SetDataOnSimObject,
            GSDatumID_AIWaypoints, m_simObjectID, 0U, static_cast<DWORD>(m_aiWaypoints.size() - skipLast),
            static_cast<DWORD>(sizeof(SIMCONNECT_DATA_WAYPOINT)), 
            m_aiWaypoints.data());
    }

    m_startMoveTime = std::chrono::steady_clock::now();
    m_simHandle.PostCommand(new GSCmdMvmntObjAdd(*this, reverseArrival));
}

void GSSimObj::ShootWaypointsRev()
{
    m_arriveIdx = m_aiWaypoints.size() - 1U;
    m_simHandle.Invoke(SimConnect_SetDataOnSimObject,
        GSDatumID_AIWaypoints, m_simObjectID, 0U, 1U,
        static_cast<DWORD>(sizeof(SIMCONNECT_DATA_WAYPOINT)),
        m_aiWaypoints.data() + m_arriveIdx);
}

void GSSimObj::ContinueSpawn()
{
    if (m_attached.size()) {
        for (auto& obj : m_attached)
            obj->Spawn();
    } else
        OnObjSpawned(true);
}

} // namespace NS_GSAliveAirport
