#include "GSAnimationObject.h"
#include "../General/GSLogStream.h"
#include "../General/GSGeography.h"

namespace NS_GSLiveAirportMSFS
{

constexpr std::array GSDatums_Position{
    GSDefinitions::DatumSpec{"Initial Position", nullptr, SIMCONNECT_DATATYPE_INITPOSITION},
};

void GSAnimationObject::InitDatums(GSSimConnect& handler)
{
    handler.InvokeAddDatums(GSDatums_Position, GSDefinitions::GSDefID::GSDefID_Position);
}

void GSAnimationObject::CalcNextFrame(GSAnimationObject::FrameRange& frange, float elapsedMilli) 
{
    const float delta = frange.m_fps * elapsedMilli / 1000.0f;
    if (frange.m_endFrame > frange.m_startFrame) {
        frange.m_currFrame = frange.m_startFrame + std::fmod(frange.m_currFrame - frange.m_startFrame + delta, frange.m_span);
    } else {
        frange.m_currFrame = frange.m_startFrame - std::fmod(frange.m_startFrame - frange.m_currFrame + delta, frange.m_span);
    }
}

void GSAnimationObject::SetAIWaypoints(const std::vector<SIMCONNECT_DATA_WAYPOINT>* aiWaypoints)
{
    m_aiWaypoints = aiWaypoints;
    if (m_aiWaypoints && m_aiWaypoints->size() > 1) {
        m_currLocation = { (*m_aiWaypoints)[0].Longitude, (*m_aiWaypoints)[0].Latitude };
    } else {
        m_aiWaypoints = nullptr;
    }
}

void GSAnimationObject::Move(GSSimConnect& handler, float elapsedMilli)
{
    if (!m_aiWaypoints)
        return;

    const auto& waypoints = *m_aiWaypoints;
    if (m_tgtWaypoint >= waypoints.size())
        m_tgtWaypoint = 0;

    const SIMCONNECT_DATA_WAYPOINT& waypoint = waypoints[m_tgtWaypoint];
    const GSCoord target{waypoint.Longitude, waypoint.Latitude};
    const double tgtDist = GSGeography::DistanceMeters(m_currLocation, target);
    const double headingToTarget = GSGeography::Azz(m_currLocation, target);
    const double moveMeters = waypoint.ktsSpeed * GSGeography::KnotsToMetersSecond * (static_cast<double>(elapsedMilli) / 1000.0);

    if (tgtDist <= ArrivalRadiusMeters || moveMeters >= tgtDist) {
        m_currLocation = target;
        m_tgtWaypoint = (m_tgtWaypoint + 1) % waypoints.size();
    } else {
        m_currLocation = GSGeography::RelativePosition(headingToTarget, m_currLocation, moveMeters, 0.0);
    }

    SIMCONNECT_DATA_INITPOSITION position { 
        m_currLocation.Lat(), m_currLocation.Long(), waypoint.Altitude, 0, 0,
        GSGeography::NormDeg(headingToTarget + ((waypoint.Flags & SIMCONNECT_WAYPOINT_REVERSE) ? 180.0 : 0.0)),
        (waypoint.Flags & SIMCONNECT_WAYPOINT_ON_GROUND) ? 1U : 0U, 0
    };

    const auto rcSim = handler.Invoke(
        SimConnect_SetDataOnSimObject, GSDefinitions::GSDefID_Position,
        m_simObjectID, 0, 1, static_cast<DWORD>(sizeof(position)), &position);

    if (!rcSim.isOK()) {
        GSLogStream::LogError("GSAnimationObject::Move failed to update object position: ") << rcSim.rc;
    }
}
}
