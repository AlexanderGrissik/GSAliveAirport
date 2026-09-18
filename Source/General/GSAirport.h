#pragma once
#include "GSRequest.h"
#include "GSSimConnect.h"
#include "GSCoord.h"
#include <string>

namespace NS_GSLiveAirportMSFS
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
    #pragma pack(pop)

    static_assert(sizeof(ParkingSlot) == 32);
    static_assert(sizeof(TaxiPoint) == 12);
    static_assert(sizeof(TaxiPath) == 16);

    GSAirport(const std::string& icao, const GSCoord& loc): m_icao(icao), m_location(loc)  {}

    static void InitDatums(GSSimConnect& handler);

    void LoadInfo(GSSimConnect& handler);
    const std::string& GetICAO() const { return m_icao; } 
    const GSCoord& GetLongLat() const { return m_location; }

private:

    std::string m_icao;
    GSCoord m_location;
    std::vector<ParkingSlot> m_parkings;
    std::vector<TaxiPoint> m_taxiPoints;
    std::vector<TaxiPath> m_taxiPaths;

    class GSReqInfo : public GSRequest
    {
    public:
        GSReqInfo(GSSimConnect& handler, GSAirport& airport)
            : GSRequest(handler), m_airport(airport) {}

        GSRequest::SendResult Process() override;
        bool OnMessage(SIMCONNECT_RECV* message, DWORD messageSize) override;
        void OnException(SIMCONNECT_RECV_EXCEPTION* message) override;

    private:
        GSAirport& m_airport;
    };

    friend GSReqInfo;
};

}