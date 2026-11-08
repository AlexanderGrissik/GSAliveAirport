// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimServObj.h"
#include "General/GSDefinitions.h"
#include "General/GSLogStream.h"
#include <array>
#include <utility>

namespace NS_GSAliveAirport
{

class GSCateringCart : public GSSimServObj
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

    using GSSimServObj::GSSimServObj;

    static void InitDatums(GSSimConnect& handler);
    void SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry);
    
private:

    bool PreSpawn() override;
    bool OnCreated() override;
    bool IsDelayedSpawnable() const override { return true; }
    bool IsArriveReverse() const override { return false; }
    bool IsVehicle() const override { return true; }
    bool IsLeaveReverse() const override { return true; }
    void OnArrived() override;
    void OnDespawning() override;
        
    float m_doorPosElev = 0.0f;
    DWORD m_doorIdx = 0;


};
}
