// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"

namespace NS_GSAliveAirport
{

class GSPushback : public GSSimObj
{
public:

    enum PushType {
        PUSH_DEFAULT,
        PUSH_SMALL,
        PUSH_XL
    };

    GSPushback(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, PushType tp):
        GSSimObj(simHandle, aircraft, parent), m_pushType(tp) {}

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override {}
    
    PushType m_pushType;
};
}
