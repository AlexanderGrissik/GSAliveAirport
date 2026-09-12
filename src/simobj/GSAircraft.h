#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

#include "../GSCoord.h"
#include <array>
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace NS_GSLiveAirportMSFS
{

class GSSimConnect;

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
        float posYMeter{};
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

        std::array<char, 32> title{};
        std::int32_t isUser{};
        std::int32_t numberOfEngines{};
        std::int32_t pushbackAttached{};
        std::int32_t pushbackWait{};
        double wingSpanMeters{};
        float pushbackContactXMeters{};
        float pushbackContactZMeters{};

        std::array<InteractivePointWireData, 12> interactivePoints{};
    };
    #pragma pack(pop)

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
    bool IsTaxing() const { return false; }
    bool IsUser() const { return (m_rawData.isUser || (objectID == SIMCONNECT_OBJECT_ID_USER)); }

private:
    AircraftWireData m_rawData;
    
    using InterPntRef = std::reference_wrapper<const InteractivePointWireData>;
    std::optional<InterPntRef> cargoDoorFrontPnt;
    std::optional<InterPntRef> cargoDoorBackPnt;
    std::optional<InterPntRef> rearLeftDoorPnt;
    std::optional<InterPntRef> rearRightDoorPnt;
    std::optional<InterPntRef> frontLeftDoorPnt;
    std::optional<InterPntRef> frontRightDoorPnt;
    std::optional<InterPntRef> groundPowerPnt;
    std::optional<InterPntRef> fuelHosePnt;

    AircraftSizeCategory m_category;
    DWORD objectID{};
};

} // namespace NS_GSLiveAirportMSFS
