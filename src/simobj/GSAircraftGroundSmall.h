#pragma once

#include "GSAircraftGround.h"

namespace NS_GSLiveAirportMSFS
{

class GSAircraftGroundSmall : public GSAircraftGround
{
public:
    GSAircraftGroundSmall(const GSAircraft& aircraft, GSSimConnect& simConn)
        : GSAircraftGround(aircraft, simConn) {}
};

} // namespace NS_GSLiveAirportMSFS
