#include "GSLuggageLoaderFSDT.h"

#include "GSCommon.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace std::chrono_literals;

namespace parking_services
{
GSLuggageLoaderFSDT::GSLuggageLoaderFSDT(std::uint64_t token, AircraftSnapshot aircraft,
                                         GroundServiceObject object,
                                         GroundServiceLocation location)
    : GSObject(token, std::move(aircraft), std::move(object), std::move(location),
               /*parentObjectId*/ 0)
{
}

bool GSLuggageLoaderFSDT::PreparePlacement(GSObjectServices &services)
{
    if (m_location.kind != GroundServiceLocationKind::CargoDoorRightAuto)
    {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": a luggage loader expects a cargo-door placement.");
        return false;
    }
    // Attach to a right cargo door: prefer the back door, fall back to the front.
    const AircraftCargoConnectionPoint *connection =
        m_aircraft.cargoDoorRightBack
            ? &m_aircraft.cargoDoorRightBack.value()
            : (m_aircraft.cargoDoorRightFront ? &m_aircraft.cargoDoorRightFront.value()
                                              : nullptr);
    if (!connection)
    {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": MSFS reported no matching right cargo door.");
        return false;
    }
    m_pose = RelativeToAircraft(m_aircraft, connection->rightMeters,
                                connection->forwardMeters, false);
    m_pose.headingDegrees =
        NormalizeDegrees(m_aircraft.headingDegrees + connection->relativeHeadingDegrees + 180.0);
    m_cargoDoor = {connection->interactivePointIndex,
                   connection->forwardMeters, connection->rightMeters,
                   (m_aircraft.altitudeFeet - m_aircraft.groundAltitudeFeet) * kFeetToMeters +
                       connection->verticalMeters,
                   NormalizeDegrees(connection->relativeHeadingDegrees + 180.0)};
    return true;
}

void GSLuggageLoaderFSDT::Activate(GSObjectServices &services, AircraftId objectId)
{
    if (!m_cargoDoor)
    {
        Finish(services, m_pose);
        return;
    }
    services.openCargoDoor(m_aircraft.objectId, m_cargoDoor->interactivePointIndex);
    services.freezeObject(objectId);
    services.setRampTarget(objectId, 0.0);
    m_stage = Stage::MeasureInitialGeometry;
    m_geometryRequested = false;
    m_geometryRequestDue = std::chrono::steady_clock::now() + 250ms;
}

void GSLuggageLoaderFSDT::Maintain(GSObjectServices &services,
                                   std::chrono::steady_clock::time_point now)
{
    if (m_finalized || m_objectId == 0) return;
    if (m_geometryRequested || now < m_geometryRequestDue) return;
    m_geometryRequested = true;
    services.requestBaggageGeometry(m_objectId);
}

void GSLuggageLoaderFSDT::OnGeometry(GSObjectServices &services,
                                     const BaggageLoaderGeometry &geometry)
{
    m_geometryRequested = false;
    if (!m_cargoDoor)
    {
        Finish(services, m_pose);
        return;
    }

    if (!geometry.succeeded || !std::isfinite(geometry.angleCurrentDegrees) ||
        !std::isfinite(geometry.endRampYMeters) || !std::isfinite(geometry.endRampZMeters) ||
        !std::isfinite(geometry.pivotYMeters) || !std::isfinite(geometry.pivotZMeters))
    {
        GSLog("Could not read cargo-door ramp geometry for ObjectID " +
              std::to_string(m_objectId) +
              "; keeping the configured object at the door point.");
        Finish(services, m_pose);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_stage == Stage::MeasureInitialGeometry)
    {
        const double rampLength =
            std::hypot(geometry.endRampYMeters - geometry.pivotYMeters,
                       geometry.endRampZMeters - geometry.pivotZMeters);
        if (rampLength < 0.01)
        {
            GSLog("MSFS returned no usable cargo-door ramp geometry for ObjectID " +
                  std::to_string(m_objectId) +
                  "; keeping the configured object at the door point.");
            Finish(services, m_pose);
            return;
        }
        const double currentPhase =
            std::atan2(geometry.endRampYMeters - geometry.pivotYMeters,
                       geometry.endRampZMeters - geometry.pivotZMeters);
        const double desiredPhase =
            std::asin(std::clamp((m_cargoDoor->cargoHeightMeters - geometry.pivotYMeters) /
                                     rampLength,
                                 -1.0, 1.0));
        m_rampAngleDegrees =
            std::clamp(geometry.angleCurrentDegrees +
                           (desiredPhase - currentPhase) * kRadiansToDegrees,
                       0.0, 90.0);
        m_stage = Stage::WaitForRampTarget;
        m_geometryRequestDue = now + 500ms;
        services.setRampTarget(m_objectId, m_rampAngleDegrees);
        return;
    }

    if (std::abs(geometry.angleCurrentDegrees - m_rampAngleDegrees) > 0.2)
    {
        m_geometryRequestDue = now + 500ms;
        return;
    }

    const double headingRadians =
        m_cargoDoor->modelRelativeHeadingDegrees * 3.14159265358979323846 / 180.0;
    const double rampDistanceMeters = geometry.endRampZMeters + kCargoDoorClearanceMeters;
    const double forwardMeters =
        m_cargoDoor->cargoForwardMeters - std::cos(headingRadians) * rampDistanceMeters;
    const double rightMeters =
        m_cargoDoor->cargoRightMeters - std::sin(headingRadians) * rampDistanceMeters;
    GSObject::GSObjectPos actualPose =
        RelativeToAircraft(m_aircraft, rightMeters, forwardMeters, false);
    actualPose.headingDegrees = m_pose.headingDegrees;
    services.setPosition(m_objectId, actualPose);
    Finish(services, actualPose);
    GSLog("Aligned configured cargo-door object ObjectID " + std::to_string(m_objectId) +
          " with " + std::to_string(kCargoDoorClearanceMeters) + " m door clearance.");
}

void GSLuggageLoaderFSDT::OnRemoved(GSObjectServices &services)
{
    if (m_cargoDoor)
    {
        services.closeCargoDoor(m_aircraft.objectId, m_cargoDoor->interactivePointIndex);
    }
}
} // namespace parking_services
