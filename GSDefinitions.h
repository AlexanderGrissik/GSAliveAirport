#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245) // SimConnect SDK enum declarations use signed literals.
#include <SimConnect.h>
#pragma warning(pop)

namespace parking_services {

struct DatumSpec
{
    const char *name;
    const char *units;
    SIMCONNECT_DATATYPE type;
};

class GSDefinitions
{
    enum GSEventID : SIMCONNECT_CLIENT_EVENT_ID {   // Underlying type is DWORD (unsigned long)
        GSEventID_SimState = 1,
        //constexpr SIMCONNECT_CLIENT_EVENT_ID kEventObjectRemoved = 3; //SubscribeSystemEvent(kEventObjectRemoved, "ObjectRemoved");
    };

    enum GSDefID : SIMCONNECT_DATA_DEFINITION_ID {
        GSDefID_Aircraft = 1;
    }

    
}

}