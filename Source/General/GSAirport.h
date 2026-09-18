#pragma once
#include "GSRequest.h"
#include "GSSimConnect.h"
#include "GSCoord.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

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

    struct Jetway {
        std::int32_t parkingGate;   // PARKING_GATE
        std::int32_t parkingSuffix; // PARKING_SUFFIX
        std::int32_t parkingSpot;   // PARKING_SPOT
    };
    #pragma pack(pop)

    struct TaxiPathExt;
     
    struct TaxiPointExt : public TaxiPoint {
        TaxiPointExt(DWORD idx, const TaxiPoint& pnt): TaxiPoint(pnt), m_itemIndex(idx) {}
        DWORD m_itemIndex;
        GSCoord m_longLat;
        std::vector<TaxiPathExt*> m_conns;
    };

    struct ParkingSlotExt : public ParkingSlot {
        ParkingSlotExt(DWORD idx, const ParkingSlot& slot): ParkingSlot(slot), m_itemIndex(idx) {}
        DWORD m_itemIndex;
        GSCoord m_longLat;
        TaxiPointExt* m_taxiPoint = nullptr;
        Jetway* m_jetway = nullptr;
    };

    struct TaxiPathExt : public TaxiPath {
        TaxiPathExt(const TaxiPath& path): TaxiPath(path) {}
        TaxiPointExt* m_nodeA = nullptr;
        TaxiPointExt* m_nodeB = nullptr;
    };

    static_assert(sizeof(ParkingSlot) == 32);
    static_assert(sizeof(TaxiPoint) == 12);
    static_assert(sizeof(TaxiPath) == 16);
    static_assert(sizeof(Jetway) == 12);

    GSAirport(const std::string& icao, const GSCoord& loc): m_icao(icao), m_location(loc)  {}

    static void InitDatums(GSSimConnect& handler);

    void LoadInfo(GSSimConnect& handler);
    void OrganizeStructures();
    const std::string& GetICAO() const { return m_icao; } 
    const GSCoord& GetLongLat() const { return m_location; }
   
private:

    static bool NetworkContainsNode(const std::vector<TaxiPathExt*>& network, DWORD nodeIndex);
    static void MergeNetworks(std::vector<std::vector<TaxiPathExt*>>& networks, std::size_t targetIndex, std::size_t sourceIndex);

    std::string m_icao;
    GSCoord m_location;
    std::unordered_map<DWORD, ParkingSlotExt> m_parkings;
    std::unordered_map<DWORD, TaxiPointExt> m_taxiPoints;
    std::unordered_map<DWORD, Jetway> m_jetways;
    std::vector<TaxiPathExt> m_taxiPaths;
    std::vector<std::vector<TaxiPathExt*>> m_roadNetworks;

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
