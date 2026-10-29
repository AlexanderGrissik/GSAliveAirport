// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSReqSimObjSetData.h"
#include "../SimObjects/GSStaircase.h"
#include <array>
#include <utility>

namespace NS_GSAliveAirport
{

class GSReqStaircaseSetData : public GSReqSimObjSetData<GSStaircase::StateWireDataSet, 1> {
public:
    GSReqStaircaseSetData(GSSimConnect& simHandle, GSSimObj& simObj, std::array<GSStaircase::StateWireDataSet, 1>&& data) :
        GSReqSimObjSetData<GSStaircase::StateWireDataSet, 1>(
            simHandle, simObj, GSDatumID_StaircaseExtStateSet,
            std::forward<std::array<GSStaircase::StateWireDataSet, 1>>(data)) {
    }
};

}
