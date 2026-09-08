#include "GSWalkerFSDT.h"

#include "GSCommon.h"

#include <utility>

namespace parking_services
{
GSWalkerFSDT::GSWalkerFSDT(AircraftSnapshot aircraft, GroundServiceObject object,
                           GroundServiceLocation location)
    : GSObject(std::move(aircraft), std::move(object), std::move(location))
{
}

bool GSWalkerFSDT::PreparePlacement(GSObjectServices &)
{
    if (m_location.kind != GroundServiceLocationKind::Route) {
        GSLog("Skipped " + m_object.family + " for aircraft " +
              std::to_string(m_aircraft.objectId) +
              ": an FSDT walker requires a route location.");
        return false;
    }
    if (!LocationRelationAvailable()) return false;

    m_pose = RelativeToAircraft(m_aircraft, m_location.relX1,
                                m_location.relY1, false, false,
                                m_location.wingRelative,
                                m_location.relation);
    const GSObjectPos endpoint =
        RelativeToAircraft(m_aircraft, m_location.relX2,
                           m_location.relY2, false, false,
                           m_location.wingRelative,
                           m_location.relation);
    m_movementCoordinates = {
        {m_pose.latitude, m_pose.longitude, m_pose.altitudeFeet,
         m_pose.headingDegrees},
        {endpoint.latitude, endpoint.longitude, endpoint.altitudeFeet,
         endpoint.headingDegrees}};
    m_movementSpeedMetersPerSecond = DefaultMovementSpeedMetersPerSecond;
    return true;
}
} // namespace parking_services
