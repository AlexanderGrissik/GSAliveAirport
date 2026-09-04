#pragma once

#include "GSReqBase.h"

#include "../ISimConnectHandler.h"

#include <cstdint>
#include <string>
#include <vector>

namespace parking_services
{
// One TAXI_POINT record. The bias is relative to the airport ARP (meters,
// X = longitudinal/longitude, Z = latitudinal/latitude); when the ARP has been
// received, latitude/longitude are also filled in by ResolveEndpoints.
struct GSRoadPoint
{
    int type{};
    int orientation{};
    float biasXMeters{};
    float biasZMeters{};
    bool hasLocation{};
    double latitude{};
    double longitude{};
};

// One non-aircraft road (TAXI_PATH of type 6 VEHICLE or 7 ROAD) with its
// endpoints resolved to absolute latitude/longitude where possible.
struct GSRoad
{
    int type{};
    float widthMeters{};
    int startPointIndex{-1};
    int endPointIndex{-1};
    bool startResolved{};
    double startLatitude{};
    double startLongitude{};
    bool endResolved{};
    double endLatitude{};
    double endLongitude{};
};

struct GSRoadsResult
{
    bool succeeded{};
    bool hasAirport{};
    double airportLatitude{};
    double airportLongitude{};
    double airportAltitudeMeters{};
    int taxiPoints{};
    int taxiPaths{};
    std::vector<GSRoad> roads;
    int otherPathCount{};
    std::vector<GSRoadPoint> points;
};

// Request that drives a SimConnect facility-data query for the non-aircraft
// roads of a single airport: it owns the facility definition schema, issues the
// request through the (agnostic) SimConnect handler, accumulates the streamed
// AIRPORT / TAXI_PATH / TAXI_POINT records, and resolves the road endpoints.
class GSReqRoads final : public GSReqBase
{
  public:
    GSReqRoads();

    // Registers this request's facility definition (once) and issues the
    // facility-data request for the given airport ICAO.
    void Drive(ISimConnectHandler &handler, const std::string &icao);

    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override;
    [[nodiscard]] GSRoadsResult TakeResult();

  private:
    static void SetupFacilityDefinition(ISimConnectHandler &handler);
    void ResolveEndpointsLocked();

    GSRoadsResult m_result;
};
} // namespace parking_services