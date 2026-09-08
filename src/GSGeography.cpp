#include "GSGeography.h"

#include <cmath>

namespace NS_GSLiveAirportMSFS
{

SIMCONNECT_DATA_INITPOSITION GSGeography::RelativePosition(double headingDegrees, double longitude, double latitude, double altitudeFeet, double forwardMeters, double rightMeters)
{
    const double heading = headingDegrees * s_degreesToRadians;
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

double GSGeography::HeadingTowardRelativeOrigin(double referenceHeadingDegrees, double forwardMeters, double rightMeters)
{
    if (std::hypot(forwardMeters, rightMeters) < 0.001) { return NormalizeDegrees(referenceHeadingDegrees); }
    const double relativeHeading = std::atan2(-rightMeters, -forwardMeters) * s_radiansToDegrees;
    return NormalizeDegrees(referenceHeadingDegrees + relativeHeading);
}

double GSGeography::MetersPerDegreeLong(double latitude)
{
    return s_metersPerLatitudeDegree * std::cos(latitude * s_degreesToRadians);
}

double GSGeography::DistanceMeters(double latitudeA, double longitudeA, double latitudeB, double longitudeB)
{
    const double latitudeMeters = (latitudeB - latitudeA) * MetersPerDegreeLat();
    const double averageLatitude = (latitudeA + latitudeB) / 2.0;
    const double longitudeMeters = (longitudeB - longitudeA) * MetersPerDegreeLong(averageLatitude);
    return std::hypot(latitudeMeters, longitudeMeters);
}

} // namespace NS_GSLiveAirportMSFS
