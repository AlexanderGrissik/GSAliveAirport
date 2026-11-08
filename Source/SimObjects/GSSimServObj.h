// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "GSAircraft.h"
#include "Animation/GSAnimationObject.h"
#include "General/GSRoadsNetwork.h"

namespace NS_GSAliveAirport
{

class GSSimServObj : public GSSimObj
{
public:
    GSSimServObj(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent):
        GSSimObj(simHandle, parent), m_aircraft(aircraft) {}
    ~GSSimServObj() override = default;

    static void InitDatums(GSSimConnect& handler);

    virtual void OnArrived() = 0;

    void Spawn() override;
    void Despawn() override;
    void DespawnCont();
    void QuickArrive() override;
    void OnArrival() override;
    void OnObjCreated() override;
    void RestoreInitPos();
    void SetTgtSpawnTime(const GSTimePoint& tp);

    void RegisterAnim(GSAnimationObject* animObj);
    void UnregisterAnim();
    void PrepareRoute();
    void FinalizeRoute();

protected:
    static GSTimePoint NextSpawnTime();

    void UnFreeze();
    void Freeze();
    void AddAttached(GSSimServObj* obj);
    void FinalizeBackRoute();
    virtual void OnDespawning() = 0;
    virtual bool IsDelayedSpawnable() const = 0;
    virtual bool IsVehicle() const = 0;
    virtual bool IsArriveReverse() const = 0;
    virtual bool IsLeaveReverse() const = 0;

    GSAircraft& m_aircraft;
    std::shared_ptr<GSTimePoint> m_tgtSpawnTime{nullptr};
    GSCoord m_preMoveInitPos;
    bool m_isFullModel = true;
    bool m_hasAnim = false;
    bool m_routePrepared = false;
    bool m_leaving = false;
};

} // namespace NS_GSAliveAirport
