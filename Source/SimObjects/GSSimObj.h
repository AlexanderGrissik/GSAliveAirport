// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../General/GSCoord.h"
#include "../General/GSLogStream.h"
#include "../Requests/GSSimObjReq.h"
#include "../General/GSSimConnect.h"
#include "../Animation/GSAnimationObject.h"
#include "../General/GSRoadsNetwork.h"
#include "GSAircraft.h"
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

    GSSimObj(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent):
        m_simHandle(simHandle), m_aircraft(aircraft), m_parent(parent) {}
    virtual ~GSSimObj() {}

    static void InitDatums(GSSimConnect& handler);
    
    virtual bool PreSpawn() = 0;
    virtual void OnArrived() = 0;
    virtual bool OnCreated() = 0;

    void OnSpawned(bool ok, GSSimObj& obj) override;
    void OnDespawned() override; 

    void Spawn();
    void Despawn();
    void SpawnAttached();
    void DespawnAttached();
    void Freeze();
    void RegisterAnim(GSAnimationObject* animObj);
    void UnregisterAnim();
    
    const std::string& GetTitle() const { return m_title; }
    const SIMCONNECT_DATA_INITPOSITION& GetInitPos() const { return m_initPos; }
    SIMCONNECT_DATA_INITPOSITION& GetInitPos() { return m_initPos; }
    SIMCONNECT_OBJECT_ID GetSimObjectID() const { return m_simObjectID; }
    GSSimConnect& GetSimConnect() const { return m_simHandle; }
    const SIMCONNECT_DATA_WAYPOINT& GetDestPoint() const { return m_aiWaypoints.back(); }
    const GSTimePoint& GetStartMoveTime() const { return m_startMoveTime; } 

    void SetSimObjectID(DWORD id) { m_simObjectID = static_cast<SIMCONNECT_OBJECT_ID>(id); }
    void OnObjSpawned(bool ok) { m_parent.OnSpawned(ok, *this); }
    void OnObjDespawned() { m_parent.OnDespawned(); }
    void SetTitle(const std::string& title) { m_title = title; }
    void SetHeading(double heading) { m_initPos.Heading = heading; }
    void SetPosition(const GSCoord& pos) { m_initPos.Longitude = pos.Long(); m_initPos.Latitude = pos.Lat(); }
    void SetDeferredSpawn(const GSTimePoint& val) { m_deferredSpawn = val; }
    void PrepareRoute();
    void FinalizeRoute();
    void ContinueSpawn();
    void RestoreInitPos();

    void AddWaypoint(const GSCoord& pos, float alt, float ktsSpeed, float percThrot, unsigned flags);
    void ShootWaypoints();

protected:

    virtual void OnDespawning() = 0;
    
    GSSimConnect& m_simHandle;
    GSAircraft& m_aircraft;
    SIMCONNECT_OBJECT_ID m_simObjectID = 0;
    GSSimObjUpdate& m_parent;
    std::string m_title;
    SIMCONNECT_DATA_INITPOSITION m_initPos{};
    std::vector<std::unique_ptr<GSSimObj>> m_attached; 
    std::vector<SIMCONNECT_DATA_WAYPOINT> m_aiWaypoints;
    std::optional<GSTimePoint> m_deferredSpawn;
    GSTimePoint m_startMoveTime{};
    GSCoord m_preMoveInitPos;
    size_t m_childSpawned = 0;
    bool m_hasAnim = false;
    bool m_routePrepared = false;
    bool m_spawnRequested = false;
};



}
