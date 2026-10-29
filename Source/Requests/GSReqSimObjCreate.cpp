// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqSimObjCreate.h"
#include "../SimObjects/GSSimObj.h"
#include "../General/GSSimConnect.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqSimObjCreate::Process() {
    auto id = m_simHandle.NextRequestID();
    GSReqRC rc = {
        m_simHandle.InvokeRequest(id, SimConnect_AICreateSimulatedObject_EX1, m_simObj.GetTitle().c_str(), "", m_simObj.GetInitPos(), id), true };
    
    if (!rc.isOK()) {
        GSLogStream::LogError("GSReqCreate::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
        m_simObj.OnObjSpawned(false);
    } 

    return rc;
}

bool GSReqSimObjCreate::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) {
    if (message->dwID == SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID) {
        auto* msg = static_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID*>(message);
        m_simObj.SetSimObjectID(msg->dwObjectID);
        m_simObj.RestoreInitPos();
        if (m_simObj.OnCreated()) {
            m_simObj.ContinueSpawn();
        }
    } else {
        m_simObj.OnObjSpawned(false);
        GSLogStream::LogError("GSReqCreate::OnMessage Unexpected Message: ") << message->dwID;
    }
    (void)messageSize;
    return true;
}

void GSReqSimObjCreate::OnException(SIMCONNECT_RECV_EXCEPTION *message) {
    m_simObj.OnObjSpawned(false);
    GSLogStream::LogError("GSReqSimObjCreate::OnException: ") << message->dwException << ", " << message->dwIndex;
}

}