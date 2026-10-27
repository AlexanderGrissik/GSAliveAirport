// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

#include "../General/GSDefinitions.h"
#include "../General/GSCoord.h"
#include "../General/GSSimConnect.h"
#include "../General/GSAirport.h"
#include "../General/GSStringUtils.h"
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

    static constexpr std::size_t s_MaxInteractivePnts = 12;

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
        std::array<char, 32> title{};
        double latitude{};
        double longitude{};
        std::int32_t onGround{};
        std::int32_t isUser{};
        float groundSpeedKnots{};

        float altitudeFeet{};
        float groundAltitudeFeet{};
        float headingDegrees{};
        float alt_abv_grnd{};
        float alt_abv_grnd_minus_cg{};
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
        std::int32_t pushbackAttached{};
        std::int32_t pushbackWait{};
    };

    struct AircraftWireDataConst
    {
        std::array<char, 32> atcId{};
        std::array<char, 32> atcAirline{};
        std::array<char, 32> atcFlightNumber{};
        std::int32_t numberOfEngines{};
        double wingSpanMeters{};
        float pushbackContactXMeters{};
        float pushbackContactZMeters{};

        std::array<InteractivePointWireData, s_MaxInteractivePnts> interactivePoints{};
    };
    #pragma pack(pop)

    static constexpr std::size_t AIRCRAFT_WIREDATA_INITSIZE = offsetof(AircraftWireData, altitudeFeet);
    static constexpr std::size_t AIRCRAFT_WIREDATA_DYNSIZE = sizeof(AircraftWireData) - AIRCRAFT_WIREDATA_INITSIZE;
    static constexpr std::size_t AIRCRAFT_USER_DYNSIZE = offsetof(AircraftWireData, fromAirport);

    using InterPntRef = std::reference_wrapper<const InteractivePointWireData>;
    
    static void InitDatums(GSSimConnect& handler);
    static bool IsValidInteractivePointType(std::int32_t type) { return type == 0 || type == 1 || type == 3 || type == 4; }
    static const char *InteractivePointTypeName(std::int32_t type);
    static const char *AircraftSizeName(GSAircraft::AircraftSizeCategory category);

    void LoadInitState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE& entry);
    void LoadDynamicState(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE& entry);
    void LoadFullState(const SIMCONNECT_RECV_SIMOBJECT_DATA& entry);
    void CopyDynInfo(const GSAircraft& another);
    
    bool operator==(const GSAircraft& another) const;
    bool operator!=(const GSAircraft& another) const { return !(*this == another); }

    bool IsParkedActive() const;
    AircraftSizeCategory GetCategory() const { return m_category; }
    DWORD GetObjID() const { return objectID; }
    const GSCoord& GetLongLat() const { return m_location; }
    float GetAltitudeFeet() const { return m_rawData.altitudeFeet; }
    float GetGroundAltitudeFeet() const { return m_rawData.groundAltitudeFeet; }
    float GetAltitudeAboveGroundFeet() const { return m_rawData.alt_abv_grnd; }
    float GetAltitudeAboveGroundMinusCgFeet() const { return m_rawData.alt_abv_grnd_minus_cg; }
    float GetHeading() const { return m_rawData.headingDegrees; }
    float GetGroundSpeedKnots() const { return m_rawData.groundSpeedKnots; }
    bool IsTaxing() const { return m_trafficState == "STATE_SIMPLE_TAXI"; }
    bool HasParking() const { return !m_parkingState.empty(); }
    bool IsUser() const { return (m_rawData.isUser || (objectID == SIMCONNECT_OBJECT_ID_USER)); }
    bool IsDeparting() const { return m_departing; } 
    void SetAirport(std::shared_ptr<GSAirport>& ap) { m_airport = ap; }
    void SetParking(const GSRoadsNetwork::RoadNode* prkNode) { m_parkingNode = prkNode; }
    std::string GetTitleLower() const { return GSStringUtils::ToLower(m_rawData.title); }
    std::string GetTitle() const { return m_rawData.title.data(); }
    bool IsOnGround() const { return !!m_rawData.onGround; }
    const GSAirport* GetAirport() const { return m_airport.get(); }
    const GSRoadsNetwork::RoadNode* GetParking() const { return m_parkingNode; }

    using DoorRef = std::optional<std::pair<InterPntRef, size_t>>;
    DoorRef GetRearRightDoor() const { return rearRightDoorPnt; }
    DoorRef GetFrontRightDoor() const { return frontRightDoorPnt; }
    DoorRef GetRearLeftDoor() const { return rearLeftDoorPnt; }
    DoorRef GetFrontLeftDoor() const { return frontLeftDoorPnt; }
    DoorRef GetGroundPowerDoor() const { return groundPowerPnt; }
    DoorRef GetRearCargoDoor() const { return cargoDoorBackPnt; }
    DoorRef GetFrontCargoDoor() const { return cargoDoorFrontPnt; }
    
    std::string GetAICurrAirport() const { return m_rawData.currentAirport.data(); }
    std::string GetFromAirport() const { return m_rawData.fromAirport.data(); }
    std::string GetToAirport() const { return m_rawData.toAirport.data(); }
    std::string GetATCID() const { return m_rawConstData.atcId.data(); }
    std::string GetATCAirline() const { return m_rawConstData.atcAirline.data(); }
    std::string GetARCFlightNumber() const { return m_rawConstData.atcFlightNumber.data(); }

    const std::string& GetTrafficState() const { return m_trafficState; }
    const std::string& GetAssignedParking() const { return m_parkingState; }
    std::int32_t GetETDSeconds() const { return m_rawData.etdSeconds; }
    std::int32_t GetETASeconds() const { return m_rawData.etaSeconds; }
    std::int32_t GetNumberOfEngines() const { return m_rawConstData.numberOfEngines; }
    bool IsLightBeaconOn() const { return (m_rawData.lightBeacon != 0); }
    bool IsLightNavOn() const { return (m_rawData.lightNav != 0); }
    bool IsLightTaxiOn() const { return (m_rawData.lightTaxi != 0); }
    bool IsParkingBrakeSet() const { return (m_rawData.parkingBrake != 0); }
    bool IsPushbackAttached() const { return (m_rawData.pushbackAttached != 0); }
    bool IsPushbackWait() const { return (m_rawData.pushbackWait != 0); }
    double GetWingSpanMeters() const { return m_rawConstData.wingSpanMeters; }
    float GetPushbackContactXMeters() const { return m_rawConstData.pushbackContactXMeters; }
    float GetPushbackContactZMeters() const { return m_rawConstData.pushbackContactZMeters; }
    const std::array<InteractivePointWireData, s_MaxInteractivePnts>& GetInteractivePoints() const { return m_rawConstData.interactivePoints; }

private:
    AircraftWireData m_rawData{};
    AircraftWireDataConst m_rawConstData{};

    AircraftSizeCategory m_category{};
    DoorRef cargoDoorFrontPnt;
    DoorRef cargoDoorBackPnt;
    DoorRef rearLeftDoorPnt;
    DoorRef rearRightDoorPnt;
    DoorRef frontLeftDoorPnt;
    DoorRef frontRightDoorPnt;
    DoorRef groundPowerPnt;
    DoorRef fuelHosePnt;

    DWORD objectID{};
    bool m_departing = true;
    GSCoord m_location{-999.0,-999.0};
    std::string m_trafficState;
    std::string m_parkingState;
    std::shared_ptr<GSAirport> m_airport;
    const GSRoadsNetwork::RoadNode* m_parkingNode = nullptr;
};

} // namespace NS_GSAliveAirport
