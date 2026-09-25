#include "GSAirport.h"
#include "GSGeography.h"
#include "GSLogStream.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>

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
    constexpr DWORD ParkingFlag = 0x80000000u;

    std::list<GSRoadsNetwork> roadNetworks;
    
    const auto nodeExists = [this, ParkingFlag](DWORD id) {
        const DWORD index = id & ~ParkingFlag;
        return (id & ParkingFlag)
            ? m_parkings.contains(index)
            : m_taxiPoints.contains(index);
    };

    const auto addNode = [this, ParkingFlag](GSRoadsNetwork& network, DWORD id) {
        const DWORD index = id & ~ParkingFlag;

        if (id & ParkingFlag) {
            const auto& parking = m_parkings.at(index);
            const GSCoord location = GSGeography::RelativePosition(
                0.0, m_location, parking.biasZMeters, parking.biasXMeters);

            const auto nodeType = parking.type == 13 ? GSRoadsNetwork::VEHICLE : GSRoadsNetwork::PARKING;

            network.AddNode(
                id, location, parking.headingDegTrue,
                nodeType, m_jetways.contains(index));
        } else {
            const auto& point = m_taxiPoints.at(index);
            const GSCoord location = GSGeography::RelativePosition(
                0.0, m_location, point.biasZMeters, point.biasXMeters);

            network.AddNode(id, location, 0.0F, GSRoadsNetwork::NORMAL, false);
        }
    };

    for (const TaxiPath& path : m_taxiPaths) {
        const DWORD startId = static_cast<DWORD>(path.startIndex);
        const DWORD endId = static_cast<DWORD>(path.endIndex) | (path.type == 3 ? ParkingFlag : 0u);
        if (!nodeExists(startId) || !nodeExists(endId)) {
            GSLogStream::LogError("GSAirport::OrganizeStructures - Missing taxi path endpoint: ")
                << path.type << ": " << startId << ", " << endId;
            continue;
        }

        std::vector<std::list<GSRoadsNetwork>::iterator> matches;
        for (auto it = roadNetworks.begin(); it != roadNetworks.end(); ++it) {
            if (it->HasNode(startId) || it->HasNode(endId)) {
                matches.push_back(it);
            }
        }

        if (matches.size() > 2) {
            GSLogStream::LogError("GSAirport::OrganizeStructures - Taxi path joins more than two networks: ")
                << startId << ", " << endId;
            continue;
        }

        auto targetIt = matches.empty() ? roadNetworks.emplace(roadNetworks.end()) : matches.front();

        if (matches.size() == 2) {
            targetIt->MergeNetwork(*matches.back());
            roadNetworks.erase(matches.back());
        }

        GSRoadsNetwork::RoadPathType pathType =
            (path.type == 6 || path.type == 7) ? GSRoadsNetwork::PATH_VEHICLE : GSRoadsNetwork::PATH_NORMAL;

        GSRoadsNetwork& network = *targetIt;
        addNode(network, startId);
        addNode(network, endId);
        network.AddPath(startId, endId, pathType);
    }

    MergeAllRoadNetworks(roadNetworks);
}

void GSAirport::MergeAllRoadNetworks(std::list<GSRoadsNetwork>& roadNetworks)
{
    while (roadNetworks.size() > 1) {
        GSRoadsNetwork& accumulator = roadNetworks.front();

        auto closestIt = roadNetworks.end();
        GSRoadsNetwork::RoadPath bestPath{ nullptr, nullptr, 9999999.0, GSRoadsNetwork::PATH_VEHICLE, true };
        for (auto it = std::next(roadNetworks.begin()); it != roadNetworks.end(); ++it) {
            const auto path = accumulator.GetClosestDisjointNodes(*it);
            if (path.m_distMeters < bestPath.m_distMeters) {
                bestPath = path;
                closestIt = it;
            }
        }

        if (closestIt == roadNetworks.end() || !bestPath.m_nodeA || !bestPath.m_nodeB) {
             GSLogStream::LogError("No bridge between networks on MergeAllRoadNetworks");
             return;
        }

        accumulator.MergeNetwork(*closestIt);   
        roadNetworks.erase(closestIt);
        accumulator.AddPath(bestPath);
    }

    // Move the single merged network into the member.
    m_roadNetwork = std::move(roadNetworks.front());

    m_roadNetwork.Print();
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
