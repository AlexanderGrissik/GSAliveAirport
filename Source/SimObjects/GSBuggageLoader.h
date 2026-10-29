// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "../General/GSDefinitions.h"
#include "../General/GSLogStream.h"
#include <array>
#include <cstddef>
#include <utility>

namespace NS_GSAliveAirport
{

class GSBuggageLoader : public GSSimObj
{
public:

    #pragma pack(push, 1)
    struct StateWireDataGet
    {
        float endRampYFeet;
        float endRampZMeters;
        float pivotYFeet;
        float pivotZMeters;
    };
    struct StateWireDataSet
    {
        float targetAngleDegrees;
    };
    #pragma pack(pop)

    GSBuggageLoader(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent, bool front) :
        GSSimObj(simHandle, aircraft, parent), m_front(front) {}

    static void InitDatums(GSSimConnect& handler);

    void SetFinalPositionAndState(SIMCONNECT_RECV_SIMOBJECT_DATA& entry);

private:

    bool PreSpawn() override;
    bool OnCreated() override;
    void OnArrived() override;
    void OnDespawning() override;
    
    float CalcLoaderOpenAngle(const GSBuggageLoader::StateWireDataGet& rawStateGet);

    float m_doorPosElev = 0.0f;
    DWORD m_doorIdx = 0;
    bool m_front = false;
    bool m_ext = true;


};
}
