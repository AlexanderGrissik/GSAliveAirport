#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

#include <cmath>
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

    [[nodiscard]] static double NormDeg(double degrees);

    [[nodiscard]] static double DistanceMeters(const GSCoord& coordA, const GSCoord& coordB);

    [[nodiscard]] static GSCoord RepositionZOffset(const GSCoord& coord, double mainHeadingDegrees, double zOffsetMeters);

    [[nodiscard]] static double MetersPerDegreeLat();

    [[nodiscard]] static double MetersPerDegreeLong(double latitude);

    [[nodiscard]] static GSCoord RelativePosition(double headingDeg, const GSCoord coordA, double YMeters, double XMeters);
};

} // namespace NS_GSLiveAirportMSFS
