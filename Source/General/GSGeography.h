#pragma once

#define EARTH_RADIUS_METERS 6378137.0

#include "GSCoord.h"

namespace NS_GSLiveAirportMSFS
{

class GSGeography
{
public:
    static double PI;
    static double DegToRad;
    static double RadToDeg;
    static double MetersPerLatitudeDegree;
    static double FeetPerMeter;

    [[nodiscard]] static double NormDeg(double degrees);

    [[nodiscard]] static double DistanceMeters(const GSCoord& coordA, const GSCoord& coordB);

    [[nodiscard]] static double Azz(const GSCoord& coordA, const GSCoord& coordB);

    [[nodiscard]] static GSCoord RepositionZOffset(const GSCoord& coord, double mainHeadingDegrees, double zOffsetMeters);

    [[nodiscard]] static double MetersPerDegreeLat();

    [[nodiscard]] static double MetersPerDegreeLong(double latitude);

    [[nodiscard]] static GSCoord RelativePosition(double headingDeg, const GSCoord coordA, double YMeters, double XMeters);
};

} // namespace NS_GSLiveAirportMSFS
