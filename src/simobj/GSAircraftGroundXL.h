#pragma once

#include "GSAircraftGround.h"

namespace NS_GSLiveAirportMSFS
{

class GSAircraftGroundXL : public GSAircraftGround
{
public:
    GSAircraftGroundXL(const GSAircraft& aircraft, GSSimConnect& simConn)
        : GSAircraftGround(aircraft, simConn) {}
};

} // namespace NS_GSLiveAirportMSFS
