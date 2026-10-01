// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"

namespace NS_GSLiveAirportMSFS
{

class GSLavatoryTruck : public GSSimObj
{
public:

    using GSSimObj::GSSimObj;

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override {}
    
    bool m_hasAnimLavaratory = true;
};
}
