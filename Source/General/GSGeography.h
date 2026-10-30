// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#define EARTH_RADIUS_METERS 6378137.0

#include "GSCoord.h"
#include <cmath>
#include <cstddef>

namespace NS_GSAliveAirport
{

class GSGeography
{
public:
    static constexpr double PI = 3.14159265358979323846; //std::acos(-1.0);
    static constexpr double DegToRad = PI / 180.0;
    static constexpr double RadToDeg = 180.0 / PI;
    static constexpr double MetersPerLatitudeDegree = 111'320.0;
    static constexpr double FeetPerMeter = 3.2808399;
    static constexpr double KnotsToMetersSecond = 0.514444;

    [[nodiscard]] static constexpr double DegDistPowFromMeters(double meters) { const auto val = meters / MetersPerDegreeLat(); return val * val; }

    [[nodiscard]] static constexpr double MetersPerDegreeLat() { return MetersPerLatitudeDegree; }

    [[nodiscard]] static constexpr double NormDeg(double degrees) {
        while (degrees >= 360.0) degrees -= 360.0;
        while (degrees < 0.0) degrees += 360.0;
        return degrees;
    }

    [[nodiscard]] static double LongDistAdjust(double latitude);

    [[nodiscard]] static double DistanceMeters(const GSCoord& coordA, const GSCoord& coordB);  

    [[nodiscard]] static double DistanceDegPow(const GSCoord& coordA, const GSCoord& coordB);

    [[nodiscard]] static double Azz(const GSCoord& coordA, const GSCoord& coordB);

    [[nodiscard]] static double MetersPerDegreeLong(double latitude);

    [[nodiscard]] static GSCoord RelativePosition(double headingDeg, const GSCoord coordA, double headingMeters, double crossMeters);

    // Returns intermediate headings along the shortest arc; the end is excluded.
    // Index zero is the first step. At 180 degrees, the clockwise arc is used.
    // Returns false without changing heading if spacing is not positive or the step reaches the end.
    [[nodiscard]] static bool PointOnCircle(
        double startHeading, double endHeading, double spacing, std::size_t index, double& heading);

    // Returns false without changing the outputs if the line misses the circle.
    // When the point is strictly inside, firstIntersection is behind the heading.
    // Otherwise, firstIntersection is the closer intersection. A tangent returns
    // the same point twice; a point on the circle is not considered inside.
    [[nodiscard]] static bool FindReverseCircleIntersection(
        const GSCoord& center, double radiusMeters, const GSCoord& pnt, double headingDeg,
        GSCoord& firstIntersection, GSCoord& secondIntersection, bool& pointInside);
};

} // namespace NS_GSAliveAirport
