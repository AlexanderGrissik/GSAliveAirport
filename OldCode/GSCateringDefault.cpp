#include "GSCateringDefault.h"

#include "GSCommon.h"
#include "GSRequests/GSReqBase.h"
#include "ISimConnectHandler.h"

#include <algorithm>
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
constexpr auto kStateRequestTimeout = 4s;
constexpr auto kActivationTimeout = 35s;
constexpr auto kStatePollInterval = 500ms;
constexpr double kElevationToleranceMeters = 0.05;
constexpr double kOpeningComplete = 0.98;
constexpr double kMinimumDoorContactOffsetMeters = 0.1;
constexpr double kMaximumDoorContactOffsetMeters = 50.0;

#pragma pack(push, 1)
struct CateringStateWireData
{
    double elevationCurrentMeters{};
    double elevationTargetMeters{};
    double openingCurrent{};
    double openingTarget{};
    double aircraftDoorContactOffsetZMeters{};
};
#pragma pack(pop)

static_assert(sizeof(CateringStateWireData) == 40);

std::optional<CateringStateWireData> ReadCateringStatePayload(
    const SIMCONNECT_RECV_SIMOBJECT_DATA &entry, DWORD messageSize)
{
    const auto *entryBytes = reinterpret_cast<const BYTE *>(&entry);
    const auto *payloadBytes = reinterpret_cast<const BYTE *>(&entry.dwData);
    const std::size_t offset = static_cast<std::size_t>(payloadBytes - entryBytes);
    const std::size_t received = entry.dwSize != 0
                                     ? (std::min)(static_cast<std::size_t>(entry.dwSize),
                                                  static_cast<std::size_t>(messageSize))
                                     : static_cast<std::size_t>(messageSize);
    if (received < offset + sizeof(CateringStateWireData)) return std::nullopt;

    CateringStateWireData payload{};
    std::memcpy(&payload, payloadBytes, sizeof(payload));
    return payload;
}
} // namespace

class GSReqCateringState final : public GSReqBase
{
  public:
    GSReqCateringState() : GSReqBase(kStateRequestTimeout) {}

    void OnMessage(SIMCONNECT_RECV *message, DWORD messageSize) override
    {
        std::scoped_lock lock(m_mutex);
        if (!PendingLocked()) return;
        if (!message || message->dwID != SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
            CompleteLocked(false);
            return;
        }

        const auto &entry =
            *reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA *>(message);
        const auto payload = ReadCateringStatePayload(entry, messageSize);
        if (!payload) {
            CompleteLocked(false);
            return;
        }
        m_result = *payload;
        CompleteLocked(true);
    }

    [[nodiscard]] std::optional<CateringStateWireData> Result() const
    {
        std::scoped_lock lock(m_mutex);
        ExpireLocked();
        if (m_state != State::Succeeded) return std::nullopt;
        return m_result;
    }

  private:
    CateringStateWireData m_result{};
};

GSCateringDefault::GSCateringDefault(AircraftSnapshot aircraft,
                                     GroundServiceObject object,
                                     GroundServiceLocation location)
    : GSObject(std::move(aircraft), std::move(object), std::move(location))
{
}

GSCateringDefault::~GSCateringDefault() = default;

const AircraftCargoConnectionPoint *GSCateringDefault::SelectRearRightDoor(
    const std::vector<AircraftCargoConnectionPoint> &exits)
{
    const AircraftCargoConnectionPoint *best = nullptr;
    for (const auto &exit : exits) {
        if (exit.rightMeters <= 0.0) continue;
        if (!best || exit.forwardMeters < best->forwardMeters) {
            best = &exit;
        }
    }
    return best;
}

bool GSCateringDefault::PreparePlacement(GSObjectServices &)
{
    const AircraftCargoConnectionPoint *door =
        SelectRearRightDoor(m_aircraft.mainExits);
    if (!door) {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": MSFS reported no rear-right passenger (main exit) door.");
        return false;
    }
    m_door = *door;
    m_pose = RelativeToAircraft(m_aircraft, door->rightMeters, door->forwardMeters,
                                false);
    m_pose.headingDegrees =
        NormalizeDegrees(m_aircraft.headingDegrees +
                         door->relativeHeadingDegrees + 180.0);
    m_doorSillMeters =
        (m_aircraft.altitudeFeet - m_aircraft.groundAltitudeFeet) * kFeetToMeters +
        door->verticalMeters;
    return true;
}

void GSCateringDefault::ConfigureSimConnect(GSObjectServices &services)
{
    if (m_simConnectConfigured || !services.simConnect) return;
    m_simConnectConfigured = true;

    m_positionDefinition = services.allocateDataDefinition();
    m_elevationTargetDefinition = services.allocateDataDefinition();
    m_openingTargetDefinition = services.allocateDataDefinition();
    m_stateDefinition = services.allocateDataDefinition();
    m_freezeLatitudeLongitudeEvent = services.allocateClientEvent();
    m_freezeAltitudeEvent = services.allocateClientEvent();
    m_freezeAttitudeEvent = services.allocateClientEvent();
    m_openAircraftDoorsEvent = services.allocateClientEvent();
    m_closeAircraftDoorsEvent = services.allocateClientEvent();

    ISimConnectHandler &simConnect = *services.simConnect;
    simConnect.AddDatum(m_positionDefinition, "Initial Position", "",
                        SIMCONNECT_DATATYPE_INITPOSITION, NewCommandRequest());
    simConnect.AddDatum(m_elevationTargetDefinition, "CATERINGTRUCK ELEVATION TARGET",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64, NewCommandRequest());
    simConnect.AddDatum(m_openingTargetDefinition, "CATERINGTRUCK OPENING TARGET",
                        "percent over 100", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_stateDefinition, "CATERINGTRUCK ELEVATION CURRENT",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_stateDefinition, "CATERINGTRUCK ELEVATION TARGET",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_stateDefinition, "CATERINGTRUCK OPENING CURRENT",
                        "percent over 100", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_stateDefinition, "CATERINGTRUCK OPENING TARGET",
                        "percent over 100", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_stateDefinition,
                        "CATERINGTRUCK AIRCRAFT DOOR CONTACT OFFSET Z",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.MapClientEvent(m_freezeLatitudeLongitudeEvent,
                              "FREEZE_LATITUDE_LONGITUDE_SET", NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAltitudeEvent, "FREEZE_ALTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAttitudeEvent, "FREEZE_ATTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_openAircraftDoorsEvent, "OPEN_AIRCRAFT_DOORS",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_closeAircraftDoorsEvent, "CLOSE_AIRCRAFT_DOORS",
                              NewCommandRequest());
}

void GSCateringDefault::Activate(GSObjectServices &services, AircraftId objectId)
{
    if (!services.simConnect || !m_door || !m_simConnectConfigured) {
        Finish(services, m_pose);
        return;
    }

    SetAircraftDoor(services, true);
    FreezeObject(services, objectId);
    SetOpening(services, objectId, false);

    const auto now = std::chrono::steady_clock::now();
    m_stage = Stage::AlignDoorContact;
    m_stateRequestDue = now + 250ms;
    m_activationDeadline = now + kActivationTimeout;
    m_elevationTargetMismatchCount = 0;
    m_haveState = false;
    m_haveInitialElevation = false;
}

void GSCateringDefault::Maintain(
    GSObjectServices &services, std::chrono::steady_clock::time_point now)
{
    if (m_finalized || m_objectId == 0) return;

    if (m_stateRequest) {
        if (!m_stateRequest->IsFinished()) return;

        const auto state = m_stateRequest->Result();
        m_stateRequest.reset();
        if (state && std::isfinite(state->elevationCurrentMeters) &&
            std::isfinite(state->elevationTargetMeters) &&
            std::isfinite(state->openingCurrent) &&
            std::isfinite(state->openingTarget) &&
            std::isfinite(state->aircraftDoorContactOffsetZMeters)) {
            m_haveState = true;
            m_lastElevationCurrent = state->elevationCurrentMeters;
            m_lastElevationTarget = state->elevationTargetMeters;
            m_lastOpeningCurrent = state->openingCurrent;
            if (!m_haveInitialElevation) {
                m_initialElevationMeters = state->elevationCurrentMeters;
                m_haveInitialElevation = true;
            }

            if (m_stage == Stage::AlignDoorContact) {
                const double contactOffset =
                    state->aircraftDoorContactOffsetZMeters;
                if (std::abs(contactOffset) >=
                        kMinimumDoorContactOffsetMeters &&
                    std::abs(contactOffset) <=
                        kMaximumDoorContactOffsetMeters) {
                    // The configured spawn point is the aircraft door. Move the
                    // vehicle origin away from it so the model's fully extended
                    // bridge contact point, not the truck origin, lands there.
                    const auto alignedPosition = RelativePosition(
                        m_pose.headingDegrees, m_pose.longitude, m_pose.latitude,
                        m_pose.altitudeFeet, -contactOffset, 0.0);
                    m_pose.latitude = alignedPosition.Latitude;
                    m_pose.longitude = alignedPosition.Longitude;
                    SetPosition(services, m_objectId);
                    SetElevation(services, m_objectId, m_doorSillMeters);
                    m_stage = Stage::WaitForElevation;
                    GSLog("Aligned catering truck ObjectID " +
                          std::to_string(m_objectId) +
                          " using aircraft-door contact offset Z " +
                          std::to_string(contactOffset) + " m.");
                }
            } else if (m_stage == Stage::WaitForElevation) {
                const bool requestedHeightReached =
                    std::abs(state->elevationCurrentMeters - m_doorSillMeters) <=
                    kElevationToleranceMeters;
                const bool targetMatchesRequest =
                    std::abs(state->elevationTargetMeters - m_doorSillMeters) <=
                    kElevationToleranceMeters;

                if (targetMatchesRequest) {
                    m_elevationTargetMismatchCount = 0;
                } else {
                    ++m_elevationTargetMismatchCount;
                    SetElevation(services, m_objectId, m_doorSillMeters);
                }

                // The model clamps an out-of-range target to its own min/max.
                // Accept that limit only after repeated writes and observable
                // movement, so a dropped first write cannot masquerade as a
                // successfully reached target.
                const bool clampedHeightReached =
                    m_elevationTargetMismatchCount >= 6 &&
                    std::abs(state->elevationCurrentMeters -
                             state->elevationTargetMeters) <=
                        kElevationToleranceMeters &&
                    std::abs(state->elevationTargetMeters -
                             m_initialElevationMeters) >
                        kElevationToleranceMeters;

                if (requestedHeightReached || clampedHeightReached) {
                    if (clampedHeightReached) {
                        GSLog("Catering truck ObjectID " +
                              std::to_string(m_objectId) +
                              " clamped requested elevation " +
                              std::to_string(m_doorSillMeters) + " m to " +
                              std::to_string(state->elevationTargetMeters) +
                              " m.");
                    }
                    SetOpening(services, m_objectId, true);
                    m_stage = Stage::WaitForOpening;
                }
            } else {
                if (state->openingCurrent >= kOpeningComplete) {
                    Finish(services, m_pose);
                    GSLog("Raised catering truck ObjectID " +
                          std::to_string(m_objectId) + " to " +
                          std::to_string(state->elevationCurrentMeters) +
                          " m and deployed its bridge.");
                    return;
                }
                if (state->openingTarget < kOpeningComplete) {
                    SetOpening(services, m_objectId, true);
                }
            }
        }

        now = std::chrono::steady_clock::now();
        m_stateRequestDue = now + kStatePollInterval;
    }

    if (now >= m_activationDeadline) {
        FinishAfterTimeout(services);
        return;
    }
    if (now >= m_stateRequestDue) RequestState(services);
}

void GSCateringDefault::RequestState(GSObjectServices &services)
{
    if (!services.simConnect || m_stateRequest) return;
    m_stateRequest = std::make_unique<GSReqCateringState>();
    services.simConnect->RequestObjectData(
        m_stateDefinition, m_objectId, SIMCONNECT_PERIOD_ONCE, 0,
        *m_stateRequest);
}

void GSCateringDefault::FinishAfterTimeout(GSObjectServices &services)
{
    if (m_stage == Stage::AlignDoorContact) {
        SetElevation(services, m_objectId, m_doorSillMeters);
        SetOpening(services, m_objectId, true);
        GSLog("Catering truck ObjectID " + std::to_string(m_objectId) +
              " could not read its aircraft-door contact offset before timeout; "
              "kept the configured door-point position.");
    } else if (m_stage == Stage::WaitForElevation) {
        // Leave the final target asserted. Opening here prevents a permanently
        // unreadable state variable from leaving an otherwise working truck
        // closed forever.
        SetElevation(services, m_objectId, m_doorSillMeters);
        SetOpening(services, m_objectId, true);
        GSLog("Catering truck ObjectID " + std::to_string(m_objectId) +
              " did not confirm elevation " +
              std::to_string(m_doorSillMeters) + " m before timeout" +
              (m_haveState
                   ? " (current " + std::to_string(m_lastElevationCurrent) +
                         " m, reported target " +
                         std::to_string(m_lastElevationTarget) + " m)"
                   : " (no state readback)") +
              "; left the elevation target active and requested the bridge.");
    } else {
        SetOpening(services, m_objectId, true);
        GSLog("Catering truck ObjectID " + std::to_string(m_objectId) +
              " reached elevation but did not confirm bridge deployment before "
              "timeout" +
              (m_haveState
                   ? " (opening " + std::to_string(m_lastOpeningCurrent) + ")"
                   : "") +
              ".");
    }
    Finish(services, m_pose);
}

void GSCateringDefault::FreezeObject(GSObjectServices &services, AircraftId objectId)
{
    services.simConnect->TransmitEvent(objectId, m_freezeLatitudeLongitudeEvent, 1,
                                       NewCommandRequest());
    services.simConnect->TransmitEvent(objectId, m_freezeAltitudeEvent, 1,
                                       NewCommandRequest());
    services.simConnect->TransmitEvent(objectId, m_freezeAttitudeEvent, 1,
                                       NewCommandRequest());
}

void GSCateringDefault::SetPosition(GSObjectServices &services, AircraftId objectId)
{
    const SIMCONNECT_DATA_INITPOSITION position = ToInitialPosition(m_pose);
    services.simConnect->SetObjectData(m_positionDefinition, objectId, 0,
                                       sizeof(position), &position,
                                       NewCommandRequest());
}

void GSCateringDefault::SetElevation(GSObjectServices &services, AircraftId objectId,
                                     double meters)
{
    services.simConnect->SetObjectData(m_elevationTargetDefinition, objectId, 0,
                                       sizeof(meters), &meters, NewCommandRequest());
}

void GSCateringDefault::SetOpening(GSObjectServices &services, AircraftId objectId,
                                   bool open)
{
    const double value = open ? 1.0 : 0.0;
    services.simConnect->SetObjectData(m_openingTargetDefinition, objectId, 0,
                                       sizeof(value), &value, NewCommandRequest());
}

void GSCateringDefault::SetAircraftDoor(GSObjectServices &services, bool open)
{
    if (!m_door) return;
    services.simConnect->TransmitEventEx1(
        m_aircraft.objectId,
        open ? m_openAircraftDoorsEvent : m_closeAircraftDoorsEvent,
        m_door->interactivePointIndex + 1, 1, NewCommandRequest());
}

void GSCateringDefault::OnRemoved(GSObjectServices &services)
{
    if (!services.simConnect) return;
    SetOpening(services, m_objectId, false);
    SetElevation(services, m_objectId, 0.0);
    SetAircraftDoor(services, false);
}

bool GSCateringDefault::SpecialRequestsFinished() const
{
    return !m_stateRequest || m_stateRequest->IsFinished();
}
} // namespace parking_services
