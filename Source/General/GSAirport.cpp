#include "GSAirport.h"
#include "GSLogStream.h"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_AirportInfo{
    "OPEN AIRPORT",
    "OPEN TAXI_PARKING",
    "TYPE",
    "NAME",
    "SUFFIX",
    "NUMBER",
    "HEADING",
    "RADIUS",
    "BIAS_X",
    "BIAS_Z",
    "CLOSE TAXI_PARKING",
    "OPEN TAXI_POINT",
    "TYPE",
    "BIAS_X",
    "BIAS_Z",
    "CLOSE TAXI_POINT",
    "OPEN TAXI_PATH",
    "TYPE",
    "WIDTH",
    "START",
    "END",
    "CLOSE TAXI_PATH",
    "CLOSE AIRPORT"
};

void GSAirport::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddFacilityDatums(GSDatums_AirportInfo, GSDefinitions::GSDefID_AirportInfo);
}

void GSAirport::LoadInfo(GSSimConnect& handler)
{
    handler.PostReqCommand(new GSReqInfo(handler, *this));
}

GSRequest::SendResult GSAirport::GSReqInfo::Process()
{
    auto id = m_simHandle.NextRequestID();
    GSRequest::SendResult rc = {
        m_simHandle.InvokeRequest(id, SimConnect_RequestFacilityData, GSDefinitions::GSDefID_AirportInfo, id, m_airport.GetICAO().c_str(), ""), true};
    
    if (!rc.m_simRC.isOK()) {
        GSLogStream::LogError("GSAirport::GSReqInfo::Process Failed call: ") << rc.m_simRC.rc;
        rc.m_keep = false;
    } 

    return rc;
}

bool GSAirport::GSReqInfo::OnMessage(SIMCONNECT_RECV* message, DWORD messageSize)
{
    (void)messageSize;
    if (message->dwID == SIMCONNECT_RECV_ID_FACILITY_DATA_END)
        return true;

    if (message->dwID != SIMCONNECT_RECV_ID_FACILITY_DATA) {
        GSLogStream::LogError("GSAirport::GSReqInfo unexpected message: ") << message->dwID;
        return true;
    }

    const auto& entry = *reinterpret_cast<const SIMCONNECT_RECV_FACILITY_DATA*>(message);
    switch (entry.Type) {
    case SIMCONNECT_FACILITY_DATA_TAXI_PARKING:
        m_airport.m_parkings.emplace_back(*reinterpret_cast<const ParkingSlot*>(&entry.Data));
        break;
    case SIMCONNECT_FACILITY_DATA_TAXI_POINT:
        m_airport.m_taxiPoints.emplace_back(*reinterpret_cast<const TaxiPoint*>(&entry.Data));
        break;
    case SIMCONNECT_FACILITY_DATA_TAXI_PATH:
        const auto& path = *reinterpret_cast<const TaxiPath*>(&entry.Data);
        if (path.type == 6 || path.type == 7) // VEHICLE or ROAD
            m_airport.m_taxiPaths.emplace_back(path);
        break;
    }

    return false;
}

void GSAirport::GSReqInfo::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSLogStream::LogError("GSAirport request failed: ") << message->dwException;
}

}
