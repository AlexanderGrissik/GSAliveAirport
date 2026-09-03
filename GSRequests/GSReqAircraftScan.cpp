#include "GSReqAircraftScan.h"

#include "../SimObjectPositioning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

namespace
{
constexpr auto kRequestTimeout = 8s;
constexpr std::size_t kInteractivePointProbeCount = 32;
constexpr std::int32_t kCargoInteractivePointType = 1;

template <std::size_t Size> std::string FixedString(const std::array<char, Size> &value)
{
    const auto end = std::find(value.begin(), value.end(), '\0');
    return {value.data(), static_cast<std::size_t>(end - value.begin())};
}

template <typename Payload>
std::optional<Payload> ReadPayload(const SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE &entry,
                                   DWORD messageSize)
{
    const auto *entryBytes = reinterpret_cast<const BYTE *>(&entry);
    const auto *payloadBytes = reinterpret_cast<const BYTE *>(&entry.dwData);
    const std::size_t offset = static_cast<std::size_t>(payloadBytes - entryBytes);
    const std::size_t received = entry.dwSize != 0
                                     ? (std::min)(static_cast<std::size_t>(entry.dwSize),
                                                  static_cast<std::size_t>(messageSize))
                                     : static_cast<std::size_t>(messageSize);
    if (received < offset + sizeof(Payload)) return std::nullopt;

    Payload payload{};
    std::memcpy(&payload, payloadBytes, sizeof(payload));
    return payload;
}

#pragma pack(push, 1)
struct InteractivePointWireData
{
    std::int32_t type{};
    double posXFeet{};
    double posYFeet{};
    double posZFeet{};
    double headingDegrees{};
};

struct AircraftWireData
{
    std::array<char, 256> title{};
    std::array<char, 256> atcId{};
    std::array<char, 256> atcAirline{};
    std::array<char, 256> atcFlightNumber{};
    double latitude{};
    double longitude{};
    double altitudeFeet{};
    double groundAltitudeFeet{};
    double headingDegrees{};
    double groundSpeedKnots{};
    double wingSpanMeters{};
    std::int32_t onGround{};
    std::int32_t isUser{};
    std::array<char, 256> currentAirport{};
    std::array<char, 256> assignedParking{};
    std::array<char, 256> assignedRunway{};
    std::array<char, 256> fromAirport{};
    std::array<char, 256> toAirport{};
    std::int32_t etdSeconds{};
    std::int32_t etaSeconds{};
    std::array<char, 256> trafficState{};
    std::int32_t isIfr{};
    std::int32_t numberOfEngines{};
    std::array<std::int32_t, 4> engineCombustion{};
    std::array<std::int32_t, 4> engineStarterActive{};
    std::array<double, 4> engineN1Percent{};
    std::int32_t lightBeacon{};
    std::int32_t lightNav{};
    std::int32_t lightTaxi{};
    std::int32_t lightStrobe{};
    std::int32_t parkingBrake{};
    std::int32_t pushbackAttached{};
    std::int32_t pushbackWait{};
    std::int32_t transponderState{};
    std::array<InteractivePointWireData, kInteractivePointProbeCount> interactivePoints{};
};
#pragma pack(pop)

static_assert(sizeof(InteractivePointWireData) == 36);
static_assert(sizeof(AircraftWireData) == 3888);

AircraftSnapshot ToAircraft(DWORD objectId, const AircraftWireData &data)
{
    AircraftSnapshot aircraft{};
    aircraft.objectId = objectId;
    aircraft.title = FixedString(data.title);
    aircraft.atcId = FixedString(data.atcId);
    aircraft.atcAirline = FixedString(data.atcAirline);
    aircraft.atcFlightNumber = FixedString(data.atcFlightNumber);
    aircraft.latitude = data.latitude;
    aircraft.longitude = data.longitude;
    aircraft.altitudeFeet = data.altitudeFeet;
    aircraft.groundAltitudeFeet = data.groundAltitudeFeet;
    aircraft.headingDegrees = data.headingDegrees;
    aircraft.groundSpeedKnots = data.groundSpeedKnots;
    aircraft.wingSpanMeters = data.wingSpanMeters;
    aircraft.onGround = data.onGround != 0;
    aircraft.isUser = data.isUser != 0;
    aircraft.currentAirport = FixedString(data.currentAirport);
    aircraft.assignedParking = FixedString(data.assignedParking);
    aircraft.assignedRunway = FixedString(data.assignedRunway);
    aircraft.fromAirport = FixedString(data.fromAirport);
    aircraft.toAirport = FixedString(data.toAirport);
    aircraft.etdSeconds = data.etdSeconds;
    aircraft.etaSeconds = data.etaSeconds;
    aircraft.trafficState = FixedString(data.trafficState);
    aircraft.isIfr = data.isIfr != 0;
    aircraft.numberOfEngines = data.numberOfEngines;
    for (std::size_t index = 0; index < aircraft.engineCombustion.size(); ++index) {
        aircraft.engineCombustion[index] = data.engineCombustion[index];
        aircraft.engineStarterActive[index] = data.engineStarterActive[index];
        aircraft.engineN1Percent[index] = data.engineN1Percent[index];
    }
    aircraft.lightBeacon = data.lightBeacon != 0;
    aircraft.lightNav = data.lightNav != 0;
    aircraft.lightTaxi = data.lightTaxi != 0;
    aircraft.lightStrobe = data.lightStrobe != 0;
    aircraft.parkingBrake = data.parkingBrake != 0;
    aircraft.pushbackAttached = data.pushbackAttached != 0;
    aircraft.pushbackWait = data.pushbackWait != 0;
    aircraft.transponderState = data.transponderState;
    for (std::size_t index = 0; index < data.interactivePoints.size(); ++index) {
        const InteractivePointWireData &point = data.interactivePoints[index];
        if (point.type != kCargoInteractivePointType || !std::isfinite(point.posXFeet) ||
            !std::isfinite(point.posYFeet) || !std::isfinite(point.posZFeet) ||
            !std::isfinite(point.headingDegrees)) {
            continue;
        }
        const AircraftCargoConnectionPoint candidate{
            point.posZFeet * kFeetToMeters, point.posXFeet * kFeetToMeters,
            point.posYFeet * kFeetToMeters, point.headingDegrees,
            static_cast<std::uint32_t>(index)};
        if (candidate.rightMeters <= 0.0) continue;
        if (!aircraft.cargoDoorRightFront ||
            candidate.forwardMeters > aircraft.cargoDoorRightFront->forwardMeters) {
            aircraft.cargoDoorRightFront = candidate;
        }
        if (!aircraft.cargoDoorRightBack ||
            candidate.forwardMeters < aircraft.cargoDoorRightBack->forwardMeters) {
            aircraft.cargoDoorRightBack = candidate;
        }
    }
    return aircraft;
}
} // namespace

GSReqAircraftScan::GSReqAircraftScan() : GSReqBase(kRequestTimeout) {}

void GSReqAircraftScan::OnMessage(SIMCONNECT_RECV *message, DWORD messageSize)
{
    std::scoped_lock lock(m_mutex);
    if (!PendingLocked()) return;
    if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE) {
        CompleteLocked(false);
        return;
    }

    const auto &entry = *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *>(message);
    const auto payload = ReadPayload<AircraftWireData>(entry, messageSize);
    if (!payload) {
        CompleteLocked(false);
        return;
    }
    if (entry.dwentrynumber == 1) m_batch.clear();
    m_batch[entry.dwObjectID] = ToAircraft(entry.dwObjectID, *payload);
    if (entry.dwoutof != 0 && entry.dwentrynumber < entry.dwoutof) return;

    m_aircraft.clear();
    m_aircraft.reserve(m_batch.size());
    for (auto &[objectId, aircraft] : m_batch) {
        static_cast<void>(objectId);
        m_aircraft.push_back(std::move(aircraft));
    }
    CompleteLocked(true);
}

GSAircraftScanResult GSReqAircraftScan::TakeResult()
{
    std::scoped_lock lock(m_mutex);
    ExpireLocked();
    GSAircraftScanResult result{m_state == State::Succeeded};
    if (result.succeeded) result.aircraft = std::move(m_aircraft);
    return result;
}
} // namespace parking_services
