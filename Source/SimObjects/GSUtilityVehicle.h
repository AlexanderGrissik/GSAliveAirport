// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSSimUtilObj.h"

namespace NS_GSAliveAirport
{

class GSUtilityVehicle : public GSSimUtilObj
{
public:
    using GSSimUtilObj::GSSimUtilObj;
    ~GSUtilityVehicle() override = default;

    bool PreSpawn() override;
};

} // namespace NS_GSAliveAirport
