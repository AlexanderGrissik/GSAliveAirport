// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSRequest.h"

namespace NS_GSAliveAirport
{

class GSSimServObj;

class GSSimServObjReq : public GSRequest
{
public:
    GSSimServObjReq(GSSimConnect& simHandle, GSSimServObj& simObj):
        GSRequest(simHandle), m_simObj(simObj) {}

protected:
    GSSimServObj& m_simObj;
};

} // namespace NS_GSAliveAirport
