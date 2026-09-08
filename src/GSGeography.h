#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

#include <cmath>

namespace NS_GSLiveAirportMSFS
{

class GSGeography
{
public:
    static constexpr double s_feetToMeters = 0.3048;
    static constexpr double s_radiansToDegrees = 180.0 / 3.14159265358979323846;
    static constexpr double s_metersPerLatitudeDegree = 111'320.0;
    static constexpr double s_degreesToRadians = 3.14159265358979323846 / 180.0;

    [[nodiscard]] static double NormalizeDegrees(double degrees) { return std::fmod(degrees + 360.0, 360.0); }
    static double MetersPerDegreeLat() { return s_metersPerLatitudeDegree; }

    static SIMCONNECT_DATA_INITPOSITION RelativePosition(double headingDegrees, double longitude, double latitude, double altitudeFeet, double forwardMeters, double rightMeters);

    static double HeadingTowardRelativeOrigin(double referenceHeadingDegrees, double forwardMeters, double rightMeters);

    static double MetersPerDegreeLong(double latitude);

    static double DistanceMeters(double latitudeA, double longitudeA, double latitudeB, double longitudeB);
};

} // namespace NS_GSLiveAirportMSFS
