#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable : 4245)
#include <SimConnect.h>
#pragma warning(pop)

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

    enum class AircraftSizeCategory
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
        float groundSpeedKnots{};
        std::array<char, 8> currentAirport{};
        std::array<char, 8> assignedRunway{};
        std::array<char, 8> fromAirport{};
        std::array<char, 8> toAirport{};
        std::array<char, 32> assignedParking{};
        std::array<char, 32> trafficState{};
        std::int32_t etdSeconds{};
        std::int32_t etaSeconds{};
        std::int32_t lightBeacon{};
        std::int8_t lightNav{};
        std::int8_t lightTaxi{};
        std::int8_t parkingBrake{};
        std::int8_t onGround{};

        std::array<char, 32> title{};
        std::int8_t isUser{};
        std::int8_t numberOfEngines{};
        std::int8_t pushbackAttached{};
        std::int8_t pushbackWait{};
        std::int32_t wingSpanMeters{};
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
    double LateralDistanceFrom(const GSAircraft* ac) const;

    void Print() const;

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
