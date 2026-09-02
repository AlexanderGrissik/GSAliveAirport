#pragma once

#include "GSObject.h"

namespace parking_services
{
// A walking worker (GroundServiceSpecialType::WalkerFSDT). It owns the
// "walking" placement specifics: the object is placed at a route start point
// and given a route to follow (driving the walking animation), as opposed to the
// static placement a plain GSObject uses.
class GSWalker final : public GSObject
{
  public:
    GSWalker(std::uint64_t token, AircraftSnapshot aircraft,
             GroundServiceObject object, GroundServiceLocation location);

    bool PreparePlacement(GSObjectServices &services) override;
};
} // namespace parking_services