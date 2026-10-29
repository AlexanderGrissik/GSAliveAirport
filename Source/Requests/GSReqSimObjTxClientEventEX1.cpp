// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqSimObjTxClientEventEX1.h"
#include "../SimObjects/GSSimObj.h"
#include "../General/GSSimConnect.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqSimObjTxClientEventEX1::Process()
{
    auto simRC = m_simHandle.Invoke(
        SimConnect_TransmitClientEvent_EX1, m_objID, m_eventID,
        SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY, m_data0, m_data1, 0, 0, 0);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSReqSimObjTxClientEventEX1::Process Failed call: ") << simRC.rc;
    }

    return {simRC, false};
}

}
