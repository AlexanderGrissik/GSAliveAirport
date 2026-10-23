// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAnimBFLO.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

constexpr std::array GSDatums_AnimWagonBFLO{
    GSDefinitions::DatumSpec{"WAGON BACK LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
    GSDefinitions::DatumSpec{"WAGON FRONT LINK ORIENTATION", "number", SIMCONNECT_DATATYPE_FLOAT32},
};

void GSAnimBFLO::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_AnimWagonBFLO, GSDefinitions::GSDefID::GSDefID_AnimWagonBFLO);
}

void GSAnimBFLO::AddFrameSet(const FrameRangeInfo& infoB, const FrameRangeInfo& infoF)
{
    m_BLO = infoB;
    m_FLO = infoF;
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
