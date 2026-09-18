#include "GSAirport.h"
#include "GSGeography.h"
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
    "OPEN JETWAY",
    "PARKING_GATE",
    "PARKING_SUFFIX",
    "PARKING_SPOT",
    "CLOSE JETWAY",
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

bool GSAirport::NetworkContainsNode(const std::vector<TaxiPathExt*>& network, DWORD nodeIndex)
{
    return std::any_of(network.begin(), network.end(), [nodeIndex](const TaxiPathExt* path) {
        return path->startIndex == static_cast<int>(nodeIndex) || path->endIndex == static_cast<int>(nodeIndex);
    });
}

void GSAirport::MergeNetworks(std::vector<std::vector<TaxiPathExt*>>& networks, std::size_t targetIndex, std::size_t sourceIndex)
{
    auto& target = networks[targetIndex];
    const auto& source = networks[sourceIndex];

    target.insert(target.end(), source.begin(), source.end());
    networks.erase(networks.begin() + sourceIndex);
}

void GSAirport::OrganizeStructures()
{
    for (auto& [index, point] : m_taxiPoints) {
        point.m_longLat = GSGeography::RelativePosition(0.0, m_location, point.biasZMeters, point.biasXMeters);
    }

    for (auto& [index, parking] : m_parkings) {
        parking.m_longLat = GSGeography::RelativePosition(0.0, m_location, parking.biasZMeters, parking.biasXMeters);

        const auto jetway = m_jetways.find(index);
        if (jetway != m_jetways.end())
            parking.m_jetway = &jetway->second;
    }

    for (TaxiPathExt& path : m_taxiPaths) {
        const auto pointA = m_taxiPoints.find(path.startIndex);
        const auto pointB = m_taxiPoints.find(path.endIndex);
        const auto parkingA = m_parkings.find(path.startIndex);
        const auto parkingB = m_parkings.find(path.endIndex);

        if (parkingA != m_parkings.end() && pointB != m_taxiPoints.end()) {
            parkingA->second.m_taxiPoint = &pointB->second;
            continue;
        }

        if (parkingB != m_parkings.end() && pointA != m_taxiPoints.end()) {
            parkingB->second.m_taxiPoint = &pointA->second;
            continue;
        }

        if (pointA == m_taxiPoints.end() || pointB == m_taxiPoints.end())
            continue;

        path.m_nodeA = &pointA->second;
        path.m_nodeB = &pointB->second;
        pointA->second.m_conns.emplace_back(&path);
        pointB->second.m_conns.emplace_back(&path);
    }

    std::vector<size_t> matches;
    matches.reserve(64);

    for (TaxiPathExt& path : m_taxiPaths) {
        matches.clear();
        for (size_t i = 0; i < m_roadNetworks.size(); ++i) {
            if (NetworkContainsNode(m_roadNetworks[i], path.startIndex) ||
                NetworkContainsNode(m_roadNetworks[i], path.endIndex)) {
                matches.emplace_back(i);
            }
        }

        if (matches.empty()) {
            m_roadNetworks.emplace_back();
            m_roadNetworks.back().emplace_back(&path);
        }
        else {
            const size_t target = matches.front();
            m_roadNetworks[target].emplace_back(&path);

            for (size_t i = matches.size(); i-- > 1;)
                MergeNetworks(m_roadNetworks, target, matches[i]);
        }
    }
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
    if (message->dwID == SIMCONNECT_RECV_ID_FACILITY_DATA_END) {
        m_airport.OrganizeStructures();
        return true;
    }

    if (message->dwID != SIMCONNECT_RECV_ID_FACILITY_DATA) {
        GSLogStream::LogError("GSAirport::GSReqInfo unexpected message: ") << message->dwID;
        return true;
    }

    const auto& entry = *reinterpret_cast<const SIMCONNECT_RECV_FACILITY_DATA*>(message);
    switch (entry.Type) {
    case SIMCONNECT_FACILITY_DATA_TAXI_PARKING:
        m_airport.m_parkings.try_emplace(entry.ItemIndex, entry.ItemIndex, *reinterpret_cast<const ParkingSlot*>(&entry.Data));
        break;
    case SIMCONNECT_FACILITY_DATA_TAXI_POINT:
        m_airport.m_taxiPoints.try_emplace(entry.ItemIndex, entry.ItemIndex, *reinterpret_cast<const TaxiPoint*>(&entry.Data));
        break;
    case SIMCONNECT_FACILITY_DATA_JETWAY: {
        const auto& path = *reinterpret_cast<const Jetway*>(&entry.Data);
        m_airport.m_jetways.try_emplace(path.parkingSpot, path);
        break;
    }
    case SIMCONNECT_FACILITY_DATA_TAXI_PATH: {
        const auto& path = *reinterpret_cast<const TaxiPath*>(&entry.Data);
        if (path.type == 4 || path.type == 3 || path.type == 6 || path.type == 7) // If ROAD/SERVICE/PATH/PARKING
            m_airport.m_taxiPaths.emplace_back(path);
        break;
    }
    }

    return false;
}

void GSAirport::GSReqInfo::OnException(SIMCONNECT_RECV_EXCEPTION* message)
{
    GSLogStream::LogError("GSAirport request failed: ") << message->dwException;
}

}
