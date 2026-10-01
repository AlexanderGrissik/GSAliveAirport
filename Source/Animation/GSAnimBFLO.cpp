// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAnimBFLO.h"
#include "../General/GSLogStream.h"

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_AnimWagonBFLO{
    GSDefinitions::DatumSpec{"WAGON BACK LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"WAGON FRONT LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSAnimBFLO::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_AnimWagonBFLO, GSDefinitions::GSDefID::GSDefID_AnimWagonBFLO);
}

void GSAnimBFLO::AddFrameSet(float startFrameB, float endFrameB, float fpsB, float startFrameF, float endFrameF, float fpsF)
{
    m_BLO = { startFrameB, endFrameB, fpsB, startFrameB, std::abs(endFrameB - startFrameB) };
    m_FLO = { startFrameF, endFrameF, fpsF, startFrameF, std::abs(endFrameF - startFrameF) };
}

void GSAnimBFLO::Animate(GSSimConnect& handler, float elapsedMilli)
{
    CalcNextFrame(m_BLO, elapsedMilli);
    CalcNextFrame(m_FLO, elapsedMilli);

    StateWireDataSet valSet { m_BLO.m_currFrame, m_FLO.m_currFrame };
    auto simRC = handler.Invoke(
        SimConnect_SetDataOnSimObject, GSDefinitions::GSDefID::GSDefID_AnimWagonBFLO, m_simObjectID,
        0, 1, static_cast<DWORD>(sizeof(valSet)), &valSet);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSAnimBFLO::Animate Failed SimConnect_SetDataOnSimObject call: ") << simRC.rc;
    }
}

}
