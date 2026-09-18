#pragma once

#include "GSAircraftGround.h"

namespace NS_GSLiveAirportMSFS
{

class GSAircraftGroundSmall : public GSAircraftGround
{
public:
    using GSAircraftGround::GSAircraftGround;

    void BuildObjs() override;
};

} // namespace NS_GSLiveAirportMSFS
