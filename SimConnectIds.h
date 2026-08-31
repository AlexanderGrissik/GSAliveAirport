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
enum DefinitionId : SIMCONNECT_DATA_DEFINITION_ID
{
    DefinitionAircraft = 1,
    DefinitionGround,
    DefinitionAnimationProbe,
    DefinitionAnimationUpdate,
    DefinitionBaggageBeltLoaderAnimation,
    DefinitionBaggageBeltWorkerAnimation,
    DefinitionBaggageLoaderRampTarget,
    DefinitionBaggageLoaderGeometry,
    DefinitionObjectPosition,
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
    EventOpenAircraftDoors,
    EventCloseAircraftDoors,
};
} // namespace parking_services
