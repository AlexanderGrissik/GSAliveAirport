// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSReqSimObjSetData.h"
#include "../SimObjects/GSGroundPower.h"
#include <array>
#include <utility>

namespace NS_GSAliveAirport
{

class GSReqGroundPowerSetData : public GSReqSimObjSetData<GSGroundPower::StateWireDataSet, 1> {
public:
    GSReqGroundPowerSetData(
        GSSimConnect& simHandle, GSSimObj& simObj, SIMCONNECT_DATA_DEFINITION_ID definitionID, 
        std::array<GSGroundPower::StateWireDataSet, 1>&& data): 
        GSReqSimObjSetData<GSGroundPower::StateWireDataSet, 1>(
            simHandle, simObj, definitionID, std::forward<std::array<GSGroundPower::StateWireDataSet, 1>>(data)) {}
};

}
