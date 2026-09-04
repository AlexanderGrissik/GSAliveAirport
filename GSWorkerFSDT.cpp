#include "GSWorkerFSDT.h"

#include <utility>

namespace parking_services
{
GSWorkerFSDT::GSWorkerFSDT(AircraftSnapshot aircraft,
                           GroundServiceObject object,
                           GroundServiceLocation location)
    : GSStaticObj(std::move(aircraft), std::move(object), std::move(location))
{
}
} // namespace parking_services
