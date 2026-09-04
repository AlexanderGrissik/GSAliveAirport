#pragma once

#include "GSObject.h"

namespace parking_services
{
class GSWalkerFSDT final : public GSObject
{
  public:
    GSWalkerFSDT(AircraftSnapshot aircraft, GroundServiceObject object,
                 GroundServiceLocation location);

    bool PreparePlacement(GSObjectServices &services) override;

  private:
    static constexpr double DefaultMovementSpeedMetersPerSecond =
        2.5 * 0.514444;
};
} // namespace parking_services
