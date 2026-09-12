#pragma once

#include "GSDefinitions.h"

namespace NS_GSLiveAirportMSFS {

class GSSimConnect;

class GSRequest {
public:
    struct SendResult {
        GSDefinitions::SendResult m_simRC;
        bool m_keep;
    };

    GSRequest(GSSimConnect& simHandle): m_simHandle(simHandle) {}
    virtual ~GSRequest() {}
    virtual SendResult Process() = 0;
    virtual bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) = 0;
    virtual void OnException(SIMCONNECT_RECV_EXCEPTION *message) = 0;
protected:
    GSSimConnect& m_simHandle;
};

}
