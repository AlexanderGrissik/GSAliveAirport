// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqTrackerEventAirportRemoved.h"
#include "../General/GSAircraftTrackerThread.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

bool GSReqTrackerEventAirportRemoved::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_AIRPORT_LIST) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        return false;
    }

    tracker.HandleScanAirportRemovedMessage(*reinterpret_cast<SIMCONNECT_RECV_AIRPORT_LIST *>(message));
    return false;
}

void GSReqTrackerEventAirportRemoved::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSLogStream::LogError("GSReqTrackerEventAirportRemoved Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

}
