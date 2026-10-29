// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "../General/GSDefinitions.h"
#include "../General/GSLogStream.h"
#include <array>
#include <utility>

namespace NS_GSAliveAirport
{

class GSCateringCart : public GSSimObj
{
public:

    #pragma pack(push, 1)
    struct StateWireDataGet
    {
        float doorContactOffsetZ;
    };
    struct StateWireDataSet
    {
        double posLong;
        double posLat;
        float elevationTarget;
        float openingTarget;
    };
    #pragma pack(pop)

    using GSSimObj::GSSimObj;

    static void InitDatums(GSSimConnect& handler);
    void SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry);
    
private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override {}
        
    float m_doorPosElev = 0.0f;


};
}
