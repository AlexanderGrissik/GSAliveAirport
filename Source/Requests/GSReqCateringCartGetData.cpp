// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqCateringCartGetData.h"
#include "../SimObjects/GSCateringCart.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

bool GSReqCateringCartGetData::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) {
    (void)messageSize;
    if (message->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        static_cast<GSCateringCart&>(m_simObj).SetFinalPositionAndState(*static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message));
    } else {
        m_simObj.OnObjSpawned(true);
        GSLogStream::LogError("GSReqCateringCartGetData::OnMessage Unexpected Message: ") << message->dwID;
    }
    return true;
}

void GSReqCateringCartGetData::OnException(SIMCONNECT_RECV_EXCEPTION *message) {
    m_simObj.OnObjSpawned(true);
    GSLogStream::LogError("GSReqCateringCartGetData::OnException: ") << message->dwException << ", " << message->dwIndex;
}

}
