// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAnimSingle.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_AnimVelocY{
    GSDatum{"VELOCITY BODY Y", "number", SIMCONNECT_DATATYPE_FLOAT32},
};

constexpr std::array GSDatums_AnimWagonBLO{
    GSDatum{"WAGON BACK LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
};


void GSAnimSingle::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_AnimVelocY, GSDatumID_AnimVelocBodyY);
    handler.InvokeAddDatums(GSDatums_AnimWagonBLO, GSDatumID_AnimWagonBLO);
}

void GSAnimSingle::AddFrameSet(const FrameRangeInfo& info)
{
    m_frame = info;
}

void GSAnimSingle::Animate(GSSimConnect& handler, float elapsedMilli)
{
    CalcNextFrame(m_frame, elapsedMilli);
    
    StateWireDataSet valSet { m_frame.m_currFrame };
    auto simRC = handler.Invoke(
        SimConnect_SetDataOnSimObject, m_defID, m_simObjectID,
        0, 1, static_cast<DWORD>(sizeof(valSet)), &valSet);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSAnimSingle::Animate Failed SimConnect_SetDataOnSimObject call: ") << simRC.rc;
    }

    Move(handler, elapsedMilli);
}

}
