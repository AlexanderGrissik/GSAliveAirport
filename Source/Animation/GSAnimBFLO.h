// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAnimationObject.h"

namespace NS_GSAliveAirport
{

class GSAnimBFLO : public GSAnimationObject
{
public:

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float blo;
        float flo;
    };
    #pragma pack(pop)

    GSAnimBFLO(SIMCONNECT_OBJECT_ID objID) : GSAnimationObject(objID) {}

    static void InitDatums(GSSimConnect& handler);

    void Animate(GSSimConnect& handler, float elapsedMilli) override;
    void OnNextWayPoint(std::size_t idx) override { VD(idx); }
    void AddFrameSet(const FrameRangeInfo& infoB, const FrameRangeInfo& infoF);

private:

    GSAnimationObject::FrameRange m_BLO;
    GSAnimationObject::FrameRange m_FLO;
};

}

