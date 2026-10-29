// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../General/GSDefinitions.h"

namespace NS_GSAliveAirport {

class GSSimConnect;

class GSRequest {
public:
    GSRequest(GSSimConnect& simHandle): m_simHandle(simHandle) {}
    virtual ~GSRequest() {}
    virtual GSReqRC Process() = 0;
    virtual bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) = 0;
    virtual void OnException(SIMCONNECT_RECV_EXCEPTION *message) = 0;
protected:
    GSSimConnect& m_simHandle;
};

using GSRequestU = std::unique_ptr<GSRequest>;
using GSRequestR = std::reference_wrapper<GSRequest>;

}
