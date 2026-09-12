#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

namespace NS_GSLiveAirportMSFS {

class GSDefinitions
{
public:
    enum GSEventID : SIMCONNECT_CLIENT_EVENT_ID {
        GSEventID_SimState = 1,
        GSDefID_Freeze_LongLat,
        GSDefID_Freeze_Altitude,
        GSDefID_Freeze_Attitude,
    };

    enum GSDefID : SIMCONNECT_DATA_DEFINITION_ID {
        GSDefID_Aircraft = 1,
        GSDefID_CateringTruckStateGet,
        GSDefID_CateringTruckStateSet,
    };

    enum ECommands
    {
        CMD_REQ_PROCESS = 0,
        CMD_PRINT_AIRCRAFT_ALL,
        CMD_PRINT_AIRCRAFT_1KM,
        CMD_PRINT_AIRCRAFT_PARKED,
        CMD_SPAWNER_AIRCRAFT_ADDED,
        CMD_SPAWNER_AIRCRAFT_REMOVED,
        CMD_SPAWNER_AIRCRAFT_MODIFIED,
        CMD_SPAWNER_AIRCRAFT_USER
    };

    struct DatumSpec
    {
        const char *name;
        const char *units;
        SIMCONNECT_DATATYPE type;
    };

    struct SendResultIDs {
        DWORD sendID;
        DWORD requestID;
    };

    struct SendResult : public SendResultIDs{
        HRESULT rc;
        bool isOK() { return SUCCEEDED(rc); }
    };
};

} // namespace NS_GSLiveAirportMSFS
