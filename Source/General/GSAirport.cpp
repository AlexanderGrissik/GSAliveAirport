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

void GSAirport::OrganizeStructures()
{
    m_roadNetworks.clear();

    const auto nodeExists = [this](DWORD id) {
        return m_parkings.contains(id) || m_taxiPoints.contains(id);
    };

    const auto addNode = [this](GSRoadsNetwork& network, DWORD id) {
        if (const auto parking = m_parkings.find(id); parking != m_parkings.end()) {
            const GSCoord location = GSGeography::RelativePosition(
                0.0, m_location, parking->second.biasZMeters, parking->second.biasXMeters);
            
            // SimConnect TAXI_PARKING TYPE: VEHICLE
            const auto nodeType = parking->second.type == 13 ? GSRoadsNetwork::VEHICLE : GSRoadsNetwork::PARKING;
            network.AddNode(id, location, parking->second.headingDegTrue, nodeType, m_jetways.contains(id));
            return;
        }

        const auto point = m_taxiPoints.find(id);
        const GSCoord location = GSGeography::RelativePosition(0.0, m_location, point->second.biasZMeters, point->second.biasXMeters);
        network.AddNode(id, location, 0.0F, GSRoadsNetwork::NORMAL, false);
    };

    for (const TaxiPath& path : m_taxiPaths) {
        const DWORD startId = static_cast<DWORD>(path.startIndex);
        const DWORD endId = static_cast<DWORD>(path.endIndex);
        if (!nodeExists(startId) || !nodeExists(endId)) {
            GSLogStream::LogError("GSAirport::OrganizeStructures - Missing taxi path endpoint: ")
                << startId << ", " << endId;
            continue;
        }

        std::vector<std::size_t> matches;
        for (std::size_t i = 0; i < m_roadNetworks.size(); ++i) {
            if (m_roadNetworks[i].HasNode(startId) || m_roadNetworks[i].HasNode(endId)) {
                matches.push_back(i);
            }
        }

        if (matches.size() > 2) {
            GSLogStream::LogError("GSAirport::OrganizeStructures - Taxi path joins more than two networks: ")
                << startId << ", " << endId;
            continue;
        }

        std::size_t targetIndex;
        if (matches.empty()) {
            targetIndex = m_roadNetworks.size();
            m_roadNetworks.emplace_back();
        } else {
            targetIndex = matches.front();
        }

        if (matches.size() == 2) {
            const std::size_t sourceIndex = matches.back();
            m_roadNetworks[targetIndex].MergeNetwork(m_roadNetworks[sourceIndex]);
            m_roadNetworks.erase(m_roadNetworks.begin() + sourceIndex);
        }

        GSRoadsNetwork& network = m_roadNetworks[targetIndex];
        addNode(network, startId);
        addNode(network, endId);
        network.AddPath(startId, endId);
    }
}

std::optional<const GSRoadsNetwork*> GSAirport::GetNetworkByParking(const GSCoord& loc) const
{
    constexpr double MaxParkingDistMtr = 200.0;

    const GSRoadsNetwork* closestNetwork = nullptr;
    double closestDistance = MaxParkingDistMtr;
    for (const GSRoadsNetwork& network : m_roadNetworks) {
        const auto parking = network.GetClosestNormalParking(loc);
        if (!parking) {
            continue;
        }

        const double distance = GSGeography::DistanceMeters(loc, (*parking)->m_loc);
        if (distance <= MaxParkingDistMtr && (!closestNetwork || distance < closestDistance)) {
            closestNetwork = &network;
            closestDistance = distance;
        }
    }

    if (!closestNetwork) {
        return std::nullopt;
    }
    return closestNetwork;
}

std::optional<const GSRoadsNetwork::RoadNode*> GSAirport::GetClosestJetwayParking(const GSCoord& loc) const
{
    const GSRoadsNetwork::RoadNode* closest = nullptr;
    double closestDistance = 0.0;

    for (const GSRoadsNetwork& network : m_roadNetworks) {
        const auto parking = network.GetClosestJetwayParking(loc);
        if (!parking) {
            continue;
        }

        const double distance = GSGeography::DistanceMeters(loc, (*parking)->m_loc);
        if (!closest || distance < closestDistance) {
            closest = *parking;
            closestDistance = distance;
        }
    }

    if (!closest) {
        return std::nullopt;
    }
    return closest;
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
        m_airport.m_parkings.try_emplace(entry.ItemIndex, *reinterpret_cast<const ParkingSlot*>(&entry.Data));
        break;
    case SIMCONNECT_FACILITY_DATA_TAXI_POINT:
        m_airport.m_taxiPoints.try_emplace(entry.ItemIndex, *reinterpret_cast<const TaxiPoint*>(&entry.Data));
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
