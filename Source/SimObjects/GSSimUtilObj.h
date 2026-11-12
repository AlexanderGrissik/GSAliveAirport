// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "../General/GSAirport.h"

namespace NS_GSAliveAirport
{

class GSAirport;

class GSSimUtilObj : public GSSimObj
{
public:
    GSSimUtilObj(GSSimConnect& simHandle, const GSAirport& airport, GSSimObjUpdate& parent):
        GSSimObj(simHandle, parent), m_airport(airport) {}
    ~GSSimUtilObj() override = default;

    bool OnCreated() override;
    void Spawn() override;
    void Despawn() override;
    void OnArrival() override;
    void OnObjCreated() override {}

    void DespawnCont();

protected:
    void PrepareInitPos();
    void PrepareNextDestRoute();

    const GSRoadsNetwork::RoadNode* m_lastDest = nullptr;
    bool m_despawnRequested = false;
    const GSAirport& m_airport;
};

} // namespace NS_GSAliveAirport
