// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"

namespace NS_GSAliveAirport
{

class GSLavatoryTruck : public GSSimObj
{
public:

    using GSSimObj::GSSimObj;

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return true; }
    bool IsArriveReverse() const override { return true; }
    bool IsVehicle() const override { return true; }
    bool IsLeaveReverse() const override { return false; }
    void OnArrived() override;
    void OnDespawning() override {}
    
};
}
