// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObjReq.h"
#include "../SimObjects/GSSimObj.h"

namespace NS_GSAliveAirport
{

class GSReqSimObjContDespawn : public GSSimObjReq {
public:
    using GSSimObjReq::GSSimObjReq;
    GSReqRC Process() override { m_simObj.Despawn(); return { {0,0,NOERROR},false }; }
    bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override { VD(message); VD(messageSize); return true; };
    void OnException(SIMCONNECT_RECV_EXCEPTION* message) override { VD(message); };
};

}
