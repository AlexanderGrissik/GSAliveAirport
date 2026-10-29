// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObjReq.h"

namespace NS_GSAliveAirport
{

class GSReqSimObjTxClientEvent : public GSSimObjReq {
public:
    GSReqSimObjTxClientEvent(GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_CLIENT_EVENT_ID eventID, DWORD data):
        GSSimObjReq(simHandle, simObj), m_eventID(eventID), m_data(data) {}
    GSReqRC Process() override;
    bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override { 
        (void)message;
        (void)messageSize;
        return true; 
    }
    void OnException(SIMCONNECT_RECV_EXCEPTION *message) override { (void)message; }
private:
    SIMCONNECT_CLIENT_EVENT_ID m_eventID;
    DWORD m_data;
};

}
