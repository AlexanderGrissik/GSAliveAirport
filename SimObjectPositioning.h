#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <SimConnect.h>

namespace parking_services
{
SIMCONNECT_DATA_INITPOSITION RelativePosition(
    double headingDegrees, double longitude, double latitude, double altitudeFeet,
    double forwardMeters, double rightMeters);
} // namespace parking_services
