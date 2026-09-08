#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

namespace NS_GSLiveAirportMSFS {

struct DatumSpec
{
    const char *name;
    const char *units;
    SIMCONNECT_DATATYPE type;
};

class GSDefinitions
{
public:
    enum GSEventID : SIMCONNECT_CLIENT_EVENT_ID {
        GSEventID_SimState = 1,
    };

    enum GSDefID : SIMCONNECT_DATA_DEFINITION_ID {
        GSDefID_Aircraft = 1
    };
};

} // namespace NS_GSLiveAirportMSFS
