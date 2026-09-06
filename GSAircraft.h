#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace parking_services
{

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

        std::array<InteractivePointWireData, g_InteractivePointProbeCount> interactivePoints{};
    };
    #pragma pack(pop)

    constexpr std::size_t AIRCRAFT_WIREDATA_DYNSIZE = offsetof(AircraftWireData, title);

    static void InitDatums(SimConnectHandler& handler);

    void LoadDynamicState(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry);
    void LoadFullState(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry);
    void CopyDynInfo(const GSAircraft& another);
    
    bool operator==(const GSAircraft& another);
    bool operator!=(const GSAircraft& another) { return !(*this == another); }

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

} // namespace parking_services
