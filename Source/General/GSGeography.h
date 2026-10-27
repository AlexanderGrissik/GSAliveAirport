// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#define EARTH_RADIUS_METERS 6378137.0

#include "GSCoord.h"
#include <cmath>

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

    [[nodiscard]] static GSCoord RepositionZOffset(const GSCoord& coord, double mainHeadingDegrees, double zOffsetMeters);

    [[nodiscard]] static double MetersPerDegreeLong(double latitude);

    [[nodiscard]] static GSCoord RelativePosition(double headingDeg, const GSCoord coordA, double YMeters, double XMeters);

    [[nodiscard]] static GSCoord FindReverseCircleIntersection(
        const GSCoord& center, double radiusMeters, const GSCoord& pnt, double headingDeg);
};

} // namespace NS_GSAliveAirport
