// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intsafe.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)
#include <chrono>

#define VD(x) (void)(x)

namespace NS_GSAliveAirport {

using GSTimePoint = std::chrono::steady_clock::time_point;

struct GSInvokeID {
    DWORD sendID;
    DWORD requestID;
};

struct GSInvokeRC : public GSInvokeID{
    HRESULT rc;
    bool isOK() const { return SUCCEEDED(rc); }
};

struct GSReqRC : public GSInvokeRC {
    bool keepReq;
};
    
struct GSDatum
{
    const char *name;
    const char *units;
    SIMCONNECT_DATATYPE type;
};

enum GSCommandID
{
    GS_CMD_REQ_PROCESS = 0,
    GS_CMD_REQ_PROCESS_IDENT,
    GS_CMD_SPAWN_TEST_AIRCRAFTS,
    GS_CMD_SPAWNER_AIRCRAFT_ADDED,
    GS_CMD_SPAWNER_AIRCRAFT_REMOVED,
    GS_CMD_SPAWNER_AIRCRAFT_MODIFIED,
    GS_CMD_AIRCRAFT_USER,
    GS_CMD_ANIM_OBJ_ADD,
    GS_CMD_ANIM_OBJ_REM,
    GS_CMD_ANIM_OBJ_REM_DONE,
    GS_CMD_MVMNT_OBJ_ADD,
    GS_CMD_MVMNT_OBJ_REM,
    GS_CMD_MVMNT_OBJ_REM_RET,
    GS_CMD_MVMNT_OBJ_ARR,
    GS_CMD_AIRPORT,
};

enum GSDatumID : SIMCONNECT_DATA_DEFINITION_ID {
    GSDatumID_Aircraft = 1,
    GSDatumID_AircraftConst,
    GSDatumID_Position,
    GSDatumID_AIWaypoints,
    GSDatumID_AirportInfo,
    GSDatumID_CateringTruckStateGet,
    GSDatumID_CateringTruckStateSet,
    GSDatumID_GroundPowerStateSet,
    GSDatumID_GroundPowerExtStateSet,
    GSDatumID_BuggageLoaderExtStateGet,
    GSDatumID_BuggageLoaderExtStateSet,
    GSDatumID_StaircaseExtStateGet,
    GSDatumID_StaircaseExtStateSet,
    GSDatumID_AnimVelocBodyY,
    GSDatumID_AnimWagonBLO,
    GSDatumID_AnimWagonBFLO,
    GSDatumID_AnimWagonBFLOT,
    GSDatumID_PlanePosition
};

enum GSEventID : SIMCONNECT_CLIENT_EVENT_ID {
    GSEventID_SimState = 1,
    GSEventID_Freeze_LongLat,
    GSEventID_Freeze_Altitude,
    GSEventID_Freeze_Attitude,
    GSEventID_OpenDoors,
    GSEventID_CloseDoors,
    GSEventID_Jetway,
    GSEventID_BeaconLights
};

} // namespace NS_GSAliveAirport
