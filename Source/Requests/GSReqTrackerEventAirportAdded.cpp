// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqTrackerEventAirportAdded.h"
#include "../General/GSAircraftTrackerThread.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

bool GSReqTrackerEventAirportAdded::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    GSAircraftTrackerThread &tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);

    if (!message || message->dwID != SIMCONNECT_RECV_ID_AIRPORT_LIST) {
        GSLogStream::LogError("Unexpected message: ") << (message ? message->dwID : -1) << ", Size: " << messageSize;
        return false;
    }

    tracker.HandleScanAirportMessage(*reinterpret_cast<SIMCONNECT_RECV_AIRPORT_LIST *>(message));
    return false;
}

void GSReqTrackerEventAirportAdded::OnException(SIMCONNECT_RECV_EXCEPTION *message)
{
    GSLogStream::LogError("GSReqTrackerEventAirportAdded Exception: ") << message->dwID <<
        ", Exception: " << message->dwException << "Index:" << message->dwIndex;
}

}
