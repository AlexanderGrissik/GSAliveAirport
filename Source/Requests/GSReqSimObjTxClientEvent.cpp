// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqSimObjTxClientEvent.h"
#include "../SimObjects/GSSimObj.h"
#include "../General/GSSimConnect.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqSimObjTxClientEvent::Process()
{
    auto simRC = m_simHandle.Invoke(
        SimConnect_TransmitClientEvent, m_simObj.GetSimObjectID(), m_eventID, m_data, 
        SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSReqSimObjTxClientEvent::Process Failed call: ") << simRC.rc;
    }

    return {simRC, false};
}

}
