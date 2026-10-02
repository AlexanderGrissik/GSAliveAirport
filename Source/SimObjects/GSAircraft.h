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

#include "../General/GSCoord.h"
#include "../General/GSSimConnect.h"
#include "../General/GSAirport.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace NS_GSAliveAirport
{

class GSAircraft
{
public:

    enum AircraftSizeCategory
    {
        Small,
        Medium,
        Large,
        ExtraLarge
    };

    #pragma pack(push, 1)
    struct InteractivePointWireData
    {
        std::int32_t type{};
        float posXMeter{};
        float posYFeet{};
        float posZMeter{};
        float headingDegrees{};
    };

    struct AircraftWireData
    {
        std::array<char, 32> atcId{};
        std::array<char, 32> atcAirline{};
        std::array<char, 32> atcFlightNumber{};
        double latitude{};
        double longitude{};
        float altitudeFeet{};
        float groundAltitudeFeet{};
        float headingDegrees{};
        float alt_abv_grnd{};
        float alt_abv_grnd_minus_cg{};
        float groundSpeedKnots{};
        std::array<char, 8> currentAirport{};
        std::array<char, 8> fromAirport{};
        std::array<char, 8> toAirport{};
        std::array<char, 32> assignedParking{};
        std::array<char, 32> trafficState{};
        std::int32_t etdSeconds{};
        std::int32_t etaSeconds{};
        std::int32_t lightBeacon{};
        std::int32_t lightNav{};
        std::int32_t lightTaxi{};
        std::int32_t parkingBrake{};
        std::int32_t onGround{};
        std::int32_t isUser{};

        std::array<char, 32> title{};
        std::int32_t numberOfEngines{};
        std::int32_t pushbackAttached{};
        std::int32_t pushbackWait{};
        double wingSpanMeters{};
        float pushbackContactXMeters{};
        float pushbackContactZMeters{};

        std::array<InteractivePointWireData, 12> interactivePoints{};
    };
    #pragma pack(pop)

    using InterPntRef = std::reference_wrapper<const InteractivePointWireData>;

    static constexpr std::size_t AIRCRAFT_WIREDATA_DYNSIZE = offsetof(AircraftWireData, title);
    static constexpr std::size_t s_MaxInteractivePnts = 12;

    static void InitDatums(GSSimConnect& handler);

    static bool IsValidInteractivePointType(std::int32_t type) { return type == 0 || type == 1 || type == 3 || type == 4; }
    static const char *InteractivePointTypeName(std::int32_t type);
    static const char *AircraftSizeName(GSAircraft::AircraftSizeCategory category);

    void LoadDynamicState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry);
    void LoadFullState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry);
    void CopyDynInfo(const GSAircraft& another);
    
    bool operator==(const GSAircraft& another) const;
    bool operator!=(const GSAircraft& another) const { return !(*this == another); }

    bool IsParkedActive() const;
    double LateralDistanceMetersFrom(const GSAircraft& ac) const;

    void Print() const;
    const AircraftWireData& GetRawData() const { return m_rawData; }
    AircraftSizeCategory GetCategory() const { return m_category; }
    DWORD GetObjID() const { return objectID; }
    GSCoord GetLongLat() const { return {m_rawData.longitude,m_rawData.latitude}; }
    bool IsTaxing() const { return m_trafficState == "STATE_SIMPLE_TAXI"; }
    bool HasParking() const { return !m_parkingState.empty(); }
    bool IsUser() const { return (m_rawData.isUser || (objectID == SIMCONNECT_OBJECT_ID_USER)); }
    void SetAirport(const GSAirport* ap) { m_airport = ap; }
    void SetParking(const GSRoadsNetwork::RoadNode* prkNode) { m_parkingNode = prkNode; }
    const GSAirport* GetAirport() const { return m_airport; }
    const GSRoadsNetwork::RoadNode* GetParking() const { return m_parkingNode; }

    using DoorRef = std::optional<std::pair<InterPntRef, size_t>>;
    DoorRef GetRearRightDoor() const { return rearRightDoorPnt; }
    DoorRef GetFrontRightDoor() const { return frontRightDoorPnt; }
    DoorRef GetRearLeftDoor() const { return rearLeftDoorPnt; }
    DoorRef GetFrontLeftDoor() const { return frontLeftDoorPnt; }
    DoorRef GetGroundPowerDoor() const { return groundPowerPnt; }
    DoorRef GetRearCargoDoor() const { return cargoDoorBackPnt; }
    DoorRef GetFrontCargoDoor() const { return cargoDoorFrontPnt; }
    
private:
    AircraftWireData m_rawData;
      
    DoorRef cargoDoorFrontPnt;
    DoorRef cargoDoorBackPnt;
    DoorRef rearLeftDoorPnt;
    DoorRef rearRightDoorPnt;
    DoorRef frontLeftDoorPnt;
    DoorRef frontRightDoorPnt;
    DoorRef groundPowerPnt;
    DoorRef fuelHosePnt;

    AircraftSizeCategory m_category;
    std::string m_trafficState;
    std::string m_parkingState;
    const GSAirport* m_airport;
    const GSRoadsNetwork::RoadNode* m_parkingNode;
    DWORD objectID{};
};

} // namespace NS_GSAliveAirport
