// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once
#include "GSRequest.h"
#include "GSSimConnect.h"
#include "GSCoord.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "GSRoadsNetwork.h"

namespace NS_GSAliveAirport
{

class GSAirport
{
public:

    #pragma pack(push, 1)
    struct ParkingSlot {
        std::int32_t  type;           // TYPE
        std::int32_t  name;           // NAME
        std::int32_t  suffix;         // SUFFIX
        std::uint32_t number;         // NUMBER
        float         headingDegTrue; // HEADING
        float         radiusMeters;   // RADIUS
        float         biasXMeters;    // BIAS_X
        float         biasZMeters;    // BIAS_Z
    };

    struct TaxiPoint {
        std::int32_t type;           // TYPE
        float        biasXMeters;    // BIAS_X
        float        biasZMeters;    // BIAS_Z
    };

    struct TaxiPath {
        std::int32_t type;       // TYPE: 6 = VEHICLE, 7 = ROAD
        float        widthMeters;// WIDTH
        std::int32_t startIndex; // START
        std::int32_t endIndex;   // END
    };

    struct Jetway {
        std::int32_t parkingGate;   // PARKING_GATE
        std::int32_t parkingSuffix; // PARKING_SUFFIX
        std::int32_t parkingSpot;   // PARKING_SPOT
    };
    #pragma pack(pop)

    static_assert(sizeof(ParkingSlot) == 32);
    static_assert(sizeof(TaxiPoint) == 12);
    static_assert(sizeof(TaxiPath) == 16);
    static_assert(sizeof(Jetway) == 12);

    GSAirport(const std::string& icao, const GSCoord& loc, DWORD loadingReqID): 
        m_icao(icao), m_location(loc), m_loadingReqID(loadingReqID)  {}

    static void InitDatums(GSSimConnect& handler);

    void LoadInfo(const SIMCONNECT_RECV_FACILITY_DATA& entry);
    void OrganizeStructures();
    const std::string& GetICAO() const { return m_icao; } 
    const GSCoord& GetLongLat() const { return m_location; }
    const GSRoadsNetwork& GetRoadNet() const { return m_roadNetwork; } 
    DWORD LoadingReqID() const { return m_loadingReqID; }
    bool IsFinalized() const { return m_isFinalized; }

private:

    void MergeAllRoadNetworks(std::list<GSRoadsNetwork>& roadNetworks);

    std::unordered_map<DWORD, ParkingSlot> m_parkings;
    std::unordered_map<DWORD, TaxiPoint> m_taxiPoints;
    std::unordered_map<DWORD, Jetway> m_jetways;
    std::vector<TaxiPath> m_taxiPaths;
    GSRoadsNetwork m_roadNetwork;
    std::string m_icao;
    GSCoord m_location;
    DWORD m_loadingReqID;
    bool m_isFinalized = false;
};

}
