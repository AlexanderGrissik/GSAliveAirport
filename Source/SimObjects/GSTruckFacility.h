// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimServObj.h"
#include <cstddef>

namespace NS_GSAliveAirport
{

class GSTruckFacility : public GSSimServObj
{
public:

    using GSSimServObj::GSSimServObj;

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return true; }
    bool IsArriveReverse() const override { return false; }
    bool IsVehicle() const override { return true; }
    bool IsLeaveReverse() const override { return true; }
    void OnArrived() override;
    void OnDespawning() override {}
};
}
