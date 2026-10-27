// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSGeography.h"

namespace NS_GSAliveAirport
{

double GSGeography::LongDistAdjust(double latitude)
{
    return std::cos(latitude * DegToRad);
}

double GSGeography::MetersPerDegreeLong(double latitude)
{
    return MetersPerLatitudeDegree * LongDistAdjust(latitude);
}

double GSGeography::DistanceMeters(const GSCoord& coordA, const GSCoord& coordB)
{
    const double latitudeMeters = (coordB.Lat() - coordA.Lat()) * MetersPerDegreeLat();
    const double averageLatitude = (coordA.Lat() + coordB.Lat()) / 2.0;
    const double longitudeMeters = (coordB.Long() - coordA.Long()) * MetersPerDegreeLong(averageLatitude);
    return std::hypot(latitudeMeters, longitudeMeters);
}

double GSGeography::DistanceDegPow(const GSCoord& coordA, const GSCoord& coordB)
{
    const double latitudeDelta = (coordB.Lat() - coordA.Lat());
    const double averageLatitude = (coordA.Lat() + coordB.Lat()) / 2.0;
    const double longitudeDelta = (coordB.Long() - coordA.Long()) * LongDistAdjust(averageLatitude);
    return (latitudeDelta * latitudeDelta) + (longitudeDelta * longitudeDelta);
}

double GSGeography::Azz(const GSCoord& coordA, const GSCoord& coordB)
{
    const double northMeters = (coordB.Lat() - coordA.Lat()) * MetersPerDegreeLat();
    const double averageLatitude = (coordA.Lat() + coordB.Lat()) / 2.0;
    const double eastMeters = (coordB.Long() - coordA.Long()) * MetersPerDegreeLong(averageLatitude);
    return NormDeg(std::atan2(eastMeters, northMeters) * RadToDeg);
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

GSCoord GSGeography::FindReverseCircleIntersection(const GSCoord& center, double radiusMeters, const GSCoord& pnt, double headingDeg)
{
    double radius = radiusMeters / MetersPerDegreeLong(center.Lat());

    // Move point into circle-local coordinates.
    const double x0 = pnt.Long() - center.Long();
    const double y0 = pnt.Lat() - center.Lat();

    // Aviation heading: 0° = North, 90° = East
    const double heading = headingDeg * DegToRad;
    const double dx = std::sin(heading);
    const double dy = std::cos(heading);

    // Circle: x^2 + y^2 = radius^2
    // Line: x = x0 + t * dx, y = y0 + t * dy

    const double B = x0 * dx + y0 * dy;
    const double C = x0 * x0 + y0 * y0 - radius * radius;
    const double discriminant = B * B - C;

    if (discriminant < 0.0) {
        return center;
    }

    // Negative root = intersection behind the current heading.
    const double t = -B - std::sqrt(discriminant);

    // Intersection in circle-local coordinates.
    const double x = x0 + t * dx;
    const double y = y0 + t * dy;

    // Convert back to original lon/lat coordinate system.
    return { center.Long() + x, center.Lat() + y };
}

} // namespace NS_GSAliveAirport
