// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqTrackerAirportInfo.h"
#include "General/GSAircraftTrackerThread.h"
#include "General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqTrackerAirportInfo::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSReqRC rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestFacilityData, GSDatumID_AirportInfo, id, m_airportInfo.first.c_str(), ""), true};
    
    if (!rc.isOK()) {
        GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
        tracker.ClearFailedAircraft(m_objectID);
        GSLogStream::LogError("GSReqTrackerAirportInfo::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
    } 

    return rc;
}

bool GSReqTrackerAirportInfo::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    GSAircraftTrackerThread& tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    if (message->dwID != SIMCONNECT_RECV_ID_FACILITY_DATA && message->dwID != SIMCONNECT_RECV_ID_FACILITY_DATA_END) {
        GSLogStream::LogError("GSReqTrackerAirportInfo unexpected message: ") << message->dwID;
        tracker.ClearFailedAircraft(m_objectID);
        return true;
    }

    tracker.HandleAirportInfoMessage(*message, m_objectID, m_airportInfo);
    return message->dwID == SIMCONNECT_RECV_ID_FACILITY_DATA_END;
}

void GSReqTrackerAirportInfo::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    tracker.ClearFailedAircraft(m_objectID);
    GSLogStream::LogError("GSReqTrackerAirportInfo request failed: ") << message->dwException;
}

}
