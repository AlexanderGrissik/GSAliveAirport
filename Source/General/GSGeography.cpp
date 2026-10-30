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

GSCoord GSGeography::RelativePosition(double headingDeg, const GSCoord coordA, double headingMeters, double crossMeters)
{
    const double heading = headingDeg * DegToRad;
    const double northMeters = headingMeters * std::cos(heading) - crossMeters * std::sin(heading);
    const double eastMeters = headingMeters * std::sin(heading) + crossMeters * std::cos(heading);
    return {
        coordA.Long() + eastMeters / MetersPerDegreeLong(coordA.Lat()),
        coordA.Lat() + northMeters / MetersPerDegreeLat()
    };
}

bool GSGeography::FindReverseCircleIntersection(
    const GSCoord& center, double radiusMeters, const GSCoord& pnt, double headingDeg,
    GSCoord& firstIntersection, GSCoord& secondIntersection, bool& pointInside)
{
    if (radiusMeters < 0.0 || std::abs(center.Lat()) >= 90.0) {
        return false;
    }

    const double metersPerLong = MetersPerDegreeLong(center.Lat());
    const double x0 = (pnt.Long() - center.Long()) * metersPerLong;
    const double y0 = (pnt.Lat() - center.Lat()) * MetersPerDegreeLat();
    const double heading = headingDeg * DegToRad;
    const double dx = std::sin(heading);
    const double dy = std::cos(heading);

    // Solve the intersection of the heading line and the circle in meters.
    const double b = x0 * dx + y0 * dy;
    const double c = x0 * x0 + y0 * y0 - radiusMeters * radiusMeters;
    const double discriminant = b * b - c;
    if (discriminant < 0.0) {
        return false;
    }

    const double root = std::sqrt(discriminant);
    const double lower = -b - root;
    const double upper = -b + root;
    pointInside = lower < 0.0 && upper > 0.0;
    const bool useLowerFirst = pointInside || std::abs(lower) <= std::abs(upper);

    const auto toCoord = [&](double distance) {
        return GSCoord(center.Long() + (x0 + distance * dx) / metersPerLong,
                       center.Lat() + (y0 + distance * dy) / MetersPerDegreeLat());
    };

    firstIntersection = toCoord(useLowerFirst ? lower : upper);
    secondIntersection = toCoord(useLowerFirst ? upper : lower);
    return true;
}

bool GSGeography::PointOnCircle(
    double startHeading, double endHeading, double spacing, std::size_t index, double& heading)
{
    const double start = NormDeg(startHeading);
    const double clockwiseDistance = NormDeg(endHeading - start);
    const bool clockwise = clockwiseDistance <= 180.0;
    const double arcLength = clockwise ? clockwiseDistance : 360.0 - clockwiseDistance;
    const double step = (static_cast<double>(index) + 1.0) * spacing;
    if (step >= arcLength) {
        return false;
    }

    heading = NormDeg(start + (clockwise ? step : -step));
    return true;
}

} // namespace NS_GSAliveAirport
