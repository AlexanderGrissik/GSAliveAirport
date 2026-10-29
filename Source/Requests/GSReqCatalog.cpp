// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqCatalog.h"
#include "../General/GSCatalog.h"
#include "../General/GSLogStream.h"

namespace NS_GSAliveAirport
{

GSReqRC GSReqCatalog::Process()
{
    const DWORD requestId = m_simHandle.NextRequestID();
    GSReqRC rc = {
        m_simHandle.InvokeRequest(requestId, SimConnect_EnumerateSimObjectsAndLiveries, requestId, SIMCONNECT_SIMOBJECT_TYPE_ALL), true };
    if (!rc.isOK()) {
        GSLogStream::LogError("GSReqCatalog::Process Failed call: ") << rc.rc;
        rc.keepReq = false;
        static_cast<GSCatalog&>(m_simHandle).Done();
    }

    return rc;
}

bool GSReqCatalog::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    (void)messageSize;
    const auto& entry = *reinterpret_cast<const SIMCONNECT_RECV_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST*>(message);
    if (entry.dwOutOf == 0 || message->dwID != SIMCONNECT_RECV_ID_ENUMERATE_SIMOBJECT_AND_LIVERY_LIST) { // Empty Scan
        static_cast<GSCatalog&>(m_simHandle).Done();
        GSLogStream::LogError("GSReqCatalog::OnMessage Empty Catalog. ID: ") << message->dwID << ", Count: " << entry.dwOutOf;
        return true;
    }

    auto &catalog = static_cast<GSCatalog&>(m_simHandle);
    for (DWORD index = 0; index < entry.dwArraySize; ++index) {
        catalog.AddEntry(entry.rgData[index].AircraftTitle, entry.rgData[index].LiveryName);
    }

    if (entry.dwOutOf == entry.dwEntryNumber + 1) {
        static_cast<GSCatalog&>(m_simHandle).Done();
        return true;
    }

    return false;
}

void GSReqCatalog::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    static_cast<GSCatalog&>(m_simHandle).Done();
    GSLogStream::LogError("GSReqCatalog::OnException Catalog Error. ID: ") << message->dwSendID << ", Err: " << message->dwException;
}

}
