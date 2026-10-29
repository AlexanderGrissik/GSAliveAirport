// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObjReq.h"

namespace NS_GSAliveAirport
{

class GSReqSimObjTxClientEventEX1 : public GSSimObjReq {
public:
    GSReqSimObjTxClientEventEX1(
        GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_CLIENT_EVENT_ID eventID,
        SIMCONNECT_OBJECT_ID objID, DWORD data0, DWORD data1):
        GSSimObjReq(simHandle, simObj), m_eventID(eventID), m_data0(data0), m_data1(data1), m_objID(objID) {}
    GSReqRC Process() override;
    bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override { 
        (void)message;
        (void)messageSize;
        return true; 
    }
    void OnException(SIMCONNECT_RECV_EXCEPTION *message) override { (void)message; }
private:
    SIMCONNECT_CLIENT_EVENT_ID m_eventID;
    DWORD m_data0;
    DWORD m_data1;
    SIMCONNECT_OBJECT_ID m_objID;
};

}
