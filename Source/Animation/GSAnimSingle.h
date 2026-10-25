// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAnimationObject.h"

namespace NS_GSAliveAirport
{

class GSAnimSingle : public GSAnimationObject
{
public:

    #pragma pack(push, 1)
    struct StateWireDataSet
    {
        float val;
    };
    #pragma pack(pop)

    GSAnimSingle(SIMCONNECT_OBJECT_ID objID, const GSCoord& initCoord, GSDefinitions::GSDefID defID):
        GSAnimationObject(objID, initCoord, false), m_defID(defID) {}

    static void InitDatums(GSSimConnect& handler);

    void Animate(GSSimConnect& handler, float elapsedMilli) override;
    void OnNextWayPoint(std::size_t idx) override { VD(idx); }
    void AddFrameSet(const FrameRangeInfo& info);

private:

    GSAnimationObject::FrameRange m_frame;
    GSDefinitions::GSDefID m_defID;
};

}

