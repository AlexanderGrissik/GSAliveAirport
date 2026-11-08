// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimServObjReq.h"
#include "SimObjects/GSSimServObj.h"

namespace NS_GSAliveAirport
{

class GSReqSimObjContDespawn : public GSSimServObjReq {
public:
    using GSSimServObjReq::GSSimServObjReq;
    GSReqRC Process() override { m_simObj.DespawnCont(); return { {{0,0},NOERROR},false }; }
    bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override { return true; };
    void OnException(SIMCONNECT_RECV_EXCEPTION* message) override { };
};

}
