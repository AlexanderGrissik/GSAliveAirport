// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSRequest.h"

namespace NS_GSAliveAirport
{

class GSReqTrackerSpawnAircraft : public GSRequest {
public:
    GSReqTrackerSpawnAircraft(GSSimConnect& simHandle, int t):
        GSRequest(simHandle), m_type(t) {}
    GSReqRC Process() override;
    bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
    void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;
private:
    SIMCONNECT_DATA_INITPOSITION m_pos{};
    int m_type;
};

}
