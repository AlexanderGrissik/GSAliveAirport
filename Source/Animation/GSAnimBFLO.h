// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAnimationObject.h"

namespace NS_GSLiveAirportMSFS
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
    void AddFrameSet(float startFrameB, float endFrameB, float fpsB, float startFrameF, float endFrameF, float fpsF);

private:

    GSAnimationObject::FrameRange m_BLO;
    GSAnimationObject::FrameRange m_FLO;
};

}

