// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "General/GSCoord.h"
#include "General/GSLogStream.h"
#include "Requests/GSSimObjReq.h"
#include "General/GSSimConnect.h"
#include "GSSimObjUpdate.h"
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace NS_GSAliveAirport
{

class GSSimObj : public GSSimObjUpdate
{
public:

    GSSimObj(GSSimConnect& simHandle, GSSimObjUpdate& parent):
        m_simHandle(simHandle), m_parent(parent) {}
    virtual ~GSSimObj() {}

    static void InitDatums(GSSimConnect& handler);

    virtual bool PreSpawn() = 0;
    virtual bool OnCreated() = 0;
    virtual void OnObjCreated() = 0;
    virtual void Spawn() = 0;
    virtual void Despawn() = 0;
    virtual void OnArrival() = 0;
    
    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned() override; 

    const std::string& GetTitle() const { return m_title; }
    const SIMCONNECT_DATA_INITPOSITION& GetInitPos() const { return m_initPos; }
    SIMCONNECT_DATA_INITPOSITION& GetInitPos() { return m_initPos; }
    SIMCONNECT_OBJECT_ID GetSimObjectID() const { return m_simObjectID; }
    GSSimConnect& GetSimConnect() const { return m_simHandle; }
    const SIMCONNECT_DATA_WAYPOINT* GetDestPoint() const { return m_aiWaypoints.size() ? &m_aiWaypoints[m_arriveIdx] : nullptr; }
    const GSTimePoint& GetStartMoveTime() const { return m_startMoveTime; } 

    void SetSimObjectID(DWORD id) { m_simObjectID = static_cast<SIMCONNECT_OBJECT_ID>(id); }
    void OnObjSpawned(bool ok) { m_parent.OnSpawned(ok, *this); }
    void OnObjDespawned() { m_parent.OnDespawned(); }
    void SetTitle(const std::string& title) { m_title = title; }
    void SetHeading(double heading) { m_initPos.Heading = heading; }
    void SetPosition(const GSCoord& pos) { m_initPos.Longitude = pos.Long(); m_initPos.Latitude = pos.Lat(); }
    void ContinueSpawn();
    void DespawnAttached();

    void AddWaypoint(const GSCoord& pos, float alt, float ktsSpeed, float percThrot, unsigned flags);
    void ShootWaypoints(bool reverseArrival);
    void ShootWaypointsRev();

protected:

    void DespawnReady();
    void DespawnCommand();
    
    GSSimConnect& m_simHandle;
    SIMCONNECT_OBJECT_ID m_simObjectID = 0;
    GSSimObjUpdate& m_parent;
    std::string m_title;
    SIMCONNECT_DATA_INITPOSITION m_initPos{};
    std::vector<std::unique_ptr<GSSimObj>> m_attached; 
    std::vector<SIMCONNECT_DATA_WAYPOINT> m_aiWaypoints;
    GSTimePoint m_startMoveTime{};
    size_t m_childSpawned = 0;
    size_t m_arriveIdx = 0;
    bool m_spawnRequested = false;
    bool m_readyDespawn = false;
};



}
