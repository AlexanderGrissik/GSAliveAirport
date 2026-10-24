// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObj.h"
#include "../General/GSCoord.h"
#include "../Animation/GSAnimationObject.h"

#include <array>
#include <string>
#include <unordered_map>

namespace NS_GSAliveAirport
{

class GSPassenger : public GSSimObj
{
public:

    GSPassenger(GSSimConnect& simHandle, GSAircraft& aircraft, GSSimObjUpdate& parent,
                const GSCoord& stairBasePos, const GSCoord& stairTopPos, const GSCoord& doorPos,
                float stairBaseHeading, float doorAltFeet, bool departing):
        GSSimObj(simHandle, aircraft, parent), m_stairBasePos(stairBasePos), m_stairTopPos(stairTopPos), m_doorPos(doorPos),
        m_stairBaseHeading(stairBaseHeading), m_doorAltFeet(doorAltFeet), m_departing(departing) {}

    bool PreSpawn() override;

    // Per passenger model title: [0] = walk.0 (longWalk) range,
    // [1] = walk.3 (stoop_insideAircraft / climb) range, [2] = walk.0 (longWalk) range. All frames @ 30 fps.
    static const std::unordered_map<std::string, std::array<GSAnimationObject::FrameRangeInfo, 3>> s_ExtPassengerFrames;

private:

    bool OnCreated() override;
    void OnArrived() override {}
    void OnDespawning() override {}

    void BuildWalkPath();

    GSCoord m_stairBasePos;
    GSCoord m_stairTopPos;
    GSCoord m_doorPos;
    GSCoord m_busPos;
    float m_doorAltFeet = 0.0f;
    float m_stairBaseHeading = 0.0f;
    bool m_dynamic = true;
    bool m_departing = true;
};

}
