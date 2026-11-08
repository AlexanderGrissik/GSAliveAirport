// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimServObj.h"
#include "GSBuggageLoader.h"
#include <cstddef>

namespace NS_GSAliveAirport
{

class GSBuggageTrain : public GSSimServObj
{
public:

    GSBuggageTrain(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, GSBuggageLoader& loader) :
        GSSimServObj(simHandle, aircraft, parent), m_loader(loader) {}

    bool PreSpawn() override;

private:
        
    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return false; }
    bool IsArriveReverse() const override { return false; }
    bool IsVehicle() const override { return true; }
    bool IsLeaveReverse() const override { return true; }
    void OnArrived() override {}
    void OnDespawning() override {}

    GSBuggageLoader& m_loader;
};
}
