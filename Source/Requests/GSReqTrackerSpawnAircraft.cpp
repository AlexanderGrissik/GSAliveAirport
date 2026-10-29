// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSReqTrackerSpawnAircraft.h"
#include "../General/GSAircraftTrackerThread.h"
#include "../General/GSLogStream.h"
#include <chrono>
#include <thread>

namespace NS_GSAliveAirport
{

using namespace std::chrono_literals;

GSReqRC GSReqTrackerSpawnAircraft::Process()
{
    GSAircraftTrackerThread& tracker = static_cast<GSAircraftTrackerThread&>(m_simHandle);
    return tracker.SpawnTestAircraft(m_type, m_pos);
}

bool GSReqTrackerSpawnAircraft::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    if (message->dwID == SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID) {
        auto* msg = static_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID*>(message);
        
        auto requestId = m_simHandle.NextRequestID();
        auto aiRC = m_simHandle.InvokeRequest(requestId, SimConnect_AIReleaseControl, msg->dwObjectID, requestId);
        auto beaconRC = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSEventID_BeaconLights, 0,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

        std::this_thread::sleep_for(100ms);
        m_pos.Airspeed = 0;

        auto simRC1 = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSEventID_Freeze_LongLat, 1,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        auto simRC2 = m_simHandle.Invoke(SimConnect_TransmitClientEvent, msg->dwObjectID, GSEventID_Freeze_Altitude, 1,
            SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        auto simRC = m_simHandle.Invoke(SimConnect_SetDataOnSimObject, GSDatumID_Position, msg->dwObjectID, 0, 1,
            static_cast<DWORD>(sizeof(m_pos)), &m_pos);
        
        if (!simRC1.isOK() || !simRC2.isOK() || !simRC.isOK() || !aiRC.isOK() || !beaconRC.isOK()) {
            GSLogStream::LogError("GSReqTrackerSpawnAircraft::OnMessage Failed call Freeze: ") << 
                simRC1.rc << "," << simRC2.rc << "," << simRC.rc << "," << beaconRC.rc;
        }

        std::this_thread::sleep_for(100ms);
    
        if (m_type == 2) {
            m_simHandle.Invoke(
                SimConnect_TransmitClientEvent, msg->dwObjectID, GSEventID_Jetway,
                0, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        }
    } else {
        GSLogStream::LogError("GSReqTrackerSpawnAircraft::OnMessage Unexpected Message: ") << message->dwID;
    }
    (void)messageSize;
    return true;
}

void GSReqTrackerSpawnAircraft::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSLogStream::LogError("GSReqTrackerSpawnAircraft::OnException: ") << message->dwException << ", " << message->dwIndex;
}

}
