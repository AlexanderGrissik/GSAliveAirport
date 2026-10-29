// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSRequest.h"

namespace NS_GSAliveAirport
{

class GSSimConnect;
class GSSimObj;

class GSSimObjReq : public GSRequest {
public:
    GSSimObjReq(GSSimConnect& simHandle, GSSimObj& simObj): GSRequest(simHandle), m_simObj(simObj) {}
protected:
    GSSimObj& m_simObj;
};

}