// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqTrackerScanAircrafts.h"
#include "../General/GSAircraftTrackerThread.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqTrackerScanAircrafts::Process()
{
    GSLogStream::LogDebug(GSLogStream::DBG_LOG_AIRCRAFT_TRACKER, "Starting Aircraft Scan");

    auto id = m_simHandle.NextRequestID();
    GSReqRC rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestDataOnSimObjectType, id, GSDatumID_Aircraft, GSAircraftTrackerThread::DiscoveryRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT), true };
    
    if (!rc.isOK()) {
        GSLogStream::LogError("GSReqTrackerScanAircrafts::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
    } 

    return rc;
}

bool GSReqTrackerScanAircrafts::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        return false;
    }

    return tracker.HandleScanMessage(*reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message));
}

void GSReqTrackerScanAircrafts::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    GSLogStream::LogError("GSReqTrackerScanAircrafts:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
    
    tracker.SetInProgress(false);
    tracker.HandleRemoved();
}

}
