// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimServObj.h"
#include <cstddef>

namespace NS_GSAliveAirport
{

class GSWingmans : public GSSimServObj
{
public:

    enum CountType{
        AIRLINE,
        SMALL
    };

    GSWingmans(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, CountType tp):
        GSSimServObj(simHandle, aircraft, parent), m_countTP(tp) {}

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return false; }
    bool IsArriveReverse() const override { return false; }
    bool IsVehicle() const override { return false; }
    bool IsLeaveReverse() const override { return true; }
    void OnArrived() override {}
    void OnDespawning() override {}
    
    static void BuildWalkPath(const GSCoord& wp1, const GSCoord& wp2, GSSimObj& simObj);

    CountType m_countTP;
};
}
