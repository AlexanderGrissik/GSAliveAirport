// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAnimMulti.h"
#include "../General/GSLogStream.h"
#include <algorithm>
#include <cmath>

namespace NS_GSAliveAirport
{

void GSAnimMulti::OnNextWayPoint(std::size_t idx)
{
    m_activeFrameRange = std::min(idx, m_frameRanges.size() - 1);
}

void GSAnimMulti::Animate(GSSimConnect& handler, float elapsedMilli)
{
    Move(handler, elapsedMilli);

    CalcNextFrame(m_frameRanges[m_activeFrameRange], elapsedMilli);

    StateWireDataSet valSet { m_frameRanges[m_activeFrameRange].m_currFrame };
    auto simRC = handler.Invoke(
        SimConnect_SetDataOnSimObject, m_defID, m_simObjectID,
        0, 1, static_cast<DWORD>(sizeof(valSet)), &valSet);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSAnimMulti::Animate Failed SimConnect_SetDataOnSimObject call: ") << simRC.rc;
    }
}

}
