#include "SimObjectPositioning.h"

#include "Aircraft.h"

#include <cmath>

namespace parking_services
{
double NormalizeDegrees(double degrees)
{
    return std::fmod(degrees + 360.0, 360.0);
}

SIMCONNECT_DATA_INITPOSITION RelativePosition(
    double headingDegrees, double longitude, double latitude, double altitudeFeet,
    double forwardMeters, double rightMeters)
{
    constexpr double degreesToRadians = 3.14159265358979323846 / 180.0;
    const double heading = headingDegrees * degreesToRadians;
    const double northMeters = forwardMeters * std::cos(heading) - rightMeters * std::sin(heading);
    const double eastMeters = forwardMeters * std::sin(heading) + rightMeters * std::cos(heading);

    SIMCONNECT_DATA_INITPOSITION position{};
    position.Latitude = latitude + northMeters / MetersPerDegreeLat();
    position.Longitude = longitude + eastMeters / MetersPerDegreeLong(latitude);
    position.Altitude = altitudeFeet;
    position.Heading = headingDegrees;
    position.OnGround = 1;
    return position;
}

double HeadingTowardRelativeOrigin(double referenceHeadingDegrees,
                                   double forwardMeters, double rightMeters)
{
    if (std::hypot(forwardMeters, rightMeters) < 0.001) {
        return NormalizeDegrees(referenceHeadingDegrees);
    }
    const double relativeHeading =
        std::atan2(-rightMeters, -forwardMeters) * kRadiansToDegrees;
    return NormalizeDegrees(referenceHeadingDegrees + relativeHeading);
}
} // namespace parking_services
