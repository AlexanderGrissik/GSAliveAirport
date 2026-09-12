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

    [[nodiscard]] static double NormalizeDegrees(double degrees) { return std::fmod(degrees + 360.0, 360.0); }

    static double DistanceMeters(const GSCoord& coordA, const GSCoord& coordB);

    static GSCoord RepositionZOffset(const GSCoord& coord, double mainHeadingDegrees, double zOffsetMeters);

    static double MetersPerDegreeLat();

    static double MetersPerDegreeLong(double latitude);
};

} // namespace NS_GSLiveAirportMSFS
