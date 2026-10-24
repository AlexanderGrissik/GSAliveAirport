// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"

namespace NS_GSAliveAirport
{

class GSPaxBus : public GSSimObj
{
public:

    GSPaxBus(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent,
             const GSCoord& targetPos, double targetHeading, bool& isArrived):
        GSSimObj(simHandle, aircraft, parent), m_targetPos(targetPos),
        m_targetHeading(targetHeading), m_isArrived(isArrived) {}

    bool PreSpawn() override;

private:

    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override {}

    GSCoord m_targetPos;
    double m_targetHeading;
    bool& m_isArrived;
};

} // namespace NS_GSAliveAirport
