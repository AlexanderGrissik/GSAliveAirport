// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObjReq.h"

namespace NS_GSAliveAirport
{

class GSReqSimObjDelete : public GSSimObjReq {
public:
    using GSSimObjReq::GSSimObjReq;

    GSReqRC Process() override;
    bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;
};

}
