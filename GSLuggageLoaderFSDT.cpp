#include "GSLuggageLoaderFSDT.h"

#include "GSCommon.h"
#include "ISimConnectHandler.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace parking_services
{
using namespace std::chrono_literals;

GSLuggageLoaderFSDT::GSLuggageLoaderFSDT(
    AircraftSnapshot aircraft, GroundServiceObject object,
    GroundServiceLocation location)
    : GSObject(std::move(aircraft), std::move(object), std::move(location))
{
}

bool GSLuggageLoaderFSDT::PreparePlacement(GSObjectServices &)
{
    const AircraftCargoConnectionPoint *connection =
        m_aircraft.cargoDoorRightBack
            ? &m_aircraft.cargoDoorRightBack.value()
            : (m_aircraft.cargoDoorRightFront
                   ? &m_aircraft.cargoDoorRightFront.value()
                   : nullptr);
    if (!connection) {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": MSFS reported no matching right cargo door.");
        return false;
    }

    m_pose = RelativeToAircraft(m_aircraft, connection->rightMeters,
                                connection->forwardMeters, false);
    m_pose.headingDegrees = NormalizeDegrees(
        m_aircraft.headingDegrees + connection->relativeHeadingDegrees + 180.0);
    m_cargoDoor = {
        connection->interactivePointIndex,
        connection->forwardMeters,
        connection->rightMeters,
        (m_aircraft.altitudeFeet - m_aircraft.groundAltitudeFeet) *
                kFeetToMeters +
            connection->verticalMeters,
        NormalizeDegrees(connection->relativeHeadingDegrees + 180.0)};
    return true;
}

void GSLuggageLoaderFSDT::ConfigureSimConnect(GSObjectServices &services)
{
    if (m_simConnectConfigured || !services.simConnect) return;
    m_simConnectConfigured = true;

    m_rampTargetDefinition = services.allocateDataDefinition();
    m_geometryDefinition = services.allocateDataDefinition();
    m_positionDefinition = services.allocateDataDefinition();
    m_freezeLatitudeLongitudeEvent = services.allocateClientEvent();
    m_freezeAltitudeEvent = services.allocateClientEvent();
    m_freezeAttitudeEvent = services.allocateClientEvent();
    m_openAircraftDoorsEvent = services.allocateClientEvent();
    m_closeAircraftDoorsEvent = services.allocateClientEvent();

    ISimConnectHandler &simConnect = *services.simConnect;
    simConnect.AddDatum(m_rampTargetDefinition, "BAGGAGELOADER ANGLE TARGET",
                        "degrees", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_geometryDefinition, "BAGGAGELOADER ANGLE CURRENT",
                        "degrees", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_geometryDefinition, "BAGGAGELOADER END RAMP Y",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_geometryDefinition, "BAGGAGELOADER END RAMP Z",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_geometryDefinition, "BAGGAGELOADER PIVOT Y",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_geometryDefinition, "BAGGAGELOADER PIVOT Z",
                        "meters", SIMCONNECT_DATATYPE_FLOAT64,
                        NewCommandRequest());
    simConnect.AddDatum(m_positionDefinition, "Initial Position", "",
                        SIMCONNECT_DATATYPE_INITPOSITION,
                        NewCommandRequest());
    simConnect.MapClientEvent(m_freezeLatitudeLongitudeEvent,
                              "FREEZE_LATITUDE_LONGITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAltitudeEvent, "FREEZE_ALTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_freezeAttitudeEvent, "FREEZE_ATTITUDE_SET",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_openAircraftDoorsEvent, "OPEN_AIRCRAFT_DOORS",
                              NewCommandRequest());
    simConnect.MapClientEvent(m_closeAircraftDoorsEvent, "CLOSE_AIRCRAFT_DOORS",
                              NewCommandRequest());
}

void GSLuggageLoaderFSDT::Activate(GSObjectServices &services,
                                    AircraftId objectId)
{
    if (!m_cargoDoor || !m_simConnectConfigured) {
        Finish(services, m_pose);
        return;
    }
    SetCargoDoor(services, true);
    FreezeObject(services, objectId);
    SetRampTarget(services, objectId, 0.0);
    m_stage = Stage::MeasureInitialGeometry;
    m_geometryRequestDue = std::chrono::steady_clock::now() + 250ms;
}

void GSLuggageLoaderFSDT::Maintain(
    GSObjectServices &services, std::chrono::steady_clock::time_point now)
{
    if (m_finalized || m_objectId == 0) return;

    if (m_geometryRequest) {
        if (!m_geometryRequest->IsFinished()) return;
        const GSBaggageGeometryResult result = m_geometryRequest->Result();
        m_geometryRequest.reset();
        HandleGeometry(services, result);
        return;
    }
    if (now >= m_geometryRequestDue) RequestGeometry(services);
}

void GSLuggageLoaderFSDT::RequestGeometry(GSObjectServices &services)
{
    if (!services.simConnect || m_geometryRequest) return;
    m_geometryRequest = std::make_unique<GSReqBaggageGeometry>();
    services.simConnect->RequestObjectData(
        m_geometryDefinition, m_objectId, SIMCONNECT_PERIOD_ONCE, 0,
        *m_geometryRequest);
}

void GSLuggageLoaderFSDT::HandleGeometry(
    GSObjectServices &services, const GSBaggageGeometryResult &geometry)
{
    if (!m_cargoDoor) {
        Finish(services, m_pose);
        return;
    }
    if (!geometry.succeeded || !std::isfinite(geometry.angleCurrentDegrees) ||
        !std::isfinite(geometry.endRampYMeters) ||
        !std::isfinite(geometry.endRampZMeters) ||
        !std::isfinite(geometry.pivotYMeters) ||
        !std::isfinite(geometry.pivotZMeters)) {
        GSLog("Could not read cargo-door ramp geometry for ObjectID " +
              std::to_string(m_objectId) +
              "; keeping the configured object at the door point.");
        Finish(services, m_pose);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_stage == Stage::MeasureInitialGeometry) {
        const double rampLength =
            std::hypot(geometry.endRampYMeters - geometry.pivotYMeters,
                       geometry.endRampZMeters - geometry.pivotZMeters);
        if (rampLength < 0.01) {
            GSLog("MSFS returned no usable cargo-door ramp geometry for ObjectID " +
                  std::to_string(m_objectId) +
                  "; keeping the configured object at the door point.");
            Finish(services, m_pose);
            return;
        }
        const double currentPhase =
            std::atan2(geometry.endRampYMeters - geometry.pivotYMeters,
                       geometry.endRampZMeters - geometry.pivotZMeters);
        const double desiredPhase = std::asin(std::clamp(
            (m_cargoDoor->heightMeters - geometry.pivotYMeters) / rampLength,
            -1.0, 1.0));
        m_rampAngleDegrees = std::clamp(
            geometry.angleCurrentDegrees +
                (desiredPhase - currentPhase) * kRadiansToDegrees,
            0.0, 90.0);
        m_stage = Stage::WaitForRampTarget;
        m_geometryRequestDue = now + 500ms;
        SetRampTarget(services, m_objectId, m_rampAngleDegrees);
        return;
    }

    if (std::abs(geometry.angleCurrentDegrees - m_rampAngleDegrees) > 0.2) {
        m_geometryRequestDue = now + 500ms;
        return;
    }

    const double headingRadians =
        m_cargoDoor->modelRelativeHeadingDegrees / kRadiansToDegrees;
    const double rampDistanceMeters =
        geometry.endRampZMeters + CargoDoorClearanceMeters;
    const double forwardMeters =
        m_cargoDoor->forwardMeters -
        std::cos(headingRadians) * rampDistanceMeters;
    const double rightMeters =
        m_cargoDoor->rightMeters -
        std::sin(headingRadians) * rampDistanceMeters;
    GSObjectPos actualPose =
        RelativeToAircraft(m_aircraft, rightMeters, forwardMeters, false);
    actualPose.headingDegrees = m_pose.headingDegrees;
    SetPosition(services, m_objectId, actualPose);
    Finish(services, actualPose);
    GSLog("Aligned configured cargo-door object ObjectID " +
          std::to_string(m_objectId) + " with " +
          std::to_string(CargoDoorClearanceMeters) +
          " m door clearance.");
}

void GSLuggageLoaderFSDT::FreezeObject(GSObjectServices &services,
                                        AircraftId objectId)
{
    if (!services.simConnect) return;
    services.simConnect->TransmitEvent(objectId, m_freezeLatitudeLongitudeEvent,
                                       1, NewCommandRequest());
    services.simConnect->TransmitEvent(objectId, m_freezeAltitudeEvent, 1,
                                       NewCommandRequest());
    services.simConnect->TransmitEvent(objectId, m_freezeAttitudeEvent, 1,
                                       NewCommandRequest());
}

void GSLuggageLoaderFSDT::SetPosition(GSObjectServices &services,
                                       AircraftId objectId,
                                       const GSObjectPos &pose)
{
    if (!services.simConnect) return;
    SIMCONNECT_DATA_INITPOSITION position = ToInitialPosition(pose);
    position.OnGround = 1;
    services.simConnect->SetObjectData(
        m_positionDefinition, objectId, 0, sizeof(position), &position,
        NewCommandRequest());
}

void GSLuggageLoaderFSDT::SetRampTarget(GSObjectServices &services,
                                         AircraftId objectId,
                                         double angleDegrees)
{
    if (!services.simConnect) return;
    const RampTargetWireData data{angleDegrees};
    services.simConnect->SetObjectData(
        m_rampTargetDefinition, objectId, 0, sizeof(data), &data,
        NewCommandRequest());
}

void GSLuggageLoaderFSDT::SetCargoDoor(GSObjectServices &services, bool open)
{
    if (!services.simConnect || !m_cargoDoor) return;
    services.simConnect->TransmitEventEx1(
        m_aircraft.objectId,
        open ? m_openAircraftDoorsEvent : m_closeAircraftDoorsEvent,
        m_cargoDoor->interactivePointIndex + 1, 1, NewCommandRequest());
}

void GSLuggageLoaderFSDT::OnRemoved(GSObjectServices &services)
{
    SetCargoDoor(services, false);
}

bool GSLuggageLoaderFSDT::SpecialRequestsFinished() const
{
    return !m_geometryRequest || m_geometryRequest->IsFinished();
}
} // namespace parking_services
