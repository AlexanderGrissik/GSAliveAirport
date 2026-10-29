// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqTrackerAircraftsInfo.h"
#include "../General/GSAircraftTrackerThread.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqTrackerAircraftsInfo::Process()
{
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Starting Aircraft Const Info");

    auto id = m_simHandle.NextRequestID();
    GSReqRC rc = {
        m_simHandle.InvokeRequest(
            id, SimConnect_RequestDataOnSimObject, id, GSDatumID_AircraftConst, m_objectID, SIMCONNECT_PERIOD_ONCE, 0, 0, 0, 0), true };
    
    if (!rc.isOK()) {
        GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
        tracker.ClearFailedAircraft(m_objectID);
        GSLogStream::LogError("GSReqTrackerAircraftsInfo::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
    } 

    return rc;
}

bool GSReqTrackerAircraftsInfo::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        tracker.ClearFailedAircraft(m_objectID);
        return false;
    }

    tracker.HandleAircraftConstScan(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(message), m_airportInfo);
    return true;
}

void GSReqTrackerAircraftsInfo::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    tracker.ClearFailedAircraft(m_objectID);

    GSLogStream::LogError("GSReqTrackerAircraftsInfo:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

}
