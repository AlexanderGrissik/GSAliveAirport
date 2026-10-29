// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimObjReq.h"
#include "../General/GSDefinitions.h"

namespace NS_GSAliveAirport
{

class GSReqSimObjGetData : public GSSimObjReq {
public:
    GSReqSimObjGetData(GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_DATA_DEFINITION_ID definitionID):
        GSSimObjReq(simHandle, simObj), m_definitionID(definitionID) {}
    GSReqRC Process() override;
private:
    SIMCONNECT_DATA_DEFINITION_ID m_definitionID;
};

}