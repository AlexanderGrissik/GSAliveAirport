// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimServObj.h"

namespace NS_GSAliveAirport
{

class GSPushback : public GSSimServObj
{
public:

    enum PushType {
        PUSH_DEFAULT,
        PUSH_SMALL,
        PUSH_XL
    };

    GSPushback(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, PushType tp):
        GSSimServObj(simHandle, aircraft, parent), m_pushType(tp) {}

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return true; }
    bool IsArriveReverse() const override { return false; }
    bool IsVehicle() const override { return true; }
    bool IsLeaveReverse() const override { return true; }
    void OnArrived() override;
    void OnDespawning() override {}
    
    PushType m_pushType;
};
}
