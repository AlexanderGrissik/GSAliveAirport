// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqStaircaseGetData.h"
#include "../SimObjects/GSStaircase.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

bool GSReqStaircaseGetData::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) {
    (void)messageSize;
    if (message->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        static_cast<GSStaircase&>(m_simObj).SetFinalPositionAndState(*static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message));
    } else {
        GSLogStream::LogError("GSReqStaircaseGetData::OnMessage Unexpected Message: ") << message->dwID;
        m_simObj.OnObjSpawned(true);
    }
    return true;
}

void GSReqStaircaseGetData::OnException(SIMCONNECT_RECV_EXCEPTION* message) {
    m_simObj.OnObjSpawned(true);
    GSLogStream::LogError("GSReqStaircaseGetData::OnException: ") << message->dwException << ", " << message->dwIndex;
}

}
