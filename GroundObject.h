#pragma once

#include <cstdint>
#include <string>

namespace parking_services
{
using GroundObjectId = std::uint32_t;

struct GroundSnapshot
{
    GroundObjectId objectId{};
    std::string title;
    double latitude{};
    double longitude{};
    double groundSpeedKnots{};
};
} // namespace parking_services
