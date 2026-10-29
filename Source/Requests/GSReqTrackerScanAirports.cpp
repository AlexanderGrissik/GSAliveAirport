// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqTrackerScanAirports.h"
#include "../General/GSAircraftTrackerThread.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqTrackerScanAirports::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSReqRC rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestFacilitiesList_EX1, SIMCONNECT_FACILITY_LIST_TYPE_AIRPORT, id), true };
    
    if (!rc.isOK()) {
        GSLogStream::LogError("GSReqTrackerScanAirports::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
    } 

    return rc;
}

bool GSReqTrackerScanAirports::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) 
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_AIRPORT_LIST) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        return false;
    }

    if (tracker.HandleScanAirportMessage(*reinterpret_cast<SIMCONNECT_RECV_AIRPORT_LIST *>(message))) {
        tracker.SetAirportScanInProgress(false);
        return true;
    }

    return false;
}
    
void GSReqTrackerScanAirports::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSAircraftTrackerThread& tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    tracker.SetAirportScanInProgress(false);
    GSLogStream::LogError("GSReqTrackerScanAirports:: Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

}
