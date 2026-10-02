// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

#define VD(x) (void)(x)

namespace NS_GSAliveAirport {

class GSDefinitions
{
public:
    enum GSEventID : SIMCONNECT_CLIENT_EVENT_ID {
        GSEventID_SimState = 1,
        GSDefID_Freeze_LongLat,
        GSDefID_Freeze_Altitude,
        GSDefID_Freeze_Attitude,
        GSDefID_OpenDoors,
        GSDefID_CloseDoors,
        GSDefID_Jetway
    };

    enum GSDefID : SIMCONNECT_DATA_DEFINITION_ID {
        GSDefID_Aircraft = 1,
        GSDefID_Position,
        GSDefID_AIWaypoints,
        GSDefID_AirportInfo,
        GSDefID_CateringTruckStateGet,
        GSDefID_CateringTruckStateSet,
        GSDefID_GroundPowerStateSet,
        GSDefID_GroundPowerExtStateSet,
        GSDefID_BuggageLoaderExtStateGet,
        GSDefID_BuggageLoaderExtStateSet,
        GSDefID_AnimVelocBodyY,
        GSDefID_AnimWagonBLO,
        GSDefID_AnimWagonBFLO,
        GSDefID_AnimWagonBFLOT,
        GSDefID_PlanePosition
    };

    enum ECommands
    {
        CMD_REQ_PROCESS = 0,
        CMD_REQ_PROCESS_IDENT,
        CMD_PRINT_AIRCRAFT_ALL,
        CMD_PRINT_AIRCRAFT_1KM,
        CMD_PRINT_AIRCRAFT_PARKED,
        CMD_SPAWN_TEST_AIRCRAFT,
        CMD_SPAWNER_AIRCRAFT_ADDED,
        CMD_SPAWNER_AIRCRAFT_REMOVED,
        CMD_SPAWNER_AIRCRAFT_MODIFIED,
        CMD_SPAWNER_AIRCRAFT_USER,
        CMD_ANIM_OBJ_ADD,
        CMD_ANIM_OBJ_REM,
        CMD_ANIM_OBJ_REM_DONE,
        CMD_MVMNT_OBJ_ADD,
        CMD_MVMNT_OBJ_REM,
        CMD_MVMNT_OBJ_REM_RET,
        CMD_MVMNT_OBJ_ARR,
        CMD_AIRPORT,
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
        bool isOK() const { return SUCCEEDED(rc); }
    };
};

} // namespace NS_GSAliveAirport
