// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimServObj.h"

namespace NS_GSAliveAirport
{

class GSStatic : public GSSimServObj
{
public:

    enum PosRelation {
        REL_ABSOLUTE,
        REL_AIRCRAFT
    };

    using GSSimServObj::GSSimServObj;

    void SetPosRel(PosRelation rel) { m_posRel = rel; }
    void SetAnimObj(GSAnimationObject* animObj) { m_animObj.reset(animObj); }
    bool PreSpawn() override;

private:

    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return false; }
    bool IsArriveReverse() const override { return false; }
    bool IsVehicle() const override { return false; }
    bool IsLeaveReverse() const override { return true; }
    void OnArrived() override {}
    void OnDespawning() override {}

    PosRelation m_posRel = REL_AIRCRAFT;
    std::unique_ptr<GSAnimationObject> m_animObj;
};
}
