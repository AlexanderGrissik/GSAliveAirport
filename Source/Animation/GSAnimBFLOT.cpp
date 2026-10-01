// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAnimBFLOT.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_AnimWagonBFLOT{
    GSDefinitions::DatumSpec{"WAGON BACK LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"WAGON FRONT LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"PLANE TOUCHDOWN HEADING DEGREES TRUE", "number", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSAnimBFLOT::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_AnimWagonBFLOT, GSDefinitions::GSDefID::GSDefID_AnimWagonBFLOT);
}

void GSAnimBFLOT::AddFrameSet(
    float startFrameB, float endFrameB, float fpsB, float startFrameF, float endFrameF, float fpsF,
    float startFrameT, float endFrameT, float fpsT)
{
    m_BLO = { startFrameB, endFrameB, fpsB, startFrameB, std::abs(endFrameB - startFrameB) };
    m_FLO = { startFrameF, endFrameF, fpsF, startFrameF, std::abs(endFrameF - startFrameF) };
    m_TDW = { startFrameT, endFrameT, fpsT, startFrameT, std::abs(endFrameT - startFrameT) };
}

void GSAnimBFLOT::Animate(GSSimConnect& handler, float elapsedMilli)
{
    CalcNextFrame(m_BLO, elapsedMilli);
    CalcNextFrame(m_FLO, elapsedMilli);
    CalcNextFrame(m_TDW, elapsedMilli);

    StateWireDataSet valSet { m_BLO.m_currFrame, m_FLO.m_currFrame, m_TDW.m_currFrame };
    auto simRC = handler.Invoke(
        SimConnect_SetDataOnSimObject, GSDefinitions::GSDefID::GSDefID_AnimWagonBFLOT, m_simObjectID,
        0, 1, static_cast<DWORD>(sizeof(valSet)), &valSet);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSAnimBFLOT::Animate Failed SimConnect_SetDataOnSimObject call: ") << simRC.rc;
    }
}

}
