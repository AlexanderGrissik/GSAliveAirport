// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "GSAircraftGround.h"

namespace NS_GSLiveAirportMSFS
{

class GSAircraftGroundMedium : public GSAircraftGround
{
public:
    using GSAircraftGround::GSAircraftGround;

    void BuildObjs() override;
};

} // namespace NS_GSLiveAirportMSFS
