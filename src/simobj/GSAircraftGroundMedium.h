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
