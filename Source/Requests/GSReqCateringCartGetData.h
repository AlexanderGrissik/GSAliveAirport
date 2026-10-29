// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSReqSimObjGetData.h"

namespace NS_GSAliveAirport
{

class GSReqCateringCartGetData : public GSReqSimObjGetData {
public:
    GSReqCateringCartGetData(GSSimConnect& simHandle, GSSimObj& simObj) : GSReqSimObjGetData(simHandle, simObj, GSDatumID_CateringTruckStateGet) {}
    bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
    void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;
};

}
