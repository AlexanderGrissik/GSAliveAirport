// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "GSBuggageLoader.h"
#include <cstddef>

namespace NS_GSAliveAirport
{

class GSBuggageTrain : public GSSimObj
{
public:

    GSBuggageTrain(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, GSBuggageLoader& loader) :
        GSSimObj(simHandle, aircraft, parent), m_loader(loader) {}

    bool PreSpawn() override;

private:
        
    bool OnCreated() override;
    void OnArrived() override {}
    void OnDespawning() override {}

    GSBuggageLoader& m_loader;
};
}
