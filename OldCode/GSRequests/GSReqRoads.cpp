#include "GSReqRoads.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kRequestTimeout = 10s;

// Facility-data definition IDs live in a separate namespace from the SimObject
// data definitions (1,2,3,6) and client-event IDs used elsewhere in the app.
constexpr SIMCONNECT_DATA_DEFINITION_ID kRoadsFacilityDefinitionId = 1000;

// Non-aircraft taxi-path type codes (TAXI_PATH.TYPE).
constexpr int kRoadTypeVehicle = 6;
constexpr int kRoadTypeRoad = 7;

// Meters-per-degree conversion factors for resolving ARP-relative biases.
constexpr double kMetersPerDegreeLatitude = 111'132.0;
constexpr double kMetersPerDegreeLongitude = 111'320.0;
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;

// The facility definition: field order below must match the wire structs, which
// SimConnect returns in the same order they were requested.
const char *kSchema[] = {
    "OPEN AIRPORT",
    "LATITUDE",      // FLOAT64
    "LONGITUDE",     // FLOAT64
    "ALTITUDE",      // FLOAT64
    "N_TAXI_POINTS", // INT32
    "N_TAXI_PATHS",  // INT32
    "OPEN TAXI_PATH",
    "TYPE",   // INT32
    "WIDTH",  // FLOAT32
    "START",  // INT32
    "END",    // INT32
    "CLOSE TAXI_PATH",
    "OPEN TAXI_POINT",
    "TYPE",        // INT32
    "ORIENTATION", // INT32
    "BIAS_X",      // FLOAT32 (meters, longitudinal/longitude)
    "BIAS_Z",      // FLOAT32 (meters, latitudinal/latitude)
    "CLOSE TAXI_POINT",
    "CLOSE AIRPORT",
};

#pragma pack(push, 1)
struct FacilityAirport
{
    double latitude{};
    double longitude{};
    double altitude{};
    int nTaxiPoints{};
    int nTaxiPaths{};
};
struct FacilityTaxiPath
{
    int type{};
    float width{};
    int startIndex{};
    int endIndex{};
};
struct FacilityTaxiPoint
{
    int type{};
    int orientation{};
    float biasXMeters{};
    float biasZMeters{};
};
#pragma pack(pop)

static_assert(sizeof(FacilityAirport) == 32);
static_assert(sizeof(FacilityTaxiPath) == 16);
static_assert(sizeof(FacilityTaxiPoint) == 16);

template <typename Payload>
const Payload *ReadFacilityPayload(const SIMCONNECT_RECV_FACILITY_DATA &header,
                                   DWORD messageSize)
{
    const auto *headerBytes = reinterpret_cast<const BYTE *>(&header);
    const auto *dataBytes = reinterpret_cast<const BYTE *>(&header.Data);
    const std::size_t offset = static_cast<std::size_t>(dataBytes - headerBytes);
    const std::size_t received = header.dwSize != 0
        ? (std::min)(static_cast<std::size_t>(header.dwSize),
                     static_cast<std::size_t>(messageSize))
        : static_cast<std::size_t>(messageSize);
    if (received < offset + sizeof(Payload)) return nullptr;
    return reinterpret_cast<const Payload *>(dataBytes);
}
} // namespace

GSReqRoads::GSReqRoads() : GSReqBase(kRequestTimeout) {}

void GSReqRoads::SetupFacilityDefinition(ISimConnectHandler &handler)
{
    for (const char *field : kSchema) {
        handler.DefineFacilityDataField(kRoadsFacilityDefinitionId, field);
    }
}

void GSReqRoads::Drive(ISimConnectHandler &handler, const std::string &icao)
{
    SetupFacilityDefinition(handler);
    handler.RequestFacilityData(icao, kRoadsFacilityDefinitionId, *this);
}
void GSReqRoads::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    std::scoped_lock lock(m_mutex);
    if (!PendingLocked()) return;
    if (!message) return;

    if (message->dwID == SIMCONNECT_RECV_ID_FACILITY_DATA_END) {
        if (messageSize < sizeof(SIMCONNECT_RECV_FACILITY_DATA_END)) return;
        ResolveEndpointsLocked();
        CompleteLocked(true);
        return;
    }

    if (message->dwID != SIMCONNECT_RECV_ID_FACILITY_DATA) return;
    if (messageSize < sizeof(SIMCONNECT_RECV_FACILITY_DATA)) return;
    const auto &header = *reinterpret_cast<const SIMCONNECT_RECV_FACILITY_DATA *>(message);

    switch (header.Type) {
    case SIMCONNECT_FACILITY_DATA_AIRPORT: {
        const auto *p = ReadFacilityPayload<FacilityAirport>(header, messageSize);
        if (!p) { CompleteLocked(false); return; }
        m_result.hasAirport = true;
        m_result.airportLatitude = p->latitude;
        m_result.airportLongitude = p->longitude;
        m_result.airportAltitudeMeters = p->altitude;
        m_result.taxiPoints = p->nTaxiPoints;
        m_result.taxiPaths = p->nTaxiPaths;
        break;
    }
    case SIMCONNECT_FACILITY_DATA_TAXI_PATH: {
        const auto *p = ReadFacilityPayload<FacilityTaxiPath>(header, messageSize);
        if (!p) { CompleteLocked(false); return; }
        if (p->type == kRoadTypeVehicle || p->type == kRoadTypeRoad) {
            m_result.roads.push_back(
                {p->type, p->width, p->startIndex, p->endIndex, false, 0, 0, false, 0, 0});
        } else {
            ++m_result.otherPathCount;
        }
        break;
    }
    case SIMCONNECT_FACILITY_DATA_TAXI_POINT: {
        const auto *p = ReadFacilityPayload<FacilityTaxiPoint>(header, messageSize);
        if (!p) { CompleteLocked(false); return; }
        const std::size_t index = header.IsListItem != 0
            ? static_cast<std::size_t>(header.ItemIndex)
            : m_result.points.size();
        if (index >= m_result.points.size()) m_result.points.resize(index + 1);
        m_result.points[index] = {p->type, p->orientation, p->biasXMeters, p->biasZMeters,
                                  false, 0, 0};
        break;
    }
    default:
        break;
    }
}

void GSReqRoads::ResolveEndpointsLocked()
{
    if (!m_result.hasAirport) return;
    const double arpLatitude = m_result.airportLatitude;
    const double arpLongitude = m_result.airportLongitude;
    const double cosLatitude = std::cos(arpLatitude * kDegreesToRadians);

    // Resolve each taxi point from its ARP-relative bias into absolute lat/lon.
    for (auto &point : m_result.points) {
        point.hasLocation = true;
        point.latitude = arpLatitude + point.biasZMeters / kMetersPerDegreeLatitude;
        point.longitude =
            arpLongitude + point.biasXMeters / (kMetersPerDegreeLongitude * cosLatitude);
    }

    // Attach the resolved endpoints to each road by its start/end point index.
    for (auto &road : m_result.roads) {
        if (road.startPointIndex >= 0 &&
            static_cast<std::size_t>(road.startPointIndex) < m_result.points.size()) {
            const auto &point = m_result.points[road.startPointIndex];
            road.startResolved = point.hasLocation;
            road.startLatitude = point.latitude;
            road.startLongitude = point.longitude;
        }
        if (road.endPointIndex >= 0 &&
            static_cast<std::size_t>(road.endPointIndex) < m_result.points.size()) {
            const auto &point = m_result.points[road.endPointIndex];
            road.endResolved = point.hasLocation;
            road.endLatitude = point.latitude;
            road.endLongitude = point.longitude;
        }
    }
}

GSRoadsResult GSReqRoads::TakeResult()
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    GSRoadsResult result = m_result;
    result.succeeded = (m_state == State::Succeeded);
    if (!result.succeeded) {
        result.roads.clear();
        result.points.clear();
    }
    return result;
}
} // namespace parking_services