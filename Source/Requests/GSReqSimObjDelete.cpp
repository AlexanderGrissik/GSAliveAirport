// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqSimObjDelete.h"
#include "../SimObjects/GSSimObj.h"
#include "../General/GSSimConnect.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqSimObjDelete::Process() 
{
    auto id = m_simHandle.NextRequestID();
    auto simRC = m_simHandle.Invoke(SimConnect_AIRemoveObject, m_simObj.GetSimObjectID(), id);
    if (!simRC.isOK()) {
        GSLogStream::LogError("GSReqSimObjDelete::Process Failed call: ") << simRC.rc;
    } else {
        m_simObj.OnObjDespawned();
        //m_simObj.SetInProgress(true); // Unclear if SimConnect_AIRemoveObject fires OnMessage.
    }
    return {simRC, true};
}

bool GSReqSimObjDelete::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    (void)message;
    (void)messageSize;
    return true;
}

void GSReqSimObjDelete::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    (void)message;
}

}
