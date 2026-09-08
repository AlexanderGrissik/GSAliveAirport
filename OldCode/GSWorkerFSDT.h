#pragma once

#include "GSStaticObj.h"

namespace parking_services
{
// A stationary FSDT worker. Placement is identical to GSStaticObj, while its
// VELOCITY BODY Y animation carrier is written together with the fixed pose so
// MSFS does not collapse the carrier back to zero.
class GSWorkerFSDT final : public GSStaticObj
{
  public:
    GSWorkerFSDT(AircraftSnapshot aircraft, GroundServiceObject object,
                 GroundServiceLocation location);

    [[nodiscard]] bool UsesPositionedVelocityAnimation() const override
    {
        return true;
    }
};
} // namespace parking_services
