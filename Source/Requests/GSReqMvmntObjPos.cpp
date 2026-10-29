// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqMvmntObjPos.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqMvmntObjPos::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSReqRC rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestDataOnSimObject, id, GSDatumID_PlanePosition,
            m_simObj.GetSimObjectID(), SIMCONNECT_PERIOD_ONCE, 0U, 0U, 0U, 0U), true };

    if (!rc.isOK()) {
        GSLogStream::LogError("GSReqMvmntObjPos::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
    }

    return rc;
}

bool GSReqMvmntObjPos::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    GSMovementThread& tracker = static_cast<GSMovementThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
    } else {
        tracker.HandlePosMessage(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message), *m_track);
    }

    return true;
}

void GSReqMvmntObjPos::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSMovementThread& tracker = static_cast<GSMovementThread&>(m_simHandle);

    GSLogStream::LogError("GSReqMvmntObjPos:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;

    tracker.DecrInflight();
}

}
