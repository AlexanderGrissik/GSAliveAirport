#include "SimObjectPositioning.h"

#include "Aircraft.h"

#include <cmath>

namespace parking_services
{
SIMCONNECT_DATA_INITPOSITION RelativePosition(
    double headingDegrees, double longitude, double latitude, double altitudeFeet,
    double forwardMeters, double rightMeters)
{
    constexpr double degreesToRadians = 3.14159265358979323846 / 180.0;
    const double heading = headingDegrees * degreesToRadians;
    const double northMeters = forwardMeters * std::cos(heading) - rightMeters * std::sin(heading);
    const double eastMeters = forwardMeters * std::sin(heading) + rightMeters * std::cos(heading);
    const double metersPerDegree = MetersPerDegree(latitude);

    SIMCONNECT_DATA_INITPOSITION position{};
    position.Latitude = latitude + northMeters / metersPerDegree;
    position.Longitude = longitude + eastMeters / metersPerDegree;
    position.Altitude = altitudeFeet;
    position.Heading = headingDegrees;
    position.OnGround = 1;
    return position;
}
} // namespace parking_services
