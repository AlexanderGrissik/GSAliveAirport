// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqSimObjGetData.h"
#include "SimObjects/GSSimObj.h"

namespace NS_GSAliveAirport
{

GSInvokeRC GSReqSimObjGetData::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSInvokeRC rc = m_simHandle.InvokeRequest(
            id, SimConnect_RequestDataOnSimObject, id, m_definitionID, m_simObj.GetSimObjectID(), SIMCONNECT_PERIOD_ONCE, 0U, 0U, 0U, 0U);
    
    if (!rc.isOK()) {
        GSLogStream::LogError("GSReqGetDataSimObj::Process Failed call: ") << rc.rc;
        m_simObj.OnObjSpawned(false);
    }

    return rc;
}

}