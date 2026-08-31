#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <SimConnect.h>

namespace parking_services
{
enum DefinitionId : SIMCONNECT_DATA_DEFINITION_ID
{
    DefinitionAircraft = 1,
    DefinitionGround,
    DefinitionAnimationProbe,
    DefinitionAnimationUpdate,
};

enum EventId : SIMCONNECT_CLIENT_EVENT_ID
{
    EventSimStart = 1,
    EventSimStop,
    EventObjectAdded,
    EventObjectRemoved,
    EventFreezeLatitudeLongitude,
    EventFreezeAltitude,
    EventFreezeAttitude,
};
} // namespace parking_services
