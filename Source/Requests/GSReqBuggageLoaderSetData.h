// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSReqSimObjSetData.h"
#include "../SimObjects/GSBuggageLoader.h"
#include <array>
#include <utility>

namespace NS_GSAliveAirport
{

class GSReqBuggageLoaderSetData : public GSReqSimObjSetData<GSBuggageLoader::StateWireDataSet, 1> {
public:
    GSReqBuggageLoaderSetData(GSSimConnect& simHandle, GSSimObj& simObj, std::array<GSBuggageLoader::StateWireDataSet, 1>&& data) :
        GSReqSimObjSetData<GSBuggageLoader::StateWireDataSet, 1>(
            simHandle, simObj, GSDatumID_BuggageLoaderExtStateSet,
            std::forward<std::array<GSBuggageLoader::StateWireDataSet, 1>>(data)) {
    }
};

}
