#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245) // SimConnect SDK enum declarations use signed literals.
#include <SimConnect.h>
#pragma warning(pop)

namespace parking_services
{
// Shared placement/alignment constants used across the codebase.
inline constexpr double kFeetToMeters = 0.3048;
inline constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;

// Normalizes an angle into the [0, 360) degree range.
[[nodiscard]] double NormalizeDegrees(double degrees);

SIMCONNECT_DATA_INITPOSITION RelativePosition(
    double headingDegrees, double longitude, double latitude, double altitudeFeet,
    double forwardMeters, double rightMeters);
double HeadingTowardRelativeOrigin(double referenceHeadingDegrees,
                                   double forwardMeters, double rightMeters);
} // namespace parking_services
