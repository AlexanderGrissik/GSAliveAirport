// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSReqSimObjSetData.h"
#include "../SimObjects/GSCateringCart.h"
#include <array>
#include <utility>

namespace NS_GSAliveAirport
{

class GSReqCateringCartSetData : public GSReqSimObjSetData<GSCateringCart::StateWireDataSet, 1> {
public:
    GSReqCateringCartSetData(GSSimConnect& simHandle, GSSimObj& simObj, std::array<GSCateringCart::StateWireDataSet, 1>&& data): 
        GSReqSimObjSetData<GSCateringCart::StateWireDataSet, 1>(
            simHandle, simObj, GSDatumID_CateringTruckStateSet,
            std::forward<std::array<GSCateringCart::StateWireDataSet, 1>>(data)) {}
};

}
