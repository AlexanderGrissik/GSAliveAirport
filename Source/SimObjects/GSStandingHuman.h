// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "General/GSCoord.h"

namespace NS_GSAliveAirport
{

class GSStandingHuman : public GSSimObj
{
public:

    enum HumanType {
        PILOT,
        PASSENGER
    };

    GSStandingHuman(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, HumanType humanType, const GSCoord& loc):
        GSSimObj(simHandle, aircraft, parent), m_humanType(humanType), m_loc(loc){}

    bool PreSpawn() override;

private:
        
    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return false; }
    bool IsArriveReverse() const override { return false; }
    bool IsVehicle() const override { return false; }
    bool IsLeaveReverse() const override { return true; }
    void OnArrived() override {}
    void OnDespawning() override {}

    HumanType m_humanType = PASSENGER;
    GSCoord m_loc;
};
}
