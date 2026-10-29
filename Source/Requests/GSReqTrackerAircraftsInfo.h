// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSRequest.h"
#include "../General/GSCoord.h"
#include <string>
#include <utility>

namespace NS_GSAliveAirport
{

class GSReqTrackerAircraftsInfo : public GSRequest {
public:
    GSReqTrackerAircraftsInfo(GSSimConnect& simHandle, DWORD objID, const std::pair<const std::string, GSCoord>& airportInfo):
        GSRequest(simHandle), m_objectID(objID), m_airportInfo(airportInfo) {}
    GSReqRC Process() override;
    bool OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    void OnException(SIMCONNECT_RECV_EXCEPTION *message) override;
private:
    DWORD m_objectID;
    std::pair<const std::string, GSCoord> m_airportInfo;
};

}
