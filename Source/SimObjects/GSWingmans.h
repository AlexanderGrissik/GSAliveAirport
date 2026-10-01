// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include <cstddef>

namespace NS_GSAliveAirport
{

class GSWingmans : public GSSimObj
{
public:

    enum CountType{
        AIRLINE,
        SMALL
    };

    GSWingmans(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, CountType tp):
        GSSimObj(simHandle, aircraft, parent), m_countTP(tp) {}

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override {}
    void OnDespawning() override {}
    
    static void BuildWalkPath(const GSCoord& wp1, const GSCoord& wp2, GSSimObj& simObj);

    CountType m_countTP;
};
}
