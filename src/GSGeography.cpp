#include "GSGeography.h"

#include <cmath>

namespace NS_GSLiveAirportMSFS
{

double GSGeography::PI = std::acos(-1.0);
double GSGeography::DegToRad = GSGeography::PI / 180.0;
double GSGeography::RadToDeg = 180.0 / GSGeography::PI;
double GSGeography::MetersPerLatitudeDegree = 111'320.0;

double GSGeography::MetersPerDegreeLat()
{
    return MetersPerLatitudeDegree;
}

double GSGeography::MetersPerDegreeLong(double latitude)
{
    return MetersPerLatitudeDegree * std::cos(latitude * DegToRad);
}

double GSGeography::NormDeg(double degrees)
{
    while (degrees >= 360.0) degrees -= 360.0;
    while (degrees < 0.0) degrees += 360.0;
    return degrees;
}

double GSGeography::DistanceMeters(const GSCoord& coordA, const GSCoord& coordB)
{
    const double latitudeMeters = (coordB.Lat() - coordA.Lat()) * MetersPerDegreeLat();
    const double averageLatitude = (coordA.Lat() + coordB.Lat()) / 2.0;
    const double longitudeMeters = (coordB.Long() - coordA.Long()) * MetersPerDegreeLong(averageLatitude);
    return std::hypot(latitudeMeters, longitudeMeters);
}

GSCoord GSGeography::RepositionZOffset(const GSCoord& coord, double mainHeadingDegrees, double zOffsetMeters)
{
    const double heading = mainHeadingDegrees * DegToRad;
    const double doorLatitude = coord.Lat() * DegToRad;

    // Move the truck origin opposite to its contact-point offset.
    const double northOffsetMeters = zOffsetMeters * std::cos(heading);
    const double eastOffsetMeters = zOffsetMeters * std::sin(heading);

    double newPosLat = coord.Lat() + (northOffsetMeters / EARTH_RADIUS_METERS) * RadToDeg;
    double newPosLong = coord.Long() + (eastOffsetMeters / (EARTH_RADIUS_METERS * std::cos(doorLatitude))) * RadToDeg;

    return {newPosLong, newPosLat};
}

GSCoord GSGeography::RelativePosition(double headingDeg, const GSCoord coordA, double YMeters, double XMeters)
{
    const double heading = headingDeg * DegToRad;
    const double northMeters = YMeters * std::cos(heading) - XMeters * std::sin(heading);
    const double eastMeters = YMeters * std::sin(heading) + XMeters * std::cos(heading);
    return {
        coordA.Long() + eastMeters / MetersPerDegreeLong(coordA.Lat()),
        coordA.Lat() + northMeters / MetersPerDegreeLat()
    };
}

} // namespace NS_GSLiveAirportMSFS
